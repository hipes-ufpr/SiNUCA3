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

#include "../src/utils/logging.hpp"
#include "x86_generator_file_handler.hpp"

const int MEMREAD_EA = IARG_MEMORYREAD_EA;
const int MEMREAD_SIZE = IARG_MEMORYREAD_SIZE;
const int MEMWRITE_EA = IARG_MEMORYWRITE_EA;
const int MEMWRITE_SIZE = IARG_MEMORYWRITE_SIZE;
const int MEMREAD2_EA = IARG_MEMORYREAD2_EA;

/**
 * @brief Set this to 1 to print all rotines that name begins with "gomp",
 * case insensitive (Statically linking GOMP is recommended).
 */
const int DEBUG_PRINT_GOMP_RNT = 0;

/** @brief When this is enabled, every thread will be instrumented. */
static bool isInstrumentating;
/** @brief Enables instrumentation per thread. */
static std::vector<bool> isThreadInstrumentatingEnabled;

char* imageName = NULL;
const char* folderPath;

trace::traceGenerator::StaticTraceFile* staticTrace;
std::vector<trace::traceGenerator::DynamicTraceFile*> dynamicTraces;
std::vector<trace::traceGenerator::MemoryTraceFile*> memoryTraces;

PIN_LOCK pinLock;
std::vector<const char*> OMP_ignore;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", "-1",
                        "Maximum number of instructions to be traced");
KNOB<std::string> KnobFolder(KNOB_MODE_WRITEONCE, "pintool", "trace_folder",
                             "./", "Path to store trace");

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

void PrintRtnName(const char* s, THREADID tid) {
    PIN_GetLock(&pinLock, tid);
    SINUCA3_DEBUG_PRINTF("RNT called: %s\n", s);
    PIN_ReleaseLock(&pinLock);
}

VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&pinLock, tid);
    SINUCA3_DEBUG_PRINTF("New thread created! N => %d (%s)\n", tid, imageName);
    staticTrace->IncThreadCount();
    isThreadInstrumentatingEnabled.push_back(false);
    dynamicTraces.push_back(new trace::traceGenerator::DynamicTraceFile(
        folderPath, imageName, tid));
    memoryTraces.push_back(
        new trace::traceGenerator::MemoryTraceFile(folderPath, imageName, tid));
    PIN_ReleaseLock(&pinLock);
}

VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    PIN_GetLock(&pinLock, tid);
    delete dynamicTraces[tid];
    delete memoryTraces[tid];
    PIN_ReleaseLock(&pinLock);
}

VOID InitInstrumentation() {
    if (isInstrumentating) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("Start of tool instrumentation block\n");
    isInstrumentating = true;
    PIN_ReleaseLock(&pinLock);
}

VOID StopInstrumentation() {
    if (!isInstrumentating) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("End of tool instrumentation block\n");
    isInstrumentating = false;
    PIN_ReleaseLock(&pinLock);
}

VOID EnableInstrumentationInThread(THREADID tid) {
    if (isThreadInstrumentatingEnabled[tid]) return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Enabling tool instrumentation in thread [%d]\n", tid);
    isThreadInstrumentatingEnabled[tid] = true;
    PIN_ReleaseLock(&pinLock);
}

VOID DisableInstrumentationInThread(THREADID tid) {
    if (!isThreadInstrumentatingEnabled[tid]) return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Disabling tool instrumentation in thread [%d]\n", tid);
    isThreadInstrumentatingEnabled[tid] = false;
    PIN_ReleaseLock(&pinLock);
}

VOID AppendToDynamicTrace(UINT32 bblId) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;
    dynamicTraces[tid]->DynAppendToBuffer(&bblId, sizeof(trace::BBLID));
}

VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;
    static trace::DataMEM data;
    data.addr = addr;
    data.size = size;
    memoryTraces[tid]->MemAppendToBuffer(&data, sizeof(data));
}

VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;

    static trace::DataMEM readings[64];
    static trace::DataMEM writings[64];
    static unsigned short numR;
    static unsigned short numW;

    memoryTraces[tid]->PrepareDataNonStdAccess(&numR, readings, &numW, writings,
                                               accessInfo);
    memoryTraces[tid]->MemAppendToBuffer(&numR, SIZE_NUM_MEM_R_W);
    memoryTraces[tid]->MemAppendToBuffer(&numW, SIZE_NUM_MEM_R_W);
    memoryTraces[tid]->MemAppendToBuffer(readings, numR * sizeof(*readings));
    memoryTraces[tid]->MemAppendToBuffer(writings, numW * sizeof(*writings));
}

VOID InstrumentMemoryOperations(const INS* ins) {
    /*
     * INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise
     */
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
                       MEMREAD_EA, MEMREAD_SIZE, IARG_END);
    }
    if (hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       MEMREAD2_EA, MEMREAD_SIZE, IARG_END);
    }
    if (isWrite) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       MEMWRITE_EA, MEMWRITE_SIZE, IARG_END);
    }
}

VOID Trace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    THREADID tid = PIN_ThreadId();
    RTN traceRtn = TRACE_Rtn(trace);

    if (RTN_Valid(traceRtn)) {
        const char* traceRtnName = RTN_Name(traceRtn).c_str();
#if DEBUG_PRINT_GOMP_RNT == 1
        if (StrStartsWithGomp(traceRtnName)) {
            TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)PrintRtnName,
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

    PIN_GetLock(&pinLock, tid);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_UINT32, staticTrace->GetBBlCount(), IARG_END);

        staticTrace->IncBBlCount();
        unsigned int numIns = BBL_NumIns(bbl);
        staticTrace->StAppendToBuffer(&numIns, SIZE_NUM_BBL_INS);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            static struct trace::DataINS data;
            staticTrace->PrepareData(&data, &ins);
            staticTrace->StAppendToBuffer(&data, sizeof(data));
            InstrumentMemoryOperations(&ins);
            staticTrace->IncInstCount();
        }
    }

    PIN_ReleaseLock(&pinLock);
    return;
}

VOID ImageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string completeImgPath = IMG_Name(img);
    unsigned long imgPathLen = completeImgPath.size();
    const char* completeImgPathPtr = completeImgPath.c_str();

    // As Pin gives us the absolute path for the executable, it always have at
    // least a / (the root of the filesystem).
    int idx = 0;
    for (int i = imgPathLen - 1; i >= 0; --i) {
        if (completeImgPathPtr[i] == '/') {
            idx = i + 1;
            break;
        }
    }

    unsigned long imageNameLen = imgPathLen - idx + sizeof('\0');
    imageName = (char*)malloc(imageNameLen); // freed in Fini()
    memcpy(imageName, completeImgPathPtr + idx, imageNameLen);

    staticTrace =
        new trace::traceGenerator::StaticTraceFile(folderPath, imageName);
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

VOID Fini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    SINUCA3_DEBUG_PRINTF("Number of BBLs [%u]\n", staticTrace->GetBBlCount());

    if(imageName != NULL)
        free(imageName);

    // Close static trace file
    delete staticTrace;
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

    PIN_InitLock(&pinLock);

    isInstrumentating = false;

    // All these functions have a PAUSE instruction (Spin-lock hint)
    OMP_ignore.push_back("gomp_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_cancel_end");
    OMP_ignore.push_back("gomp_mutex_lock_slow");
    OMP_ignore.push_back("GOMP_doacross_wait");
    OMP_ignore.push_back("GOMP_doacross_ull_wait");
    OMP_ignore.push_back("gomp_ptrlock_get_slow");
    OMP_ignore.push_back("gomp_sem_wait_slow");

    IMG_AddInstrumentFunction(ImageLoad, NULL);
    TRACE_AddInstrumentFunction(Trace, NULL);
    PIN_AddFiniFunction(Fini, NULL);

    PIN_AddThreadStartFunction(ThreadStart, NULL);
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}
