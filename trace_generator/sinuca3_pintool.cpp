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

static bool isInstrumentationOn;
static unsigned int numThreads = 0;

traceGenerator::TraceFileHandler tfHandler;

PIN_LOCK pinLock;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", "-1",
                        "Maximum number of instructions to be traced");

int Usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

void traceGenerator::TraceFileHandler::OpenNewTraceFile(
    traceGenerator::TraceType type, unsigned int threadID) {
    unsigned long fileNameSize = strlen(tfHandler.imgName);
    assert(fileNameSize > 0 && "tfHandler not initialized");

    fileNameSize += 128;
    char* fileName = (char*)alloca(fileNameSize);
    unsigned long filePathSize = strlen(traceFolderPath) + fileNameSize;
    char* filePath = (char*)alloca(filePathSize);

    if (access(traceGenerator::traceFolderPath, F_OK) != 0) {
        mkdir(traceGenerator::traceFolderPath, S_IRWXU | S_IRWXG | S_IROTH);
    }

    strcpy(filePath, traceFolderPath);

    switch (type) {
        case traceGenerator::TRACE_STATIC:
            snprintf(fileName, fileNameSize, "static_%s.trace",
                     tfHandler.imgName);
            tfHandler.staticTraceFile = fopen(strcat(filePath, fileName), "wb");
            tfHandler.staticBuffer = new Buffer;

            /* This space will be used to store the amount   *
             * of BBLs in the trace, number of instructions, *
             * and total number of threads                   */
            fseek(tfHandler.staticTraceFile, 3 * sizeof(unsigned int),
                  SEEK_SET);

            break;

        case traceGenerator::TRACE_DYNAMIC:
            snprintf(fileName, fileNameSize, "dynamic_%s_tid%d.trace",
                     tfHandler.imgName, threadID);

            /* push_back is problematic if threadIDs are not sequential */
            if (tfHandler.dynamicTraceFiles.size() <= threadID) {
                tfHandler.dynamicTraceFiles.resize(threadID);
                tfHandler.dynamicBuffers.resize(threadID);
            }

            tfHandler.dynamicTraceFiles[threadID] =
                fopen(strcat(filePath, fileName), "wb");
            tfHandler.dynamicBuffers[threadID] = new Buffer;
            tfHandler.dynamicBuffers[threadID]->minSpacePerOperation =
                sizeof(unsigned short);
            break;

        case traceGenerator::TRACE_MEMORY:
            snprintf(fileName, fileNameSize, "memory_%s_tid%d.trace",
                     tfHandler.imgName, threadID);

            /* push_back is problematic if threadIDs are not sequential */
            if (tfHandler.memoryTraceFiles.size() <= threadID) {
                tfHandler.memoryTraceFiles.resize(threadID);
                tfHandler.memoryBuffers.resize(threadID);
            }

            tfHandler.memoryTraceFiles[threadID] =
                fopen(strcat(filePath, fileName), "wb");
            tfHandler.memoryBuffers[threadID] = new Buffer;
            tfHandler.memoryBuffers[threadID]->minSpacePerOperation =
                sizeof(ADDRINT) + sizeof(INT32);
            break;
    }
}

void traceGenerator::TraceFileHandler::CloseTraceFile(
    traceGenerator::TraceType type, unsigned int threadID) {
    switch (type) {
        case traceGenerator::TRACE_STATIC:
            tfHandler.staticBuffer->LoadBufToFile(tfHandler.staticTraceFile,
                                                  false);
            fclose(tfHandler.staticTraceFile);
            delete tfHandler.staticBuffer;
            break;
        case traceGenerator::TRACE_DYNAMIC:
            tfHandler.dynamicBuffers[threadID]->LoadBufToFile(
                tfHandler.dynamicTraceFiles[threadID], true);
            fclose(tfHandler.dynamicTraceFiles[threadID]);
            delete tfHandler.dynamicBuffers[threadID];
            break;
        case traceGenerator::TRACE_MEMORY:
            tfHandler.memoryBuffers[threadID]->LoadBufToFile(
                tfHandler.memoryTraceFiles[threadID], true);
            fclose(tfHandler.memoryTraceFiles[threadID]);
            delete tfHandler.memoryBuffers[threadID];
            break;
    }
}

