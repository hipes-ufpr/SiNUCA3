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

#include <cassert>  // assert

#include "pin.H"
#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <sys/stat.h>  // mkdir
#include <unistd.h>    // access
}

#include <sinuca3.hpp>
#include "utils/dynamic_trace_writer.hpp"
#include "utils/memory_trace_writer.hpp"
#include "utils/static_trace_writer.hpp"

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
    sinucaTracer::DynamicTraceWriter dynamicTrace;
    sinucaTracer::MemoryTraceWriter memoryTrace;
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

std::vector<struct ThreadData*> threadDataVec;
sinucaTracer::StaticTraceFile* staticTrace;

/** @brief Used to block two threads from trying to print simultaneously. */
PIN_LOCK debugPrintLock;
/** @brief OnThreadStart writes in global structures. */
PIN_LOCK threadStartLock;
/** @brief OnTrace writes in global structures. */
PIN_LOCK onTraceLock;

const char* traceDir;
char* imageName = NULL;
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

const char* GOMP_RTNS_DESTROY[] = {"GOMP_parallel_end", NULL}

#ifndef NDEBUG
#define PINTOOL_DEBUG_PRINTF(...)                         \
    {                                                     \
        PIN_GetLock(&debugPrintLock, PIN_ThreadId());     \
        printf("[DEBUG] ");                               \
        printf(__VA_ARGS__);                              \
        PIN_ReleaseLock(&debugPrintLock, PIN_ThreadId()); \
    }
#else
#define PINTOOL_DEBUG_PRINTF(...) \
    do {                          \
    } while (0)
#endif

int Usage() {
    SINUCA3_LOG_PRINTF(
        "To enable instrumentation, wrap the target code with "
        "BeginInstrumentationBlock() and EndInstrumentationBlock().\n"
        "Instrumentation code is only inserted within these blocks, and "
        "analysis is only executed if the thread has\n"
        "called EnableThreadInstrumentation().\n"
        "Use the -f flag to force instrumentation even when no blocks are "
        "defined.\n\n");
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
    if (!isInstrumentating || KnobForceInstrumentation.Value()) return;
    PINTOOL_DEBUG_PRINTF("End of tool instrumentation block\n")
    isInstrumentating = false;
}

/** @brief Enables execution of analysis code. */
VOID EnableInstrumentationInThread(THREADID tid) {
    if (threadDataVec[tid]->isThreadAnalysisEnabled) return;
    PINTOOL_DEBUG_PRINTF("Enabled tool instrumentation in thread [%d]\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = true;
}

/** @brief Disables execution of analysis code. */
VOID DisableInstrumentationInThread(THREADID tid) {
    if (!threadDataVec[tid]->isThreadAnalysisEnabled ||
        KnobForceInstrumentation.Value()) {
        return;
    }
    PINTOOL_DEBUG_PRINTF("Disabled tool instrumentation in thread [%d]\n", tid);
    threadDataVec[tid]->isThreadAnalysisEnabled = false;
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&threadStartLock, tid);
    int parentThreadId = PIN_GetParentTid();
    SINUCA3_DEBUG_PRINTF("Thread [%d] created! Parent is [%d]\n", tid,
                         imageName, parentThreadId);
    staticTrace->IncThreadCount();
    if (tid != threadDataVec.size()) {
        SINUCA3_ERROR_PRINTF("Thread data not created. Tid isnt sequential.\n");
        return;
    }
    struct ThreadData* threadData = new ThreadData;
    if (!threadData) {
        SINUCA3_ERROR_PRINTF("Failed to alloc thread data.\n");
        return;
    }
    threadDataVec.push_back(threadData);
    threadData[parentThreadId]->dynamicTrace.AddThreadCreate(tid);
    PIN_ReleaseLock(&threadStartLock, tid);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    /* delete is thread safe */
    delete threadDataVec[tid];
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(UINT32 bblId, UINT32 numInst) {
    int tid = PIN_ThreadId();
    if (!threadDataVec[tid]->isThreadAnalysisEnabled) return;
    threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId);
    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);
}

/** @brief Append standard memory op to memory trace. */
VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size, UINT8 type) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->AddMemoryOperation(addr, size, type);
    PINTOOL_DEBUG_PRINTF(
        "New memory operation: addr[%lu], size[%u], type[%u], tid[%d]\n", addr,
        size, type, tid);
}

