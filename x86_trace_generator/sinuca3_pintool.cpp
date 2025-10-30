//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file sinuca3_pintool.cpp
 * @brief Implementation of the SiNUCA3 x86_64 tracer based on Intel Pin.
 * @details To enable instrumentation, wrap the target code with
 * BeginInstrumentationBlock() and EndInstrumentationBlock(). Instrumentation
 * code is only inserted within these blocks, and analysis is only executed if
 * the thread has called EnableThreadInstrumentation().
 *
 * Example command:
 * ./pin/pin -t ./obj-intel64/my_pintool.so -o ./my_trace -- ./my_program
 *
 * A knob is
 */

#include <sinuca3.hpp>

#include "pin.H"
#include "tracer/sinuca/file_handler.hpp"
#include "utils/dynamic_trace_writer.hpp"
#include "utils/memory_trace_writer.hpp"
#include "utils/static_trace_writer.hpp"

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
}

/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief When enabled, this flag allows the pintool to record all instructions
 * static info into a static trace file, and allows the instrumentation phase
 * (e.g., in OnTrace) to insert analysis code into the target program. However,
 * the inserted analysis code will only execute at runtime if the corresponding
 * thread has its threadInstrumentationEnabled flag set to true.
 */
bool isInstrumentating;

struct ThreadData {
    DynamicTraceWriter dynamicTrace;
    MemoryTraceWriter memoryTrace;
    /**
     * @note Instrumentation is the process of deciding where and what code
     * should be inserted into the target program, while analysis refers to the
     * code that is actually executed at those insertion points to gather
     * information about the program’s behavior.
     *
     * @details Indicates, for each thread, whether it is allowed to execute
     * previously inserted analysis code.
     *
     * This flag does not control the instrumentation process itself (i.e.,
     * whether code is inserted into the target program), but rather whether a
     * specific thread is permitted to execute that analysis code at runtime.
     *
     * The insertion of analysis code occurs only when the global
     * `isInstrumentating` flag is enabled. Later, during program execution, the
     * inserted code will only be executed by a thread if its corresponding
     * entry in this vector is set to true.
     *
     * When executed, the analysis code records dynamic and memory trace
     * information into files associated with the executing thread.
     */
    bool isThreadAnalysisEnabled;
    /** @brief Whenever a thread is destroyed, this struct is not deleted,
     * instead it is set as not active because it may later be reused in
     * another parallel block. */
    bool isThreadActive;
};

std::vector<ThreadData*> threadDataVec;
StaticTraceWriter* staticTrace;
/** @brief Used to block spinlock routines from being instrumented. */
std::vector<std::string> rtnsWithPauseInst;

/** @brief  */
PIN_LOCK threadAnalysisLock;

const char* traceDir = NULL;
const char* imageName = NULL;
bool wasInitInstrumentationCalled = false;

/** @brief Set directory to save trace with '-o'. Default is current dir. */
KNOB<std::string> knobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Path to store the trace files.");
/** @brief Allows one to force full instrumentation with '-f'. Default is 0. */
KNOB<BOOL> knobForceInstrumentation(
    KNOB_MODE_WRITEONCE, "pintool", "f", "0",
    "Force instrumentation for the entire execution for all created threads.");

int Usage() {
    SINUCA3_LOG_PRINTF(
        "Example command: "
        "\t./pin/pin -t ./obj-intel64/my_pintool.so -o ./my_trace -- "
        "./my_program\n"
        "------------------------------------------------------------"
        "-f: force instrumentation even when no blocks are defined.\n"
        "-o: output directory.\n");

    return 1;
}

