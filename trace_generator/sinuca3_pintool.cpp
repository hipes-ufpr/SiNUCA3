#include "pin.H"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstddef>  // size_t
#include <cstring>  // memcpy
extern "C" {
    #include <sys/stat.h> // mkdir
    #include <unistd.h>   // access
}

#include "../src/utils/logging.hpp"
#include "./file_handler.hpp"

#include "sinuca3_pintool.hpp"
#include "../src/sinuca3.hpp"

// Set this to 1 to print all rotines
// that name begins with "gomp", case insensitive
// (Statically linking GOMP is recommended)
#define DEBUG_PRINT_GOMP_RNT 1

// When this is enabled, every thread will be instrumented;
static bool isGlobalInstrumentating;
// And this enable instrumentation per thread.
static std::vector<bool> isThreadInstrumentating;
static unsigned int numThreads = 0;

sinuca::traceGenerator::TraceFileHandler tfHandler;

static unsigned int bblCount = 0;
static unsigned int instCount = 0;

PIN_LOCK pinLock;

std::vector<const char*> OMP_ignore;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst",
                        "-1", "Maximum number of instructions to be traced");

int usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

inline void copy(char* buf, size_t* used, void* src, size_t size) {
    memcpy(buf + *used, src, size);
    (*used) += size;
}

inline void setBit(unsigned char* byte, int position, bool value) {
    if (value == true) {
        *byte |= (1 << position);
    } else {
        *byte &= 0xff - (1 << position);
    }
}

bool strStartsWithGomp(const char* s) {
    const char* prefix = "GOMP";
    for (size_t i = 0; prefix[i] != '\0'; ++i) {
        if (std::tolower(s[i]) != std::tolower(prefix[i])) {
            return false;
        }
    }
    return true;
}

void printRtnName(const char* s){
    SINUCA3_DEBUG_PRINTF("RNT called: %s\n", s);
}

void sinuca::traceGenerator::TraceFileHandler::openNewTraceFile(sinuca::traceGenerator::TraceType type, unsigned int tid){
    char fileName[96];
    char filePath[256];


    unsigned int fileNameSize = strlen(tfHandler.imgName);
    assert(fileNameSize > 0  && "tfHandler not initialized");

    if (access(sinuca::traceGenerator::traceFolderPath, F_OK) != 0) {
        mkdir(sinuca::traceGenerator::traceFolderPath, S_IRWXU | S_IRWXG | S_IROTH);
    }

    strcpy(filePath, sinuca::traceGenerator::traceFolderPath);

    switch (type) {

        case sinuca::traceGenerator::TRACE_STATIC:
            snprintf(fileName, sizeof(fileName), "static_%s.trace", tfHandler.imgName);
            tfHandler.staticTraceFile = fopen(strcat(filePath, fileName), "wb");
            tfHandler.staticBuffer = new Buffer;

            // This space will be used to store the amount of BBLs in the trace.
            fseek(tfHandler.staticTraceFile, 2*sizeof(unsigned int), SEEK_SET);

            break;

        case sinuca::traceGenerator::TRACE_DYNAMIC:
            snprintf(fileName, sizeof(fileName), "dynamic_%s_tid%d.trace", tfHandler.imgName, tid);

            if(tfHandler.dynamicTraceFiles.size() <= tid){
                tfHandler.dynamicTraceFiles.resize(tid * 2 + 1);
                tfHandler.dynamicBuffers.resize(tid * 2 + 1);
            }

            tfHandler.dynamicTraceFiles[tid] = fopen(strcat(filePath, fileName), "wb");
            tfHandler.dynamicBuffers[tid] = new Buffer;
            tfHandler.dynamicBuffers[tid]->minSpacePerOperation = sizeof(unsigned short);
            break;

        case sinuca::traceGenerator::TRACE_MEMORY:
            snprintf(fileName, sizeof(fileName), "memory_%s_tid%d.trace", tfHandler.imgName, tid);

            if(tfHandler.memoryTraceFiles.size() <= tid){
                tfHandler.memoryTraceFiles.resize(tid * 2 + 1);
                tfHandler.memoryBuffers.resize(tid * 2 + 1);
            }

            tfHandler.memoryTraceFiles[tid] = fopen(strcat(filePath, fileName), "wb");
            tfHandler.memoryBuffers[tid] = new Buffer;
            tfHandler.memoryBuffers[tid]->minSpacePerOperation = sizeof(ADDRINT) + sizeof(INT32);
            break;
    }
}

