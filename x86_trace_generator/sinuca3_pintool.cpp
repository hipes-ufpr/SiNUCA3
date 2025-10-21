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
static bool isInstrumentating;

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
};

std::vector<ThreadData*> threadDataVec;
StaticTraceWriter* staticTrace;
std::vector<std::string> rtnsWithPauseInst;

/** @brief Used to block two threads from trying to print simultaneously. */
static PIN_LOCK debugPrintLock;
/** @brief OnThreadStart writes in global structures. */
static PIN_LOCK threadStartLock;
/** @brief */
static PIN_LOCK getParentThreadLock;

static const char* traceDir = NULL;
static const char* imageName = NULL;
static bool wasInitInstrumentationCalled = false;
/** @brief */
static unsigned int numberOfActiveThreads = 1;
/** @brief */
static THREADID parentThreadId;

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
    PINTOOL_DEBUG_PRINTF("Start of tool instrumentation block\n");
    wasInitInstrumentationCalled = true;
    isInstrumentating = true;
}

/** @brief Disables instrumentation. */
VOID StopInstrumentation() {
    if (!isInstrumentating || knobForceInstrumentation.Value()) return;
    PINTOOL_DEBUG_PRINTF("End of tool instrumentation block\n")
    isInstrumentating = false;
}

/** @brief Enables execution of analysis code. */
VOID EnableInstrumentationInThread(THREADID tid) {
    if (threadDataVec[tid]->isThreadAnalysisEnabled) return;
    PINTOOL_DEBUG_PRINTF("Enabled thread [%d] to run analysis code\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = true;
}

/** @brief Disables execution of analysis code. */
VOID DisableInstrumentationInThread(THREADID tid) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled ||
        knobForceInstrumentation.Value()) {
        return;
    }
    PINTOOL_DEBUG_PRINTF("Disabled thread [%d] to run analysis code\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = false;
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&threadStartLock, tid);

    /* check if tid is sequential */
    if (tid != threadDataVec.size()) {
        SINUCA3_ERROR_PRINTF("Thread data not created. Tid isnt sequential.\n");
        return;
    }

    struct ThreadData* threadData = new ThreadData;
    if (!threadData) {
        SINUCA3_ERROR_PRINTF("Failed to alloc thread data.\n");
        return;
    }

    /* create files */
    if (threadData->dynamicTrace.OpenFile(traceDir, imageName, tid)) {
        PINTOOL_DEBUG_PRINTF("Failed to open dynamic trace file\n");
        return;
    }
    if (threadData->memoryTrace.OpenFile(traceDir, imageName, tid)) {
        PINTOOL_DEBUG_PRINTF("Failed to open memory trace file\n");
        return;
    }

    threadData->isThreadAnalysisEnabled = false;
    threadDataVec.push_back(threadData);
    staticTrace->IncThreadCount();

    PIN_ReleaseLock(&threadStartLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    delete threadDataVec[tid];
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bblId, UINT32 numInst) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) {
        return;
    }
    if (threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId)) {
        PINTOOL_DEBUG_PRINTF("Failed to add basic block id to file\n");
    }
    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);

    /* new thread created or reused */
    if (numberOfActiveThreads <= tid) {
        threadDataVec[parentThreadId]->dynamicTrace.AddThreadEvent(
            ThreadEventCreateThread, tid);
        ++numberOfActiveThreads;

        PINTOOL_DEBUG_PRINTF("Thread [%d] created and parent is [%d]\n", tid,
                         parentThreadId);
    }
}