VOID ThreadStart(THREADID threadid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&pinLock, threadid);

    SINUCA3_DEBUG_PRINTF("New thread created! N => %d\n", threadid);
    numThreads++;

    tfHandler.OpenNewTraceFile(traceGenerator::TRACE_DYNAMIC, threadid);
    tfHandler.OpenNewTraceFile(traceGenerator::TRACE_MEMORY, threadid);

    PIN_ReleaseLock(&pinLock);
}

VOID ThreadFini(THREADID threadid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    PIN_GetLock(&pinLock, threadid);

    SINUCA3_DEBUG_PRINTF("A thread has finalized! N => %d\n", threadid);

    tfHandler.CloseTraceFile(traceGenerator::TRACE_DYNAMIC, threadid);
    tfHandler.CloseTraceFile(traceGenerator::TRACE_MEMORY, threadid);

    PIN_ReleaseLock(&pinLock);
}

VOID InitInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");

    isInstrumentationOn = true;
}

VOID StopInstrumentation(unsigned int bblCount, unsigned int instCount) {
    THREADID tid = PIN_ThreadId();

    SINUCA3_LOG_PRINTF("Trace ended from thread %d\n", tid);
    SINUCA3_LOG_PRINTF("End of tool instrumentation\n");
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", bblCount);
    isInstrumentationOn = false;

    rewind(tfHandler.staticTraceFile);
    fwrite(&numThreads, 1, sizeof(numThreads), tfHandler.staticTraceFile);
    fwrite(&bblCount, 1, sizeof(bblCount), tfHandler.staticTraceFile);
    fwrite(&instCount, 1, sizeof(instCount), tfHandler.staticTraceFile);
}

VOID AppendToDynamicTrace(UINT32 bblId) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.dynamicBuffers[tid]->store;
    size_t* used = &tfHandler.dynamicBuffers[tid]->numUsedBytes;

    traceGenerator::CopyToBuf(buf, used, &bblId, sizeof(bblId));
    if (tfHandler.dynamicBuffers[tid]->IsBufFull() == true) {
        tfHandler.dynamicBuffers[tid]->LoadBufToFile(
            tfHandler.dynamicTraceFiles[tid], true);
    }
}

UINT16 FillRegs(const INS* ins, unsigned short int* regs, unsigned int maxRegs,
                REG (*func)(INS, UINT32)) {
    unsigned short int reg;
    unsigned short int it;
    unsigned short int cont;

    for (it = 0, cont = 0; it < maxRegs; it++) {
        reg = static_cast<unsigned short int>(func(*ins, it));
        if (reg != REG_INVALID()) {
            regs[cont] = reg;
            cont++;
        }
    }

    return cont;
}

VOID AppendToMemTraceStd(ADDRINT addr, INT32 size) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.memoryBuffers[tid]->store;
    size_t* used = &tfHandler.memoryBuffers[tid]->numUsedBytes;
    static traceGenerator::DataMEM data;

    data.addr = addr;
    data.size = size;
    traceGenerator::CopyToBuf(buf, used, &data, sizeof(data));
}

VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* acessInfo) {
    THREADID tid = PIN_ThreadId();
    char* buf = tfHandler.memoryBuffers[tid]->store;
    size_t* used = &tfHandler.memoryBuffers[tid]->numUsedBytes;
    unsigned short numMemOps;
    unsigned short numReadings;
    unsigned short numWritings;
    PIN_MEM_ACCESS_INFO* memop;

    static traceGenerator::DataMEM readings[MAX_MEM_OPERATIONS];
    static traceGenerator::DataMEM writings[MAX_MEM_OPERATIONS];

    numMemOps = *(unsigned short*)(&acessInfo->numberOfMemops);
    tfHandler.memoryBuffers[tid]->minSpacePerOperation =
        (sizeof(numMemOps) + numMemOps * sizeof(*readings));
    if (tfHandler.memoryBuffers[tid]->IsBufFull()) {
        tfHandler.memoryBuffers[tid]->LoadBufToFile(
            tfHandler.memoryTraceFiles[tid], true);
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
    traceGenerator::CopyToBuf(buf, used, &numReadings, sizeof(numReadings));
    traceGenerator::CopyToBuf(buf, used, &numWritings, sizeof(numWritings));
    traceGenerator::CopyToBuf(buf, used, readings,
                              numReadings * sizeof(*readings));
    traceGenerator::CopyToBuf(buf, used, writings,
                              numWritings * sizeof(*writings));
}

VOID SetMinStdMemOp() {
    THREADID tid = PIN_ThreadId();
    tfHandler.memoryBuffers[tid]->minSpacePerOperation =
        (sizeof(traceGenerator::DataMEM) * 3);
    if (tfHandler.memoryBuffers[tid]->IsBufFull()) {
        tfHandler.memoryBuffers[tid]->LoadBufToFile(
            tfHandler.memoryTraceFiles[tid], true);
    }
}

VOID InstrumentMem(const INS* ins, traceGenerator::DataINS* data) {
    bool isRead;
    bool hasRead2;
    bool isWrite;
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        traceGenerator::SetBit(&data->booleanValues, 4, true);

        return;
    }

    isRead = INS_IsMemoryRead(*ins);
    hasRead2 = INS_HasMemoryRead2(*ins);
    isWrite = INS_IsMemoryWrite(*ins);
    if (isWrite || isRead || hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)SetMinStdMemOp, IARG_END);
    }

    if (isRead) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       MEMREAD_EA, MEMREAD_SIZE, IARG_END);
        traceGenerator::SetBit(&data->booleanValues, 5, true);
    }
    if (hasRead2) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       MEMREAD2_EA, MEMREAD_SIZE, IARG_END);
        traceGenerator::SetBit(&data->booleanValues, 6, true);
    }
    if (isWrite) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceStd,
                       MEMWRITE_EA, MEMWRITE_SIZE, IARG_END);
        traceGenerator::SetBit(&data->booleanValues, 7, true);
    }
}