/** @brief Enables instrumentation. */
VOID InitInstrumentation() {
    if (isInstrumentating) return;
    SINUCA3_DEBUG_PRINTF("-----------------------------------\n");
    SINUCA3_DEBUG_PRINTF("Start of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("-----------------------------------\n");
    wasInitInstrumentationCalled = true;
    isInstrumentating = true;
}

/** @brief Disables instrumentation. */
VOID StopInstrumentation() {
    if (!isInstrumentating || knobForceInstrumentation.Value()) return;
    SINUCA3_DEBUG_PRINTF("---------------------------------\n");
    SINUCA3_DEBUG_PRINTF("End of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("---------------------------------\n");
    isInstrumentating = false;
}

bool WasThreadCreated(THREADID tid) {
    bool wasCreated = (threadDataVec.size() - tid > 0);

    if (!wasCreated) {
        SINUCA3_DEBUG_PRINTF("[WasThreadCreated] Thread [%d] not created!\n",
                             tid);
    }

    return wasCreated;
}

inline int IsThreadAnalysisActive(THREADID tid) {
    return (WasThreadCreated(tid) && threadDataVec[tid]->isThreadAnalysisEnabled);
}

/** @brief Enables execution of analysis code. */
VOID EnableInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF(
        "[EnableInstrumentationInThread] Thread [%d] analysis enabled\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = true;
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Disables execution of analysis code. */
VOID DisableInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid) || knobForceInstrumentation.Value()) {
        return;
    }

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF(
        "[DisableInstrumentationInThread] Thread [%d] analysis disabled\n",
        tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = false;
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    SINUCA3_DEBUG_PRINTF("[OnThreadStart] thread id [%d]\n", tid);

    PIN_GetLock(&threadAnalysisLock, tid);

    struct ThreadData* threadData = new ThreadData;
    if (!threadData) {
        SINUCA3_ERROR_PRINTF("[OnThreadStart] Failed to alloc thread data.\n");
        return;
    }

    /* create tracer files */
    if (threadData->dynamicTrace.OpenFile(traceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF(
            "[OnThreadStart] Failed to open dynamic trace file\n");
    }
    if (threadData->memoryTrace.OpenFile(traceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF(
            "[OnThreadStart] Failed to open memory trace file\n");
    }

    threadData->isThreadAnalysisEnabled = false;
    threadData->isThreadActive = (tid == 0);

    threadDataVec.push_back(threadData);
    staticTrace->IncThreadCount();

    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    if (!WasThreadCreated(tid)) return;
    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnThreadFini] thread id [%d]\n", tid);
    delete threadDataVec[tid];
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bblId, UINT32 numInst) {
    if (!IsThreadAnalysisActive(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);

    if (threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId)) {
        SINUCA3_ERROR_PRINTF(
            "[AppendToDynamicTrace] Failed to add basic block id to file\n");
    }

    /* detect thread being reused */
    if (!threadDataVec[tid]->isThreadActive) {
        threadDataVec[0]->dynamicTrace.AddThreadCreateEvent(tid);
        threadDataVec[tid]->isThreadActive = true;
    }
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Append non standard memory op to memory trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    if (!IsThreadAnalysisActive(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);

    /* comment */
    int totalOps = accessInfo->numberOfMemops;
    if (threadDataVec[tid]->memoryTrace.AddNumberOfMemOperations(totalOps)) {
        SINUCA3_ERROR_PRINTF(
            "[AppendToMemTrace] Failed to add number of mem ops to file\n");
    }

    for (int i = 0; i < totalOps; ++i) {
        if (!accessInfo->memop[i].maskOn) continue;

        unsigned char opType;
        if (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD) {
            opType = MemoryOperationLoad;
        } else if (accessInfo->memop[i].memopType == PIN_MEMOP_STORE) {
            opType = MemoryOperationStore;
        } else {
            SINUCA3_ERROR_PRINTF("[AppendToMemTrace] Invalid operation!\n");
            return;
        }

        int failed = threadDataVec[tid]->memoryTrace.AddMemoryOperation(
            accessInfo->memop[i].memoryAddress,
            accessInfo->memop[i].bytesAccessed, opType);

        if (failed) {
            SINUCA3_ERROR_PRINTF(
                "[AppendToMemTrace] Failed to add memory operation!\n");
        }
    }
    PIN_ReleaseLock(&threadAnalysisLock);
}

/**
 * @brief
 * @note pin already holds VM Lock before calling any instrumentation routine.
 */
VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn)) {
        SINUCA3_ERROR_PRINTF("[OnTrace] Found invalid routine! Skipping...\n");
        return;
    }

    RTN_Open(rtn);
    std::string rtnName = RTN_Name(rtn);
    RTN_Close(rtn);

    /*
     * Every routine that performs spinlock has a 'pause' asm instruction;
     * since the thread syncronization is simulated by the trace reader,
     * there is no sense in adding code executed during busy wait;
     * hence the routines that have this instruction are not instrumented.
     */
    for (std::string& str : rtnsWithPauseInst) {
        if (rtnName == str) {
            SINUCA3_DEBUG_PRINTF("[OnTrace] Thread id [%d]: Ignoring [%s]!\n",
                                 PIN_ThreadId(), str.c_str());
            return;
        }
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        unsigned int numberInstInBasicBlock = BBL_NumIns(bbl);
        /*
         * The number of basic blocks found serves as an index as well used in
         * the dynamic trace.
         */
        unsigned int basicBlockIndex = staticTrace->GetBasicBlockCount();
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, basicBlockIndex,
                       IARG_UINT32, numberInstInBasicBlock, IARG_END);
        /*
         * The trace reader needs to know where the block begins and
         * ends to create the basic block dictionary.
         */
        if (staticTrace->AddBasicBlockSize(numberInstInBasicBlock)) {
            SINUCA3_ERROR_PRINTF("[OnTrace] Failed to add basic block count to file\n");
        }

        staticTrace->IncBasicBlockCount();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (staticTrace->AddInstruction(&ins)) {
                SINUCA3_ERROR_PRINTF("[OnTrace] Failed to add instruction to file\n");
            }
            /*
             * The number of static instructions will later be useful while
             * reading the trace and instantiating the basic block dictionary.
             */
            staticTrace->IncStaticInstructionCount();

            if (!INS_IsMemoryRead(ins) && !INS_IsMemoryWrite(ins)) continue;
            /*
             * Add call to AppendToMemTrace on every instruction that performs
             * one or more memory accesses.
             */
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                           IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                           IARG_END);
        }
    }
}