void sinuca::traceGenerator::TraceFileHandler::closeTraceFile(sinuca::traceGenerator::TraceType type, unsigned int tid){
    switch (type) {
        case sinuca::traceGenerator::TRACE_STATIC:
            tfHandler.staticBuffer->loadBufToFile(tfHandler.staticTraceFile);

            rewind(tfHandler.staticTraceFile);
            fwrite(&bblCount, 1, sizeof(bblCount), tfHandler.staticTraceFile);
            fwrite(&instCount, 1, sizeof(instCount), tfHandler.staticTraceFile);

            fclose(tfHandler.staticTraceFile);
            delete tfHandler.staticBuffer;
            break;
        case sinuca::traceGenerator::TRACE_DYNAMIC:
            tfHandler.dynamicBuffers[tid]->loadBufToFile(tfHandler.dynamicTraceFiles[tid]);
            fclose(tfHandler.dynamicTraceFiles[tid]);
            delete tfHandler.dynamicBuffers[tid];
            break;
        case sinuca::traceGenerator::TRACE_MEMORY:
            tfHandler.memoryBuffers[tid]->loadBufToFile(tfHandler.memoryTraceFiles[tid]);
            fclose(tfHandler.memoryTraceFiles[tid]);
            delete tfHandler.memoryBuffers[tid];
            break;
    }
}

VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v){
    PIN_GetLock(&pinLock, tid);

    SINUCA3_DEBUG_PRINTF("New thread created! N => %d\n", tid);
    numThreads++;

    if(isThreadInstrumentating.size() <= tid){
        isThreadInstrumentating.resize(tid * 2 + 1);
    }
    isThreadInstrumentating[tid] = isGlobalInstrumentating;

    tfHandler.openNewTraceFile(sinuca::traceGenerator::TRACE_DYNAMIC, tid);
    tfHandler.openNewTraceFile(sinuca::traceGenerator::TRACE_MEMORY, tid);

    PIN_ReleaseLock(&pinLock);
}

VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v){
    PIN_GetLock(&pinLock, tid);

    SINUCA3_DEBUG_PRINTF("A thread has finalized! N => %d\n", tid);

    tfHandler.closeTraceFile(sinuca::traceGenerator::TRACE_DYNAMIC, tid);
    tfHandler.closeTraceFile(sinuca::traceGenerator::TRACE_MEMORY , tid);

    numThreads--;

    PIN_ReleaseLock(&pinLock);
}

VOID initInstrumentationGlobally() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation globally.\n");
    isGlobalInstrumentating = true;
    std::fill(isThreadInstrumentating.begin(), isThreadInstrumentating.end(), true);
}

VOID stopInstrumentationGlobally() {
    SINUCA3_LOG_PRINTF("End of tool instrumentation globally.\n");
    isGlobalInstrumentating = false;
    std::fill(isThreadInstrumentating.begin(), isThreadInstrumentating.end(), false);
}

VOID initInstrumentationInThread(THREADID tid) {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation in thread %d.\n", tid);
    isThreadInstrumentating[tid] = true;
}

VOID stopInstrumentationInThread(THREADID tid) {
    SINUCA3_LOG_PRINTF("End of tool instrumentation in thread %d.\n", tid);
    isThreadInstrumentating[tid] = false;
}

VOID appendToDynamicTrace(UINT32 bblId) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.dynamicBuffers[tid]->store;
    size_t* used = &tfHandler.dynamicBuffers[tid]->numUsedBytes;

    copy(buf, used, &bblId, sizeof(bblId));
    if (tfHandler.dynamicBuffers[tid]->isBufFull() == true) {
        tfHandler.dynamicBuffers[tid]->loadBufToFile(tfHandler.dynamicTraceFiles[tid]);
    }
}

UINT16 fillRegs(const INS *ins, unsigned short int *regs,
                unsigned int maxRegs, REG (*func)(INS, UINT32)) {
    //---------------------------------------------------------//
    unsigned short int reg, it, cont;

    for (it = 0, cont = 0; it < maxRegs; it++) {
        reg = static_cast<unsigned short int>(func(*ins, it));
        if (reg != REG_INVALID()) {
            regs[cont] = reg;
            cont++;
        }
    }

    return cont;
}

VOID appendToMemTraceStd(ADDRINT addr, INT32 size) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.memoryBuffers[tid]->store;
    size_t* used = &tfHandler.memoryBuffers[tid]->numUsedBytes;
    static sinuca::traceGenerator::DataMEM data;

    data.addr = addr;
    data.size = size;
    copy(buf, used, &data, sizeof(data));
}

