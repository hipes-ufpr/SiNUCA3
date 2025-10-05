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
 */

#include <sinuca3.hpp>

#include "pin.H"
#include "utils/dynamic_trace_writer.hpp"
#include "utils/memory_trace_writer.hpp"
#include "utils/static_trace_writer.hpp"

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
}

using namespace sinucaTracer;

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
};

std::vector<ThreadData*> threadDataVec;
sinucaTracer::StaticTraceWriter* staticTrace;

/** @brief Used to block two threads from trying to print simultaneously. */
PIN_LOCK debugPrintLock;
/** @brief OnThreadStart writes in global structures. */
PIN_LOCK threadStartLock;

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

const char* GOMP_RTNS_FORCE_LOCK[] = {"GOMP_critical_start", "omp_set_lock",
                                      "omp_set_nest_lock", NULL};

const char* GOMP_RTNS_ATTEMPT_LOCK[] = {"omp_test_lock", "omp_test_nest_lock",
                                        NULL};

const char* GOMP_RTNS_UNLOCK[] = {"GOMP_critical_end", "omp_unset_lock",
                                  "omp_unset_nest_lock", NULL};

const char* GOMP_RTNS_BARRIER[] = {"GOMP_barrier", NULL};

const char* GOMP_RTNS_DESTROY[] = {"GOMP_parallel_end", NULL};

#define PINTOOL_DEBUG_PRINTF(...)                     \
    {                                                 \
        PIN_GetLock(&debugPrintLock, PIN_ThreadId()); \
        printf("[DEBUG] ");                           \
        printf(__VA_ARGS__);                          \
        PIN_ReleaseLock(&debugPrintLock);             \
    }

int Usage() {
    SINUCA3_LOG_PRINTF(
        "To enable instrumentation, wrap the target code with "
        "BeginInstrumentationBlock() and EndInstrumentationBlock().\n"
        "Instrumentation code is only inserted within these blocks, and "
        "analysis is only executed if the thread has\n"
        "called EnableThreadInstrumentation().\n"
        "Use the -f flag to force instrumentation even when no blocks are "
        "defined.\n");
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
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

    THREADID parentThreadId = PIN_GetParentTid();
    PINTOOL_DEBUG_PRINTF("Thread [%d] created! Parent is [%d]\n", tid,
                         parentThreadId);

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
    if (parentThreadId != tid) {
        threadDataVec[parentThreadId]->dynamicTrace.AddThreadEvent(
            ThreadEventCreateThread, tid);
    }

    staticTrace->IncThreadCount();

    PIN_ReleaseLock(&threadStartLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    delete threadDataVec[tid];
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bblId, UINT32 numInst) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) return;
    if (threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId)) {
        PINTOOL_DEBUG_PRINTF("Failed to add basic block id to file\n");
    }
    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);
}

/** @brief Append non standard memory op to memory trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) return;

    unsigned long address;
    unsigned int size;
    unsigned char type;

    int totalOps = accessInfo->numberOfMemops;
    if (threadDataVec[tid]->memoryTrace.AddNumberOfMemOperations(totalOps)) {
        PINTOOL_DEBUG_PRINTF("Failed to add number of mem ops to file\n");
    }

    for (int i = 0; i < totalOps; ++i) {
        if (!accessInfo->memop[i].maskOn) continue;

        address = accessInfo->memop[i].memoryAddress;
        size = accessInfo->memop[i].bytesAccessed;
        type = (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD)
                   ? MemoryOperationLoad
                   : MemoryOperationStore;

        if (threadDataVec[tid]->memoryTrace.AddMemoryOperation(address, size,
                                                               type)) {
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

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        unsigned int numberInstInBasicBlock = BBL_NumIns(bbl);
        unsigned int basicBlockIndex = staticTrace->GetBasicBlockCount();

        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, basicBlockIndex,
                       IARG_UINT32, numberInstInBasicBlock, IARG_END);

        if (staticTrace->AddBasicBlockSize(numberInstInBasicBlock)) {
            PINTOOL_DEBUG_PRINTF("Failed to add basic block count to file\n");
        }
        staticTrace->IncBasicBlockCount();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            if (staticTrace->AddInstruction(&ins)) {
                PINTOOL_DEBUG_PRINTF("Failed to add instruction to file\n");
            }
            staticTrace->IncStaticInstructionCount();

            bool hasMemOperators =
                INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);

            if (hasMemOperators) {
                PINTOOL_DEBUG_PRINTF("Instruction [%s] has memory operators\n",
                                     INS_Mnemonic(ins).c_str());
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                               IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                               IARG_END);
            }
        }
    }
}

/** @brief */
VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;

    std::string absoluteImgPath = IMG_Name(img);
    long size = absoluteImgPath.length() + sizeof('\0');
    char* name = (char*)malloc(size);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string sub = absoluteImgPath.substr(idx);
    strcpy(name, sub.c_str());
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

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();

            if (strcmp(name, "BeginInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
            }
            if (strcmp(name, "EndInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
            }
            if (strcmp(name, "EnableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)EnableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            }
            if (strcmp(name, "DisableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)DisableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            }

            const char* gompRtnName;
            for (int i = 0;; ++i) {
                if (!(gompRtnName = GOMP_RTNS_FORCE_LOCK[i])) break;

                if (strcmp(name, gompRtnName) == 0) {
                    // RTN_InsertCall();
                    PINTOOL_DEBUG_PRINTF("Found lock\n");
                }
            }
            for (int i = 0;; ++i) {
                if (!(gompRtnName = GOMP_RTNS_ATTEMPT_LOCK[i])) break;

                if (strcmp(name, gompRtnName) == 0) {
                    // RTN_InsertCall();
                    PINTOOL_DEBUG_PRINTF("Found test lock\n");
                }
            }
            for (int i = 0;; ++i) {
                if (!(gompRtnName = GOMP_RTNS_BARRIER[i])) break;

                if (strcmp(name, gompRtnName) == 0) {
                    // RTN_InsertCall();
                    PINTOOL_DEBUG_PRINTF("Found barrier\n");
                }
            }
            for (int i = 0;; ++i) {
                if (!(gompRtnName = GOMP_RTNS_UNLOCK[i])) break;

                if (strcmp(name, gompRtnName) == 0) {
                    // RTN_InsertCall();
                    PINTOOL_DEBUG_PRINTF("Found unlock\n");
                }
            }
            for (int i = 0;; ++i) {
                if (!(gompRtnName = GOMP_RTNS_DESTROY[i])) break;

                if (strcmp(name, gompRtnName) == 0) {
                    // RTN_InsertCall();
                    PINTOOL_DEBUG_PRINTF("Found thread end\n");
                }
            }

            RTN_Close(rtn);
        }
    }
}

/** @brief */
VOID OnFini(INT32 code, VOID* ptr) {
    PINTOOL_DEBUG_PRINTF("End of tool execution\n");

    if (imageName) {
        free((void*)imageName);
    }
    delete staticTrace;

    if (!wasInitInstrumentationCalled) {
        PINTOOL_DEBUG_PRINTF(
            "No instrumentation blocks were found in the target program.\n"
            "As result, no instruction data was recorded. \n\n");
        Usage();
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