/** @brief Append non standard memory op to memory trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) {
        return;
    }

    /* comment */
    int totalOps = accessInfo->numberOfMemops;
    if (threadDataVec[tid]->memoryTrace.AddNumberOfMemOperations(totalOps)) {
        PINTOOL_DEBUG_PRINTF("Failed to add number of mem ops to file\n");
        return;
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
            return;
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

    /* comment */
    for (std::string& str : rtnsWithPauseInst) {
        if (rtnName == str) {
            return;
        }
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        /* comment */
        unsigned int numberInstInBasicBlock = BBL_NumIns(bbl);
        unsigned int basicBlockIndex = staticTrace->GetBasicBlockCount();
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, basicBlockIndex,
                       IARG_UINT32, numberInstInBasicBlock, IARG_END);
        /* comment */
        if (staticTrace->AddBasicBlockSize(numberInstInBasicBlock)) {
            PINTOOL_DEBUG_PRINTF("Failed to add basic block count to file\n");
        }
        /* comment */
        staticTrace->IncBasicBlockCount();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (staticTrace->AddInstruction(&ins)) {
                PINTOOL_DEBUG_PRINTF("Failed to add instruction to file\n");
            }
            /* comment */
            staticTrace->IncStaticInstructionCount();

            /* comment */
            bool hasMemOperators =
                INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
            if (hasMemOperators) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                               IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                               IARG_END);
            }
        }
    }

}

VOID OnThreadEvent(THREADID tid, INT32 eid, UINT32 event) {
    PINTOOL_DEBUG_PRINTF("Thread event is [%d] and event id [%d]\n", event,
                         eid);

    if (threadDataVec.size() - tid <= 0) {
        PINTOOL_DEBUG_PRINTF("Error while adding thread event [1]\n");
        return;
    }
    if (event == ThreadEventCreateThread) {
        PIN_GetLock(&getParentThreadLock, tid);
        parentThreadId = tid;
        PIN_ReleaseLock(&getParentThreadLock);
    } else {
        if (threadDataVec[tid]->dynamicTrace.AddThreadEvent(event, eid)) {
            PINTOOL_DEBUG_PRINTF("Error while adding thread event [2]\n");
            return;
        }
    }

    if (event == ThreadEventDestroyThread) {
        numberOfActiveThreads = parentThreadId + 1;
    }
}

/** @brief */
VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;

    struct ThreadEvent {
        std::string name;
        ThreadEventType type;
    };

    static ThreadEvent GOMP_RTNS[] = {
        {"GOMP_critical_start", ThreadEventLockRequest},
        {"omp_set_lock", ThreadEventLockRequest},
        {"omp_set_nest_lock", ThreadEventNestLockRequest},
        {"omp_test_lock", ThreadEventLockAttempt},
        {"omp_test_nest_lock", ThreadEventNestLockAttempt},
        {"GOMP_critical_end", ThreadEventUnlock},
        {"omp_unset_lock", ThreadEventUnlock},
        {"omp_unset_nest_lock", ThreadEventUnlock},
        {"GOMP_barrier", ThreadEventBarrier},
        {"GOMP_parallel_end", ThreadEventDestroyThread},
        {"gomp_team_start", ThreadEventCreateThread},
        {"gomp_team_end", ThreadEventDestroyThread}};

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

    int eventIdentifier = 0;
    parentThreadId = 0;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* rtnName = RTN_Name(rtn).c_str();

            if (strcmp(rtnName, "BeginInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
            }
            if (strcmp(rtnName, "EndInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
            }
            if (strcmp(rtnName, "EnableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)EnableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            }
            if (strcmp(rtnName, "DisableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)DisableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            }

            for (unsigned long i = 0;
                 i < sizeof(GOMP_RTNS) / sizeof(ThreadEvent); ++i) {
                if (strcmp(rtnName, GOMP_RTNS[i].name.c_str()) == 0) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)OnThreadEvent,
                                   IARG_THREAD_ID, IARG_UINT32, eventIdentifier,
                                   IARG_UINT32, GOMP_RTNS[i].type, IARG_END);

                    ++eventIdentifier;
                }
            }

            /* pause instruction is spin lock hint */
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                if (INS_Mnemonic(ins) == "PAUSE") {
                    PINTOOL_DEBUG_PRINTF("Found pause in [%s]\n", rtnName);
                    rtnsWithPauseInst.push_back(rtnName);
                }
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
            "No instrumentation blocks were found in the target program.\n\n");
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
    PIN_InitLock(&getParentThreadLock);

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