VOID OnThreadCreationEvent(THREADID tid, UINT32 eventType) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnThreadCreationEvent] Thread id [%d]\n", tid);

    switch (eventType) {
        case ThreadEventCreateThread:
            if (tid != 0) {
                SINUCA3_DEBUG_PRINTF(
                    "[OnThreadCreationEvent] Thread id [%d] is not zero! There "
                    "is no support for nested parallel block!\n",
                    tid);
            }
            break;
        case ThreadEventDestroyThread:
            /*
             * If the thread is destroyed threads are reused, the call
             * 'AppendToDynamicTrace' detects the reuse by checking if it is
             * active.
             */
            for (unsigned int it = 1; it < threadDataVec.size(); ++it) {
                threadDataVec[it]->isThreadActive = false;
                threadDataVec[it]->dynamicTrace.AddThreadHaltEvent();
            }
            /* thread 0 destroys created threads */
            threadDataVec[tid]->dynamicTrace.AddThreadDestroyEvent();
            break;
        default:
            SINUCA3_DEBUG_PRINTF("[OnThreadCreationEvent] unkown type!\n");
            break;
    }
    PIN_ReleaseLock(&threadAnalysisLock);
}

VOID OnGlobalLockThreadEvent(THREADID tid, BOOL isLock) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnGlobalLockThreadEvent] Thread id [%d]\n", tid);
    if (isLock) {
        threadDataVec[tid]->dynamicTrace.AddLockEventGlobalLock();
    } else {
        threadDataVec[tid]->dynamicTrace.AddUnlockEventGlobalLock();
    }
    PIN_ReleaseLock(&threadAnalysisLock);
}

VOID OnPrivateLockThreadEvent(THREADID tid, CONTEXT* ctxt, BOOL isLock,
                              BOOL isNested, BOOL isTest) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnPrivateLockThreadEvent] Thread id [%d]\n", tid);

    ADDRINT lockAddr = PIN_GetContextReg(ctxt, REG_RDI);
    SINUCA3_DEBUG_PRINTF("\tLock Address is %p!\n", (void*)lockAddr);

    if (isLock) {
        threadDataVec[tid]->dynamicTrace.AddLockEventPrivateLock(
            lockAddr, isNested, isTest);
    } else {
        threadDataVec[tid]->dynamicTrace.AddUnlockEventPrivateLock(lockAddr,
                                                                   isNested);
    }
    PIN_ReleaseLock(&threadAnalysisLock);
}

