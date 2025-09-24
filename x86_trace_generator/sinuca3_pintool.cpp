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

#include "sinuca3_pintool.hpp"

const int DEBUG_PRINT_GOMP_RNT = 0;
const char* folderPath;

char* imageName = NULL;
bool isInstrumentating;
bool wasInitInstrumentationCalled = false;

sinucaTracer::StaticTraceFile* staticTrace;
std::vector<sinucaTracer::DynamicTraceFile*> dynamicTraces;
std::vector<sinucaTracer::MemoryTraceFile*> memoryTraces;
std::vector<bool> isThreadAnalysisEnabled;

PIN_LOCK printLock;
PIN_LOCK threadStartLock;
PIN_LOCK onTraceLock;

std::vector<const char*> OMP_ignore;

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

void InitGompHandling() {
    /* All these functions have a PAUSE instruction (Spin-lock hint) */
    OMP_ignore.push_back("gomp_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_cancel_end");
    OMP_ignore.push_back("gomp_mutex_lock_slow");
    OMP_ignore.push_back("GOMP_doacross_wait");
    OMP_ignore.push_back("GOMP_doacross_ull_wait");
    OMP_ignore.push_back("gomp_ptrlock_get_slow");
    OMP_ignore.push_back("gomp_sem_wait_slow");
}

void DebugPrintRtnName(const char* s, THREADID tid) {
    PIN_GetLock(&printLock, tid);
    SINUCA3_DEBUG_PRINTF("RNT called: %s\n", s);
    PIN_ReleaseLock(&printLock);
}

VOID InitInstrumentation() {
    if (isInstrumentating) return;
    PIN_GetLock(&printLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("Start of tool instrumentation block\n");
    isInstrumentating = true;
    wasInitInstrumentationCalled = true;
    PIN_ReleaseLock(&printLock);
}

VOID StopInstrumentation() {
    if (!isInstrumentating || KnobForceInstrumentation.Value()) return;
    PIN_GetLock(&printLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("End of tool instrumentation block\n");
    isInstrumentating = false;
    PIN_ReleaseLock(&printLock);
}

VOID EnableInstrumentationInThread(THREADID tid) {
    if (isThreadAnalysisEnabled[tid]) return;
    PIN_GetLock(&printLock, tid);
    SINUCA3_LOG_PRINTF("Enabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = true;
    PIN_ReleaseLock(&printLock);
}

VOID DisableInstrumentationInThread(THREADID tid) {
    if (!isThreadAnalysisEnabled[tid] || KnobForceInstrumentation.Value())
        return;
    PIN_GetLock(&printLock, tid);
    SINUCA3_LOG_PRINTF("Disabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = false;
    PIN_ReleaseLock(&printLock);
}

VOID AppendToDynamicTrace(UINT32 bblId, UINT32 numInst) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    dynamicTraces[tid]->PrepareId(bblId);
    dynamicTraces[tid]->AppendToBufferId();
    dynamicTraces[tid]->IncTotalExecInst(numInst);
}

VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataStdMemAccess(addr, size);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataNonStdAccess(accessInfo);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

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

VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    /* delete is thread safe for distinct objetcs in modern implementation */
    delete dynamicTraces[tid];
    delete memoryTraces[tid];
}

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

VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    THREADID tid = PIN_ThreadId();
    RTN traceRtn = TRACE_Rtn(trace);

    if (RTN_Valid(traceRtn)) {
        const char* traceRtnName = RTN_Name(traceRtn).c_str();
#if DEBUG_PRINT_GOMP_RNT == 1
        if (StrStartsWithGomp(traceRtnName)) {
            TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)DebugPrintRtnName,
                             IARG_PTR, traceRtnName, IARG_THREAD_ID, IARG_END);
        }
#endif
        /*
         * This will make every function call from libgomp that have a
         * PAUSE instruction to be ignored
         * Still not sure if this is fully correct
         */
        for (unsigned int i = 0; i < OMP_ignore.size(); ++i) {
            if (strcmp(traceRtnName, OMP_ignore[i]) == 0) {
                // has SPIN_LOCK
                return;
            }
        }
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

    InitGompHandling();

    IMG_AddInstrumentFunction(OnImageLoad, NULL);
    TRACE_AddInstrumentFunction(OnTrace, NULL);
    PIN_AddFiniFunction(OnFini, NULL);

    PIN_AddThreadStartFunction(OnThreadStart, NULL);
    PIN_AddThreadFiniFunction(OnThreadFini, NULL);

    PIN_StartProgram();

    return 0;
}
