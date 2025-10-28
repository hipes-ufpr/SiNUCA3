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

/** @brief Used to block two threads from trying to print simultaneously. */
PIN_LOCK debugPrintLock;
/** @brief OnThreadStart writes in global structures. */
PIN_LOCK threadStartLock;
/** @brief Used to protect parent thread stack for example. */
PIN_LOCK getThreadDataLock;

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

#define PINTOOL_DEBUG_PRINTF(...)                     \
    {                                                 \
        PIN_GetLock(&debugPrintLock, PIN_ThreadId()); \
        printf("[DEBUG] ");                           \
        printf(__VA_ARGS__);                          \
        PIN_ReleaseLock(&debugPrintLock);             \
    }

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
    PINTOOL_DEBUG_PRINTF("-----------------------------------\n");
    PINTOOL_DEBUG_PRINTF("Start of tool instrumentation block\n");
    PINTOOL_DEBUG_PRINTF("-----------------------------------\n");
    wasInitInstrumentationCalled = true;
    isInstrumentating = true;
}

/** @brief Disables instrumentation. */
VOID StopInstrumentation() {
    if (!isInstrumentating || knobForceInstrumentation.Value()) return;
    PINTOOL_DEBUG_PRINTF("---------------------------------\n");
    PINTOOL_DEBUG_PRINTF("End of tool instrumentation block\n");
    PINTOOL_DEBUG_PRINTF("---------------------------------\n");
    isInstrumentating = false;
}

inline int WasThreadCreated(THREADID tid) {
    return (threadDataVec.size() - tid > 0);
}

int IsThreadAnalysisActive(THREADID tid) {
    if (!WasThreadCreated(tid)) {
        PINTOOL_DEBUG_PRINTF("[IsThreadAnalysisActive] thread not created!\n");
        return 0;
    }
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) {
        return 0;
    }

    return 1;
}

/** @brief Enables execution of analysis code. */
VOID EnableInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid)) return;
    PINTOOL_DEBUG_PRINTF(
        "[EnableInstrumentationInThread]: Thread [%d] analysis enabled\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = true;
}

/** @brief Disables execution of analysis code. */
VOID DisableInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid) || knobForceInstrumentation.Value()) {
        return;
    }
    PINTOOL_DEBUG_PRINTF(
        "[DisableInstrumentationInThread]: Thread [%d] analysis disabled\n",
        tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = false;
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&threadStartLock, tid);

    /* check if tid is sequential */
    if (tid != threadDataVec.size()) {
        PINTOOL_DEBUG_PRINTF("Thread data not created. Tid isnt sequential.\n");
        return;
    }

    struct ThreadData* threadData = new ThreadData;
    if (!threadData) {
        PINTOOL_DEBUG_PRINTF("Failed to alloc thread data.\n");
        return;
    }

    /* create tracer files */
    if (threadData->dynamicTrace.OpenFile(traceDir, imageName, tid)) {
        PINTOOL_DEBUG_PRINTF("Failed to open dynamic trace file\n");
        return;
    }
    if (threadData->memoryTrace.OpenFile(traceDir, imageName, tid)) {
        PINTOOL_DEBUG_PRINTF("Failed to open memory trace file\n");
        return;
    }

    threadData->isThreadAnalysisEnabled = false;
    threadData->isThreadActive = (tid == 0);

    threadDataVec.push_back(threadData);
    staticTrace->IncThreadCount();

    PIN_ReleaseLock(&threadStartLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    if (!WasThreadCreated(tid)) return;
    delete threadDataVec[tid];
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bblId, UINT32 numInst) {
    if (!IsThreadAnalysisActive(tid)) return;

    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);

    if (threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId)) {
        PINTOOL_DEBUG_PRINTF("Failed to add basic block id to file\n");
    }

    /* detect thread being reused */
    if (!threadDataVec[tid]->isThreadActive) {
        threadDataVec[0]->dynamicTrace.AddThreadCreateEvent(tid);
        threadDataVec[tid]->isThreadActive = true;
        PINTOOL_DEBUG_PRINTF(
            "[AppendToDynamicTrace] Thread id [%d]: Thread created or reused; "
            "parent is [0]\n",
            tid);
    }
}

/** @brief Append non standard memory op to memory trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    if (!IsThreadAnalysisActive(tid)) return;

    /* comment */
    int totalOps = accessInfo->numberOfMemops;
    if (threadDataVec[tid]->memoryTrace.AddNumberOfMemOperations(totalOps)) {
        PINTOOL_DEBUG_PRINTF("Failed to add number of mem ops to file\n");
    }

    for (int i = 0; i < totalOps; ++i) {
        if (!accessInfo->memop[i].maskOn) continue;

        int failed;
        if (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD) {
            failed = threadDataVec[tid]->memoryTrace.AddMemoryOperation(
                accessInfo->memop[i].memoryAddress,
                accessInfo->memop[i].bytesAccessed, MemoryOperationLoad);
        } else {
            failed = threadDataVec[tid]->memoryTrace.AddMemoryOperation(
                accessInfo->memop[i].memoryAddress,
                accessInfo->memop[i].bytesAccessed, MemoryOperationStore);
        }
        if (failed) {
            PINTOOL_DEBUG_PRINTF("Failed to add mem operation to file\n");
        }
    }
}

