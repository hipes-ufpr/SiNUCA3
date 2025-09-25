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

extern "C" {
#include <sys/stat.h>  // mkdir
#include <unistd.h>    // access
}

#include <sinuca3.hpp>
#include <utils/dynamic_trace_writer.hpp>
#include <utils/memory_trace_writer.hpp>
#include <utils/static_trace_writer.hpp>

/**
 * @brief Set this to 1 to print all rotines that name begins with "gomp",
 * case insensitive (Statically linking GOMP is recommended).
 */
const int DEBUG_PRINT_GOMP_RNT = 0;

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
/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief Indicates, for each thread, whether it is allowed to execute
 * previously inserted analysis code.
 *
 * This flag does not control the instrumentation process itself (i.e., whether
 * code is inserted into the target program), but rather whether a specific
 * thread is permitted to execute that analysis code at runtime.
 *
 * The insertion of analysis code occurs only when the global
 * `isInstrumentating` flag is enabled. Later, during program execution, the
 * inserted code will only be executed by a thread if its corresponding entry in
 * this vector is set to true.
 *
 * When executed, the analysis code records dynamic and memory trace information
 * into files associated with the executing thread.
 */
std::vector<bool> isThreadAnalysisEnabled;

/** @brief Used to block two threads from trying to print simultaneously. */
PIN_LOCK printLock;
/** @brief OnThreadStart writes in global structures. */
PIN_LOCK threadStartLock;
/** @brief OnTrace writes in global structures. */
PIN_LOCK onTraceLock;

const char* folderPath;

char* imageName = NULL;
bool wasInitInstrumentationCalled = false;

sinucaTracer::StaticTraceFile* staticTrace;
std::vector<sinucaTracer::DynamicTraceFile*> dynamicTraces;
std::vector<sinucaTracer::MemoryTraceFile*> memoryTraces;

/** @brief Set directory to save trace with '-o'. Default is current dir. */
KNOB<std::string> KnobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Path to store the trace files");
/** @brief Allows one to force full instrumentation with '-f'. Default is 0. */
KNOB<BOOL> KnobForceInstrumentation(
    KNOB_MODE_WRITEONCE, "pintool", "f", "0",
    "Force instrumentation for the entire execution for all created threads");

int Usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

bool StrStartsWithGomp(const char* s) {
    const char* prefix = "gomp";
    for (int i = 0; prefix[i] != '\0'; ++i) {
        if (std::tolower(s[i]) != prefix[i]) {
            return false;
        }
    }
    return true;
}

void DebugPrintRtnName(const char* s, THREADID tid) {
    PIN_GetLock(&printLock, tid);
    SINUCA3_DEBUG_PRINTF("RNT called: %s\n", s);
    PIN_ReleaseLock(&printLock);
}

/** @brief */
VOID InitInstrumentation() {
    if (isInstrumentating) return;
    PIN_GetLock(&printLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("Start of tool instrumentation block\n");
    isInstrumentating = true;
    wasInitInstrumentationCalled = true;
    PIN_ReleaseLock(&printLock);
}

/** @brief */
VOID StopInstrumentation() {
    if (!isInstrumentating || KnobForceInstrumentation.Value()) return;
    PIN_GetLock(&printLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("End of tool instrumentation block\n");
    isInstrumentating = false;
    PIN_ReleaseLock(&printLock);
}

/** @brief */
VOID EnableInstrumentationInThread(THREADID tid) {
    if (isThreadAnalysisEnabled[tid]) return;
    PIN_GetLock(&printLock, tid);
    SINUCA3_LOG_PRINTF("Enabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = true;
    PIN_ReleaseLock(&printLock);
}

/** @brief */
VOID DisableInstrumentationInThread(THREADID tid) {
    if (!isThreadAnalysisEnabled[tid] || KnobForceInstrumentation.Value())
        return;
    PIN_GetLock(&printLock, tid);
    SINUCA3_LOG_PRINTF("Disabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = false;
    PIN_ReleaseLock(&printLock);
}

/** @brief */
VOID AppendToDynamicTrace(UINT32 bblId, UINT32 numInst) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    dynamicTraces[tid]->PrepareId(bblId);
    dynamicTraces[tid]->AppendToBufferId();
    dynamicTraces[tid]->IncTotalExecInst(numInst);
}

/** @brief */
VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataStdMemAccess(addr, size);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

/** @brief */
VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataNonStdAccess(accessInfo);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

/** @brief */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&threadStartLock, tid);
    SINUCA3_DEBUG_PRINTF("New thread created! tid [%d] (%s)\n", tid, imageName);
    staticTrace->IncThreadCount();
    isThreadAnalysisEnabled.push_back(KnobForceInstrumentation.Value());

    dynamicTraces.push_back(
        new sinucaTracer::DynamicTraceFile(folderPath, imageName, tid));
    memoryTraces.push_back(
        new sinucaTracer::MemoryTraceFile(folderPath, imageName, tid));
    PIN_ReleaseLock(&threadStartLock);
}

/** @brief */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    /* delete is thread safe for distinct objetcs in modern implementation */
    delete dynamicTraces[tid];
    delete memoryTraces[tid];
}

