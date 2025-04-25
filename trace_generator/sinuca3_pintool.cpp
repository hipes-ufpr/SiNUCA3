#include <cassert>  // assert
#include <cstddef>  // size_t
#include <cstdio>
#include <cstring>  // memcpy

#include "pin.H"

extern "C" {
#include <sys/stat.h>  // mkdir
#include <unistd.h>    // access
}

#include "../src/sinuca3.hpp"
#include "../src/utils/logging.hpp"
#include "./file_handler.hpp"
#include "sinuca3_pintool.hpp"

// Set this to 1 to print all rotines
// that name begins with "gomp", case insensitive
// (Statically linking GOMP is recommended)
#define DEBUG_PRINT_GOMP_RNT 0

// When this is enabled, every thread will be instrumented;
static bool isInstrumentating;
// And this enable instrumentation per thread.
static std::vector<bool> isThreadInstrumentatingEnabled;

const char* imageName;

traceGenerator::StaticTraceFile* staticTrace;
std::vector<traceGenerator::DynamicTraceFile*> dynamicTraces;
std::vector<traceGenerator::MemoryTraceFile*> memoryTraces;

PIN_LOCK pinLock;
std::vector<const char*> OMP_ignore;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", "-1",
                        "Maximum number of instructions to be traced");

int Usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

bool StrStartsWithGomp(const char* s) {
    const char* prefix = "gomp";
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
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
    staticTrace->numThreads++;
    isThreadInstrumentatingEnabled.push_back(false);
    dynamicTraces.push_back(
        new traceGenerator::DynamicTraceFile(imageName, tid));
    memoryTraces.push_back(new traceGenerator::MemoryTraceFile(imageName, tid));
    PIN_ReleaseLock(&pinLock);
}

VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    PIN_GetLock(&pinLock, tid);

    SINUCA3_DEBUG_PRINTF("A thread has finalized! N => %d\n", tid);

    delete dynamicTraces[tid];
    delete memoryTraces[tid];

    PIN_ReleaseLock(&pinLock);
}

VOID InitInstrumentation() {
    if (isInstrumentating) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("Start of tool instrumentation block.\n");
    isInstrumentating = true;
    PIN_ReleaseLock(&pinLock);
}

VOID StopInstrumentation() {
    if (!isInstrumentating) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("End of tool instrumentation block.\n");
    isInstrumentating = false;
    PIN_ReleaseLock(&pinLock);
}

VOID EnableInstrumentationInThread(THREADID tid) {
    if (isThreadInstrumentatingEnabled[tid]) return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Enabling tool instrumentation in thread %d.\n", tid);
    isThreadInstrumentatingEnabled[tid] = true;
    PIN_ReleaseLock(&pinLock);
}

VOID DisableInstrumentationInThread(THREADID tid) {
    if (!isThreadInstrumentatingEnabled[tid]) return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Disabling tool instrumentation in thread %d.\n", tid);
    isThreadInstrumentatingEnabled[tid] = false;
    PIN_ReleaseLock(&pinLock);
}

VOID AppendToDynamicTrace(UINT32 bblId) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;
    dynamicTraces[tid]->Write(bblId);
}

VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;
    static traceGenerator::DataMEM data;
    data.addr = addr;
    data.size = size;
    memoryTraces[tid]->WriteStd(&data);
}

VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* acessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadInstrumentatingEnabled[tid]) return;

    unsigned short numReadings;
    unsigned short numWritings;

    static traceGenerator::DataMEM readings[MAX_MEM_OPERATIONS];
    static traceGenerator::DataMEM writings[MAX_MEM_OPERATIONS];

    numReadings = numWritings = 0;
    for (unsigned short it = 0; it < acessInfo->numberOfMemops; it++) {
        PIN_MEM_ACCESS_INFO* memOp = &acessInfo->memop[it];
        if (memOp->memopType == PIN_MEMOP_LOAD) {
            readings[numReadings].addr = memOp->memoryAddress;
            readings[numReadings].size = memOp->bytesAccessed;
            numReadings++;
        } else {
            writings[numWritings].addr = memOp->memoryAddress;
            writings[numWritings].size = memOp->bytesAccessed;
            numWritings++;
        }
    }

    memoryTraces[tid]->WriteNonStd(readings, numReadings, writings,
                                   numWritings);
}

VOID InstrumentMemoryOperations(const INS* ins) {
    bool isRead = INS_IsMemoryRead(*ins);
    bool hasRead2 = INS_HasMemoryRead2(*ins);
    bool isWrite = INS_IsMemoryWrite(*ins);

    // INS_IsStandardMemop() returns false if this instruction has a memory
    // operand which has unconventional meaning; Returns true otherwise.
    bool isNonStandard = !INS_IsStandardMemop(*ins);
    if (isNonStandard) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        return;
    }

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