/**
 * @brief
 * @note pin already holds VM Lock before calling any instrumentation routine.
 */
VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn)) {
        PINTOOL_DEBUG_PRINTF("Found invalid routine! Skipping...\n");
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
            PINTOOL_DEBUG_PRINTF("[OnTrace] Thread id [%d]: Ignoring [%s]!\n",
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
            PINTOOL_DEBUG_PRINTF("Failed to add basic block count to file\n");
        }

        staticTrace->IncBasicBlockCount();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (staticTrace->AddInstruction(&ins)) {
                PINTOOL_DEBUG_PRINTF("Failed to add instruction to file\n");
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

    PINTOOL_DEBUG_PRINTF(
        "[OnThreadCreationEvent] Thread id [%d]: event is [%d]\n", tid,
        eventType);

    switch (eventType) {
        case ThreadEventCreateThread:
            if (tid != 0) {
                PINTOOL_DEBUG_PRINTF(
                    "[OnThreadCreationEvent] Thread id is not zero! Beware "
                    "that there is no support for nested parallel block!\n");
            }
            break;
        case ThreadEventDestroyThread:
            /*
             * All threads with tid value greater than parent's tid will be
             * destroyed, so the loop sets their 'isThreadActive' attribute to
             * false. If the thread is reused, the call 'AppendToDynamicTrace'
             * detects the reuse by checking the same attribute.
             */
            for (unsigned int it = 0; it < threadDataVec.size(); ++it) {
                threadDataVec[it]->isThreadActive = false;
            }

            threadDataVec[tid]->dynamicTrace.AddThreadDestroyEvent();
            break;
        default:
            PINTOOL_DEBUG_PRINTF("[OnThreadCreationEvent] unkown type!\n");
            break;
    }
}

VOID OnGlobalLockThreadEvent(THREADID tid, BOOL isLock) {
    if (!WasThreadCreated(tid)) return;

    if (isLock) {
        threadDataVec[tid]->dynamicTrace.AddLockEventGlobalLock();
        PINTOOL_DEBUG_PRINTF(
            "[OnGlobalLockThreadEvent] Thread id [%d]: Thread event global "
            "lock!\n",
            tid);
    } else {
        threadDataVec[tid]->dynamicTrace.AddUnlockEventGlobalLock();
        PINTOOL_DEBUG_PRINTF(
            "[OnGlobalLockThreadEvent] Thread id [%d]: Thread event global "
            "unlock!\n",
            tid);
    }
}

VOID OnPrivateLockThreadEvent(THREADID tid, CONTEXT* ctxt, BOOL isLock,
                              BOOL isNested, BOOL isTest) {
    if (!WasThreadCreated(tid)) return;

    PINTOOL_DEBUG_PRINTF("[OnPrivateLockThreadEvent] Thread id [%d]:\n", tid);

    ADDRINT lockAddr = PIN_GetContextReg(ctxt, REG_RDI);
    PINTOOL_DEBUG_PRINTF("\tLock address is %p!\n", (void*)lockAddr);

    if (isLock) {
        threadDataVec[tid]->dynamicTrace.AddLockEventPrivateLock(
            lockAddr, isNested, isTest);
        PINTOOL_DEBUG_PRINTF("\tThread event private lock!\n");
    } else {
        threadDataVec[tid]->dynamicTrace.AddUnlockEventPrivateLock(lockAddr,
                                                                   isNested);
        PINTOOL_DEBUG_PRINTF("\tThread event private unlock!\n");
    }
}

VOID OnBarrierThreadEvent(THREADID tid) {
    if (!WasThreadCreated(tid)) return;

    PINTOOL_DEBUG_PRINTF("[OnBarrierThreadEvent] Thread id [%d]:\n", tid);
    PINTOOL_DEBUG_PRINTF("\tThread event barrier!\n");

    threadDataVec[tid]->dynamicTrace.AddBarrierEvent();
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

    PINTOOL_DEBUG_PRINTF("Image name is [%s]\n", imageName);

    staticTrace = new StaticTraceWriter();
    if (staticTrace == NULL) {
        PINTOOL_DEBUG_PRINTF("Failed to create static trace file.\n");
        return;
    }
    if (staticTrace->OpenFile(traceDir, imageName)) {
        PINTOOL_DEBUG_PRINTF("Failed to open static trace file\n");
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
                 *         PINTOOL_DEBUG_PRINTF("Routine [%s] was not
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
    PINTOOL_DEBUG_PRINTF("End of tool execution!\n");

    if (imageName) {
        free((void*)imageName);
    }
    if (staticTrace) {
        delete staticTrace;
    }
    if (!wasInitInstrumentationCalled) {
        PINTOOL_DEBUG_PRINTF(
            "No instrumentation blocks were found in the target "
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

    PIN_InitLock(&debugPrintLock);
    PIN_InitLock(&threadStartLock);
    PIN_InitLock(&getThreadDataLock);

    if (knobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("Instrumenting entire program\n");
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