VOID X86ToStaticBuf(const INS* ins) {
    char* buf = tfHandler.staticBuffer->store;
    size_t* used = &tfHandler.staticBuffer->numUsedBytes;
    traceGenerator::DataINS data;
    static unsigned short int readRegs[64];
    static unsigned short int writeRegs[64];

    unsigned int maxRRegs = INS_MaxNumRRegs(*ins);
    unsigned int maxWRegs = INS_MaxNumWRegs(*ins);
    std::string name = INS_Mnemonic(*ins);

    tfHandler.staticBuffer->minSpacePerOperation =
        sizeof(data) + name.size() + maxRRegs + maxWRegs;
    if (tfHandler.staticBuffer->IsBufFull()) {
        tfHandler.staticBuffer->LoadBufToFile(tfHandler.staticTraceFile, false);
    }

    data.addr = INS_Address(*ins);
    data.size = INS_Size(*ins);
    data.baseReg = INS_MemoryBaseReg(*ins);
    data.indexReg = INS_MemoryIndexReg(*ins);
    data.booleanValues = 0;

    if (INS_IsPredicated(*ins)) {
        traceGenerator::SetBit(&data.booleanValues, 0, true);
    }
    if (INS_IsPrefetch(*ins)) {
        traceGenerator::SetBit(&data.booleanValues, 1, true);
    }

    bool isControlFlow = INS_IsControlFlow(*ins);
    if (INS_IsSyscall(*ins)) {
        data.branchType = sinuca::BranchSyscall;
        isControlFlow = true;
    } else {
        if (isControlFlow) {
            if (INS_IsCall(*ins))
                data.branchType = sinuca::BranchCall;
            else if (INS_IsRet(*ins))
                data.branchType = sinuca::BranchReturn;
            else if (INS_HasFallThrough(*ins))
                data.branchType = sinuca::BranchCond;
            else
                data.branchType = sinuca::BranchUncond;
        }
    }

    if (isControlFlow) {
        traceGenerator::SetBit(&data.booleanValues, 2, true);
        if (INS_IsIndirectControlFlow(*ins)) {
            traceGenerator::SetBit(&data.booleanValues, 3, true);
        }
    }

    InstrumentMem(ins, &data);
    data.numReadRegs = FillRegs(ins, readRegs, maxRRegs, INS_RegR);
    data.numWriteRegs = FillRegs(ins, writeRegs, maxWRegs, INS_RegW);
    // Copy data
    traceGenerator::CopyToBuf(buf, used, &data, sizeof(data));
    // Copy read regs
    traceGenerator::CopyToBuf(buf, used, readRegs,
                              sizeof(*readRegs) * data.numReadRegs);
    // Copy write regs
    traceGenerator::CopyToBuf(buf, used, writeRegs,
                              sizeof(*writeRegs) * data.numWriteRegs);
    // Copy mnemonic
    traceGenerator::CopyToBuf(buf, used, (void*)name.c_str(), name.size() + 1);
}

VOID Trace(TRACE trace, VOID* ptr) {
    THREADID tid = PIN_ThreadId();

    char* buf = tfHandler.staticBuffer->store;
    size_t *usedStatic = &tfHandler.staticBuffer->numUsedBytes, bblInit;
    static unsigned int bblCount = 0;
    static unsigned int instCount = 0;
    unsigned short numInstBbl;

    if (isInstrumentationOn == false) {
        return;
    }

    PIN_GetLock(&pinLock, tid);

    if (strstr(RTN_Name(TRACE_Rtn(trace)).c_str(), "trace_stop")) {
        StopInstrumentation(bblCount, instCount);
        PIN_ReleaseLock(&pinLock);
        return;
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_UINT32, bblCount, IARG_END);
        bblCount++;
        numInstBbl = 0;
        bblInit = *usedStatic;
        (*usedStatic) += sizeof(numInstBbl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            numInstBbl++;
            instCount++;
            X86ToStaticBuf(&ins);
        }
        memcpy(buf + bblInit, &numInstBbl, sizeof(numInstBbl));
    }

    PIN_ReleaseLock(&pinLock);

    return;
}

VOID ImageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string completeImgPath = IMG_Name(img);
    size_t it = completeImgPath.find_last_of('/') + 1;
    const char* imgName = &completeImgPath.c_str()[it];

    SINUCA3_DEBUG_PRINTF("Image Loaded\n");

    unsigned int fileNameSize = strlen(imgName);
    assert(fileNameSize < MAX_IMAGE_NAME_SIZE &&
           "Trace file name is too long. Max of 64 chars");

    tfHandler.InitTraceFileHandler(imgName);
    tfHandler.OpenNewTraceFile(traceGenerator::TRACE_STATIC, 0);

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();
            if (strstr(name, "trace_start")) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
            }
            RTN_Close(rtn);
        }
    }
}

VOID Fini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");

    tfHandler.CloseTraceFile(traceGenerator::TRACE_STATIC, 0);
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    PIN_InitLock(&pinLock);

    isInstrumentationOn = false;

    IMG_AddInstrumentFunction(ImageLoad, NULL);
    TRACE_AddInstrumentFunction(Trace, NULL);
    PIN_AddFiniFunction(Fini, NULL);

    PIN_AddThreadStartFunction(ThreadStart, NULL);
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}