VOID OnBarrierThreadEvent(THREADID tid) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnBarrierThreadEvent] Thread id [%d]:\n", tid);
    threadDataVec[tid]->dynamicTrace.AddBarrierEvent();
    PIN_ReleaseLock(&threadAnalysisLock);
}

INS FindInstInRtn(RTN rtn, std::string instName) {
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        if (INS_Mnemonic(ins) == instName) {
            return ins;
        }
    }

    return INS_Invalid();
}

/** @brief */
VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;

    struct RtnEvent {
        std::string name;
        unsigned char type;
    };
    struct LockRtnEvent {
        RtnEvent event;
        bool isTestLock;
        bool isNestedLock;
    };

    SINUCA3_DEBUG_PRINTF("[OnImageLoad] Thread id [0]\n");

    static RtnEvent threadCreationRtns[] = {
        {"gomp_team_start", ThreadEventCreateThread},
        {"gomp_team_end", ThreadEventDestroyThread}};
    static RtnEvent globalLockRtns[] = {
        {"GOMP_critical_start", ThreadEventLockRequest},
        {"GOMP_critical_end", ThreadEventUnlockRequest}};
    static LockRtnEvent privateLockRtns[] = {
        {{"GOMP_critical_name_start", ThreadEventLockRequest}, false, false},
        {{"GOMP_critical_name_end", ThreadEventUnlockRequest}, false, false},
        {{"omp_set_lock", ThreadEventLockRequest}, false, false},
        {{"omp_set_nest_lock", ThreadEventLockRequest}, false, true},
        {{"omp_test_lock", ThreadEventLockRequest}, true, false},
        {{"omp_test_nest_lock", ThreadEventLockRequest}, true, true},
        {{"omp_unset_lock", ThreadEventUnlockRequest}, false, false},
        {{"omp_unset_nest_lock", ThreadEventUnlockRequest}, false, true}};
    static RtnEvent barrierRtns[] = {{"GOMP_barrier", ThreadEventBarrier}};

    std::string absoluteImgPath = IMG_Name(img);
    long size = absoluteImgPath.length() + sizeof('\0');
    char* name = (char*)malloc(size);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string sub = absoluteImgPath.substr(idx);
    strncpy(name, sub.c_str(), size);
    imageName = name;

    SINUCA3_DEBUG_PRINTF("[OnImageLoad] Image name is [%s]\n", imageName);

    staticTrace = new StaticTraceWriter();
    if (staticTrace == NULL) {
        SINUCA3_DEBUG_PRINTF(
            "[OnImageLoad] Failed to create static trace file.\n");
        return;
    }
    if (staticTrace->OpenFile(traceDir, imageName)) {
        SINUCA3_DEBUG_PRINTF(
            "[OnImageLoad] Failed to open static trace file\n");
        return;
    }

    unsigned long iter;
    unsigned long iterMax;
    bool rtnWasTreated;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            std::string rtnName = RTN_Name(rtn);
            rtnWasTreated = false;

            /* pause instruction is spin lock hint */
            if (FindInstInRtn(rtn, "PAUSE") != INS_Invalid()) {
                rtnsWithPauseInst.push_back(rtnName);
            }

            if (rtnName == "BeginInstrumentationBlock") {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
                rtnWasTreated = true;
            }
            if (rtnName == "EndInstrumentationBlock") {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
                rtnWasTreated = true;
            }
            if (rtnName == "EnableThreadInstrumentation") {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)EnableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
                rtnWasTreated = true;
            }
            if (rtnName == "DisableThreadInstrumentation") {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)DisableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
                rtnWasTreated = true;
            }

            iterMax = sizeof(globalLockRtns) / sizeof(RtnEvent);
            for (iter = 0; iter < iterMax; ++iter) {
                if (rtnName == globalLockRtns[iter].name) {
                    UINT32 isLock =
                        (globalLockRtns[iter].type == ThreadEventLockRequest);
                    RTN_InsertCall(
                        rtn, IPOINT_BEFORE, (AFUNPTR)OnGlobalLockThreadEvent,
                        IARG_THREAD_ID, IARG_UINT32, isLock, IARG_END);
                    rtnWasTreated = true;
                }
            }
            iterMax = sizeof(privateLockRtns) / sizeof(LockRtnEvent);
            for (iter = 0; iter < iterMax; ++iter) {
                if (rtnName == privateLockRtns[iter].event.name) {
                    INS inst;
                    if (privateLockRtns[iter].event.type ==
                        ThreadEventLockRequest) {
                        inst = FindInstInRtn(rtn, "CMPXCHG_LOCK");
                        INS_InsertCall(
                            inst, IPOINT_BEFORE,
                            (AFUNPTR)OnPrivateLockThreadEvent, IARG_THREAD_ID,
                            IARG_CONTEXT, IARG_BOOL, true, IARG_BOOL,
                            privateLockRtns[iter].isNestedLock, IARG_BOOL,
                            privateLockRtns[iter].isTestLock, IARG_END);
                    } else {
                        inst = FindInstInRtn(rtn, "XCHG");
                        INS_InsertCall(
                            inst, IPOINT_BEFORE,
                            (AFUNPTR)OnPrivateLockThreadEvent, IARG_THREAD_ID,
                            IARG_CONTEXT, IARG_BOOL, false, IARG_BOOL,
                            privateLockRtns[iter].isNestedLock, IARG_BOOL,
                            privateLockRtns[iter].isTestLock, IARG_END);
                    }
                    rtnWasTreated = true;
                }
            }
            iterMax = sizeof(barrierRtns) / sizeof(RtnEvent);
            for (iter = 0; iter < iterMax; ++iter) {
                if (rtnName == barrierRtns[iter].name) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)OnBarrierThreadEvent,
                                   IARG_THREAD_ID, IARG_END);
                    rtnWasTreated = true;
                }
            }
            iterMax = sizeof(threadCreationRtns) / sizeof(RtnEvent);
            for (iter = 0; iter < iterMax; ++iter) {
                if (rtnName == threadCreationRtns[iter].name) {
                    RTN_InsertCall(rtn, IPOINT_AFTER,
                                   (AFUNPTR)OnThreadCreationEvent,
                                   IARG_THREAD_ID, IARG_UINT32,
                                   threadCreationRtns[iter].type, IARG_END);
                    rtnWasTreated = true;
                }
            }

            if (!rtnWasTreated) {
                /*     if (rtnName.find("gomp") != std::string::npos ||
                 *         rtnName.find("GOMP") != std::string::npos ||
                 *         rtnName.find("omp") != std::string::npos) {
                 *         SINUCA3_DEBUG_PRINTF("Routine [%s] was not
                 * treated\n", rtnName.c_str());
                 *    }
                 */
            }

            RTN_Close(rtn);
        }
    }
}