/** @brief Append non standard memory op to memory trace. */
VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    unsigned long addr;
    unsigned int size;
    unsigned char type;

    int totalOps = accessInfo->numberOfMemops;
    threadDataVec[tid]->memoryTrace.AddNonStdOpHeader(totalOps);
    for (int i = 0; i < totalOps; ++i) {
        if (accessInfo->memop[i].maskOn == false) {
            continue;
        }
        addr = accessInfo->memop[i].address;
        size = accessInfo->memop[i].size;
        if (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD) {
            type = sinucaTracer::MemoryOperationLoad;
        } else if (accessInfo->memop[i].memopType == PIN_MEMOP_STORE) {
            type = sinucaTracer::MemoryOperationStore;
        }
        threadDataVec[tid]->memoryTrace.AddMemoryOperation(addr, size, type);
        PINTOOL_DEBUG_PRINTF(
            "New memory operation: addr[%lu], size[%u], type[%u], tid[%d]\n",
            addr, size, type, tid);
    }
}

/** @brief Instrument instruction that have memory operators. */
VOID InsertInstrumentionOnMemoryOperations(const INS* ins) {
    /* INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise */
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        return;
    }
    if (INS_IsMemoryRead(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
                       sinucaTracer::MemoryOperationLoad, IARG_END);
    }
    if (INS_HasMemoryRead2(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
                       sinucaTracer::MemoryOperationLoad, IARG_END);
    }
    if (INS_IsMemoryWrite(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE,
                       sinucaTracer::MemoryOperationStore, IARG_END);
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
                       IARG_UINT32, basicBlockIndex, IARG_UINT32,
                       numberInstInBasicBlock, IARG_END);
        staticTrace->AddBasicBlockSize(numberInstInBasicBlock);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            staticTrace->AddInstruction(&ins);
        }
    }
}

/** @brief */
VOID OnImageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string absoluteImgPath = IMG_Name(img);
    long size = absoluteImgPath.length() + sizeof('\0');
    imageName = (char*)malloc(size);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string sub = absoluteImgPath.substr(idx);
    strcpy(imageName, sub.c_str());

    staticTrace = new sinucaTracer::StaticTraceFile();
    if (staticTrace == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create static trace file.\n");
        return;
    }
    staticTrace->OpenFile(traceDir, imageName);

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
                gompRtnName = GOMP_RTNS_FORCE_LOCK[i];
                if (gompRtnName == NULL) break;
                if (strcmp(name, gompRtnName) == 0) {
                    RTN_InsertCall();
                }
            }
            for (int i = 0;; ++i) {
                gompRtnName = GOMP_RTNS_ATTEMPT_LOCK[i];
                if (gompRtnName == NULL) break;
                if (strcmp(name, gompRtnName) == 0) {
                    RTN_InsertCall();
                }
            }
            for (int i = 0;; ++i) {
                gompRtnName = GOMP_RTNS_BARRIER[i];
                if (gompRtnName == NULL) break;
                if (strcmp(name, gompRtnName) == 0) {
                    RTN_InsertCall();
                }
            }
            for (int i = 0;; ++i) {
                gompRtnName = GOMP_RTNS_UNLOCK[i];
                if (gompRtnName == NULL) break;
                if (strcmp(name, gompRtnName) == 0) {
                    RTN_InsertCall();
                }
            }
            for (int i = 0;; ++i) {
                gompRtnName = GOMP_RTNS_DESTROY[i];
                if (gompRtnName == NULL) break;
                if (strcmp(name, gompRtnName) == 0) {
                    RTN_InsertCall();
                }
            }

            RTN_Close(rtn);
        }
    }
}

/** @brief */
VOID OnFini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    SINUCA3_LOG_PRINTF("Number of BBLs [%u]\n", staticTrace->GetBBlCount());

    staticTrace->WriteHeaderToFile();
    if (imageName != NULL) free(imageName);
    delete staticTrace;

    if (!wasInitInstrumentationCalled) {
        SINUCA3_WARNING_PRINTF(
            "No instrumentation blocks were found in the target program.\n"
            "As result, no instruction data was recorded in the trace "
            "files.\n");
        Usage();
    }
}

/** @brief */
int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    traceDir = KnobFolder.Value().c_str();
    if (access(traceDir, F_OK) != 0) {
        mkdir(traceDir, S_IRWXU | S_IRWXG | S_IROTH);
    }

    PIN_InitLock(&printLock);
    PIN_InitLock(&threadStartLock);
    PIN_InitLock(&onTraceLock);

    if (KnobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("Instrumentating entire program\n");
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