void createDataINS(const INS* ins, struct traceGenerator::DataINS* data) {
    std::string name = INS_Mnemonic(*ins);
    assert(name.length() + sizeof('\0') <=
               traceGenerator::MAX_INSTRUCTION_NAME_LENGTH &&
           "This is unexpected. You should increase "
           "MAX_INSTRUCTION_NAME_LENGTH value.");
    strncpy(data->name, name.c_str(),
            traceGenerator::MAX_INSTRUCTION_NAME_LENGTH);

    data->addr = INS_Address(*ins);
    data->size = INS_Size(*ins);
    data->baseReg = INS_MemoryBaseReg(*ins);
    data->indexReg = INS_MemoryIndexReg(*ins);
    data->booleanValues = 0;

    if (INS_IsPredicated(*ins))
        traceGenerator::SetBit(&data->booleanValues,
                               traceGenerator::IS_PREDICATED, true);

    if (INS_IsPrefetch(*ins))
        traceGenerator::SetBit(&data->booleanValues,
                               traceGenerator::IS_PREFETCH, true);

    bool isSyscall = INS_IsSyscall(*ins);
    bool isControlFlow = INS_IsControlFlow(*ins) || isSyscall;

    if (isControlFlow) {
        if (isSyscall)
            data->branchType = sinuca::BranchSyscall;
        else if (INS_IsCall(*ins))
            data->branchType = sinuca::BranchCall;
        else if (INS_IsRet(*ins))
            data->branchType = sinuca::BranchReturn;
        else if (INS_HasFallThrough(*ins))
            data->branchType = sinuca::BranchCond;
        else
            data->branchType = sinuca::BranchUncond;

        traceGenerator::SetBit(&data->booleanValues,
                               traceGenerator::IS_CONTROL_FLOW, true);
        traceGenerator::SetBit(&data->booleanValues,
                               traceGenerator::IS_INDIRECT_CONTROL_FLOW,
                               INS_IsIndirectControlFlow(*ins));
    }

    // INS_IsStandardMemop() returns false if this instruction has a memory
    // operand which has unconventional meaning; Returns true otherwise.
    bool isNonStandard = !INS_IsStandardMemop(*ins);

    traceGenerator::SetBit(&data->booleanValues,
                           traceGenerator::IS_NON_STANDARD_MEM_OP,
                           isNonStandard);
    traceGenerator::SetBit(&data->booleanValues, traceGenerator::IS_READ,
                           INS_IsMemoryRead(*ins));
    traceGenerator::SetBit(&data->booleanValues, traceGenerator::IS_READ2,
                           INS_HasMemoryRead2(*ins));
    traceGenerator::SetBit(&data->booleanValues, traceGenerator::IS_WRITE,
                           INS_IsMemoryWrite(*ins));

    for (unsigned long int i = 0; i < INS_MaxNumRRegs(*ins); ++i) {
        REG regValue = INS_RegR(*ins, i);
        if (regValue != REG_INVALID()) {
            data->readRegs[data->numReadRegs++] = regValue;
        }
    }

    for (unsigned long int i = 0; i < INS_MaxNumWRegs(*ins); ++i) {
        REG regValue = INS_RegW(*ins, i);
        if (regValue != REG_INVALID()) {
            data->writeRegs[data->numWriteRegs++] = regValue;
        }
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

        // This will make every function call from libgomp that have a
        // PAUSE instruction to be ignored.
        // I still not sure if this is fully corret.
        for (size_t i = 0; i < OMP_ignore.size(); ++i) {
            if (strcmp(traceRtnName, OMP_ignore[i]) == 0) {
                // has SPIN_LOCK
                return;
            }
        }
    }

    PIN_GetLock(&pinLock, tid);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_UINT32, staticTrace->bblCount, IARG_END);

        staticTrace->NewBBL(BBL_NumIns(bbl));
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            struct traceGenerator::DataINS data;
            createDataINS(&ins, &data);
            staticTrace->Write(&data);
            InstrumentMemoryOperations(&ins);
            staticTrace->instCount++;
        }
    }

    PIN_ReleaseLock(&pinLock);
    return;
}

VOID ImageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string completeImgPath = IMG_Name(img);
    size_t it = completeImgPath.find_last_of('/') + 1;

    // freed in Fini();
    imageName = strdup(&completeImgPath.c_str()[it]);

    unsigned int fileNameSize = strlen(imageName);
    assert(fileNameSize < MAX_IMAGE_NAME_SIZE &&
           "Trace file name is too long. Max of 64 chars");

    staticTrace = new traceGenerator::StaticTraceFile(imageName);

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();

            if (strcmp(name, "BeginInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)InitInstrumentation,
                               IARG_END);
            }

            if (strcmp(name, "EndInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
            }

            if (strcmp(name, "EnableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_AFTER,
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
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", staticTrace->bblCount);

    // Close static trace file
    delete staticTrace;
    free((void*)imageName);
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    if (access(traceGenerator::traceFolderPath.c_str(), F_OK) != 0) {
        mkdir(traceGenerator::traceFolderPath.c_str(),
              S_IRWXU | S_IRWXG | S_IROTH);
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