/** @brief */
VOID OnFini(INT32 code, VOID* ptr) {
    SINUCA3_DEBUG_PRINTF("[OnFini]: End of tool execution!\n");

    if (imageName) {
        free((void*)imageName);
    }
    if (staticTrace) {
        delete staticTrace;
    }
    if (!wasInitInstrumentationCalled) {
        SINUCA3_DEBUG_PRINTF(
            "[OnFini]: No instrumentation blocks were found in the target "
            "program!\n\n");
    }
}

/** @brief */
int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    traceDir = knobFolder.Value().c_str();
    if (access(traceDir, F_OK) != 0) {
        mkdir(traceDir, S_IRWXU | S_IRWXG | S_IROTH);
    }

    PIN_InitLock(&threadAnalysisLock);

    if (knobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("[main]: Instrumenting entire program\n");
        InitInstrumentation();
    } else {
        isInstrumentating = false;
    }

    IMG_AddInstrumentFunction(OnImageLoad, NULL);
    TRACE_AddInstrumentFunction(OnTrace, NULL);
    PIN_AddFiniFunction(OnFini, NULL);

    PIN_AddThreadStartFunction(OnThreadStart, NULL);
    PIN_AddThreadFiniFunction(OnThreadFini, NULL);

    PIN_StartProgram();

    return 0;
}