VOID appendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* acessInfo) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.memoryBuffers[tid]->store;
    size_t* used = &tfHandler.memoryBuffers[tid]->numUsedBytes;
    unsigned short numMemOps, numReadings, numWritings;
    PIN_MEM_ACCESS_INFO* memop;

    static sinuca::traceGenerator::DataMEM readings[MAX_MEM_OPERATIONS];
    static sinuca::traceGenerator::DataMEM writings[MAX_MEM_OPERATIONS];

    numMemOps = *(unsigned short*)(&acessInfo->numberOfMemops);
    tfHandler.memoryBuffers[tid]->minSpacePerOperation = (sizeof(numMemOps) + numMemOps*sizeof(*readings));
    if (tfHandler.memoryBuffers[tid]->isBufFull()) {
        tfHandler.memoryBuffers[tid]->loadBufToFile(tfHandler.memoryTraceFiles[tid]);
    }

    numReadings = numWritings = 0;
    for (unsigned short it = 0; it < numMemOps; it++) {
        memop = &acessInfo->memop[it];
        if (memop->memopType == PIN_MEMOP_LOAD) {
            readings[numReadings].addr = memop->memoryAddress;
            readings[numReadings].size = memop->bytesAccessed;
            numReadings++;
        } else {
            writings[numWritings].addr = memop->memoryAddress;
            writings[numWritings].size = memop->bytesAccessed;
            numWritings++;
        }
    }
    copy(buf, used, &numReadings, sizeof(numReadings));
    copy(buf, used, &numWritings, sizeof(numWritings));
    copy(buf, used, readings, numReadings * sizeof(*readings));
    copy(buf, used, writings, numWritings * sizeof(*writings));
}

VOID setMinStdMemOp() {
    THREADID tid = PIN_ThreadId();
    tfHandler.memoryBuffers[tid]->minSpacePerOperation = (sizeof(sinuca::traceGenerator::DataMEM)*3);
    if (tfHandler.memoryBuffers[tid]->isBufFull()) {
        tfHandler.memoryBuffers[tid]->loadBufToFile(tfHandler.memoryTraceFiles[tid]);
    }
}

VOID instrumentMem(const INS* ins, sinuca::traceGenerator::DataINS *data) {
    bool isRead, hasRead2, isWrite;
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        setBit(&data->booleanValues, 4, true);

        return;
    }

    isRead = INS_IsMemoryRead(*ins);
    hasRead2 = INS_HasMemoryRead2(*ins);
    isWrite = INS_IsMemoryWrite(*ins);
    if (isWrite || isRead || hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)setMinStdMemOp, IARG_END);
    }

    if (isRead) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd,
                        MEMREAD_EA, MEMREAD_SIZE, IARG_END);
        setBit(&data->booleanValues, 5, true);
    }
    if (hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd,
                        MEMREAD2_EA, MEMREAD_SIZE, IARG_END);
        setBit(&data->booleanValues, 6, true);
    }
    if (isWrite) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd,
                        MEMWRITE_EA, MEMWRITE_SIZE, IARG_END);
        setBit(&data->booleanValues, 7, true);
    }
}