/** @brief */
VOID InsertInstrumentionOnMemoryOperations(const INS* ins) {
    /* INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise */
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        return;
    }

    bool isRead = INS_IsMemoryRead(*ins);
    bool hasRead2 = INS_HasMemoryRead2(*ins);
    bool isWrite = INS_IsMemoryWrite(*ins);

    if (isRead) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
    if (hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
    }
    if (isWrite) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
    }
}

/** @brief */
VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    THREADID tid = PIN_ThreadId();
    RTN traceRtn = TRACE_Rtn(trace);

    if (RTN_Valid(traceRtn)) {
#if DEBUG_PRINT_GOMP_RNT == 1
        const char* traceRtnName = RTN_Name(traceRtn).c_str();
        if (StrStartsWithGomp(traceRtnName)) {
            TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)DebugPrintRtnName,
                             IARG_PTR, traceRtnName, IARG_THREAD_ID, IARG_END);
        }
#endif
    }

    PIN_GetLock(&onTraceLock, tid);
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        unsigned int numberInstBBl = BBL_NumIns(bbl);
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_UINT32, staticTrace->GetBBlCount(), IARG_UINT32,
                       numberInstBBl, IARG_END);

        staticTrace->IncBBlCount();
        staticTrace->AppendToBufferNumIns(numberInstBBl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            staticTrace->PrepareDataINS(&ins);
            staticTrace->AppendToBufferDataINS();
            InsertInstrumentionOnMemoryOperations(&ins);
            staticTrace->IncInstCount();
        }
    }
    PIN_ReleaseLock(&onTraceLock);

    return;
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

    staticTrace = new sinucaTracer::StaticTraceFile(folderPath, imageName);
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

            RTN_Close(rtn);
        }
    }
}

/** @brief */
VOID OnFini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    SINUCA3_LOG_PRINTF("Number of BBLs [%u]\n", staticTrace->GetBBlCount());

    if (imageName != NULL) free(imageName);

    delete staticTrace;

    if (!wasInitInstrumentationCalled) {
        SINUCA3_WARNING_PRINTF(
            "No instrumentation blocks were found in the target program.\n"
            "As result, no instruction data was recorded in the trace files.\n"
            "To enable instrumentation, wrap the target code with "
            "BeginInstrumentationBlock() and EndInstrumentationBlock().\n"
            "Instrumentation code is only inserted within these blocks, and "
            "analysis is only executed if the thread has\n"
            "called EnableThreadInstrumentation().\n"
            "Use the -f flag to force instrumentation even when no blocks are "
            "defined.\n"
            "Use the -f flag to force instrumentation even when no blocks are "
            "defined.\n");
    }
}

/** @brief */
int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    folderPath = KnobFolder.Value().c_str();
    if (access(folderPath, F_OK) != 0) {
        mkdir(folderPath, S_IRWXU | S_IRWXG | S_IROTH);
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