VOID x86ToStaticBuf(const INS* ins) {
    sinuca::Branch branchType;
    char* buf = tfHandler.staticBuffer->store;
    size_t* used = & tfHandler.staticBuffer->numUsedBytes;
    sinuca::traceGenerator::DataINS data;
    static unsigned short int readRegs[64];
    static unsigned short int writeRegs[64];

    unsigned int maxRRegs = INS_MaxNumRRegs(*ins);
    unsigned int maxWRegs = INS_MaxNumWRegs(*ins);
    std::string name = INS_Mnemonic(*ins);
    tfHandler.staticBuffer->minSpacePerOperation = (name.size()+24+maxRRegs+maxWRegs);
    if (tfHandler.staticBuffer->isBufFull()) {
        tfHandler.staticBuffer->loadBufToFile(tfHandler.staticTraceFile);
    }

    data.addr = static_cast<long>(INS_Address(*ins));
    data.size = static_cast<unsigned char>(INS_Size(*ins));
    data.baseReg = static_cast<unsigned short>(INS_MemoryBaseReg(*ins));
    data.indexReg = static_cast<unsigned short>(INS_MemoryIndexReg(*ins));
    data.booleanValues = 0;

    if (INS_IsPredicated(*ins)) {
        setBit(&data.booleanValues, 0, true);
    }
    if (INS_IsPrefetch(*ins)) {
        setBit(&data.booleanValues, 1, true);
    }
    bool isBranch;
    if ((isBranch = INS_IsCall(*ins))) {
        branchType = sinuca::BranchCall;
    } else if ((isBranch = INS_IsRet(*ins))) {
        branchType = sinuca::BranchReturn;
    } else if ((isBranch = INS_IsSyscall(*ins))) {
        branchType = sinuca::BranchSyscall;
    } else if ((isBranch = INS_IsControlFlow(*ins))) {
        if (INS_HasFallThrough(*ins)) {
            branchType = sinuca::BranchCond;
        } else {
            branchType = sinuca::BranchUncond;
        }
    }

    if (isBranch == true) {
        setBit(&data.booleanValues, 2, true);
        if (INS_IsIndirectControlFlow(*ins)) {
            setBit(&data.booleanValues, 3, true);
        }
    }

    instrumentMem(ins, &data);
    data.numReadRegs = fillRegs(ins, readRegs, maxRRegs, INS_RegR);
    data.numWriteRegs = fillRegs(ins, writeRegs, maxWRegs, INS_RegW);
    // copy data
    copy(buf, used, &data, sizeof(data));
    // copy read regs
    copy(buf, used, readRegs, sizeof(*readRegs) * data.numReadRegs);
    // copy write regs
    copy(buf, used, writeRegs, sizeof(*writeRegs) * data.numWriteRegs);
    // copy mnemonic
    copy(buf, used, (void*)name.c_str(), name.size()+1);
    // copy branch type
    if (isBranch == true) {
        copy(buf, used, &branchType, sizeof(branchType));
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    THREADID tid = PIN_ThreadId();

    char* buf = tfHandler.staticBuffer->store;
    size_t *usedStatic=&tfHandler.staticBuffer->numUsedBytes, bblInit;
    unsigned short numInstBbl;
    RTN traceRtn = TRACE_Rtn(trace);

    if ( !isGlobalInstrumentating && !isThreadInstrumentating[tid] )
        return;

    if(RTN_Valid(traceRtn)){
        const char *traceRtnName = RTN_Name(traceRtn).c_str();
        if (strcmp(traceRtnName, "trace_stop_global") == 0) {
            stopInstrumentationGlobally();
            return;
        }

        if (strcmp(traceRtnName, "trace_stop_thread") == 0) {
            stopInstrumentationInThread(tid);
            return;
        }

        // This will make every function call from libgomp that have a
        // PAUSE instruction to be ignored.
        // I still not sure if this is fully corret.
        for(size_t i=0; i<OMP_ignore.size(); ++i){
            if (strcmp(traceRtnName, OMP_ignore[i]) == 0) {
                // has SPIN_LOCK
                return;
            }
        }
    }

    PIN_GetLock(&pinLock, tid);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)appendToDynamicTrace,
                      IARG_UINT32, bblCount, IARG_END);
        bblCount++;
        numInstBbl = 0;
        bblInit = *usedStatic;
        (*usedStatic) += sizeof(numInstBbl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            numInstBbl++;
            instCount++;
            x86ToStaticBuf(&ins);
        }
        memcpy(buf+bblInit, &numInstBbl, sizeof(numInstBbl));
    }

    PIN_ReleaseLock(&pinLock);

    return;
}

VOID imageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string completeImgPath = IMG_Name(img);
    size_t it = completeImgPath.find_last_of('/')+1;
    const char* imgName = &completeImgPath.c_str()[it];

    unsigned int fileNameSize = strlen(imgName);
    assert(fileNameSize < MAX_IMAGE_NAME_SIZE && "Trace file name is too long. Max of 64 chars");

    tfHandler.initTraceFileHandler(imgName);
    tfHandler.openNewTraceFile(sinuca::traceGenerator::TRACE_STATIC , 0);

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();

            if (strcmp(name, "trace_start_global") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)initInstrumentationGlobally, IARG_END);
            }

            if (strcmp(name, "trace_start_thread") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)initInstrumentationInThread, IARG_THREAD_ID, IARG_END);
            }

            #if DEBUG_PRINT_GOMP_RNT == 1
            if(strStartsWithGomp(name)){
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)printRtnName, IARG_PTR, name, IARG_END);
            }
            #endif

            RTN_Close(rtn);
        }
    }

}

VOID fini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", bblCount);

    tfHandler.closeTraceFile(sinuca::traceGenerator::TRACE_STATIC, 0);
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return usage();
    }

    PIN_InitLock(&pinLock);

    isGlobalInstrumentating = false;

    // All these functions have a PAUSE instruction (Spin-lock hint)
    OMP_ignore.push_back("gomp_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_cancel_end");
    OMP_ignore.push_back("gomp_mutex_lock_slow");
    OMP_ignore.push_back("GOMP_doacross_wait");
    OMP_ignore.push_back("GOMP_doacross_ull_wait");
    OMP_ignore.push_back("gomp_ptrlock_get_slow");
    OMP_ignore.push_back("gomp_sem_wait_slow");

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
