#include "pin.H"
#include <cstdio>
extern "C" {
    #include <sys/stat.h> // mkdir
    #include <unistd.h>   // access
}

#include "sinuca3_pintool.hpp"
#include "../sinuca3.hpp"

FILE *staticTrace, *memoryTrace, *dynamicTrace;
sinuca::traceGenerator::Buffer *staticBuffer, *memoryBuffer, *dynamicBuffer;
static bool isInstrumentationOn;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", 
                        "-1", "Maximum number of instructions to be traced");

inline void copy(char* buf, size_t* used, void* src, size_t size);
inline void setBit(unsigned char* byte, int position, bool value);

int usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n", 
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}


VOID initInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");
    isInstrumentationOn = true;
    fseek(staticTrace, 2*sizeof(unsigned int), SEEK_SET);
}

VOID stopInstrumentation(unsigned int bblCount, unsigned int instCount) {
    SINUCA3_LOG_PRINTF("End of tool instrumentation\n");
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", bblCount);
    isInstrumentationOn = false;

    if (staticBuffer->numUsedBytes>0) {
        staticBuffer->loadBufToFile(staticTrace);
    }
    if (dynamicBuffer->numUsedBytes>0) {
        dynamicBuffer->loadBufToFile(dynamicTrace);

    }
    if (memoryBuffer->numUsedBytes>0) {
        memoryBuffer->loadBufToFile(memoryTrace);
    }

    rewind(staticTrace);
    fwrite(&bblCount, 1, sizeof(bblCount), staticTrace);
    fwrite(&instCount, 1, sizeof(instCount), staticTrace);
}

VOID appendToDynamicTrace(UINT32 bblId) {
    char* buf = dynamicBuffer->store;
    size_t* used = &dynamicBuffer->numUsedBytes;

    copy(buf, used, &bblId, sizeof(bblId));
    if (dynamicBuffer->isBufFull() == true) {
        dynamicBuffer->loadBufToFile(dynamicTrace);   
    }
}

UINT16 fillRegs(const INS *ins, unsigned short int *regs,
    unsigned int maxRegs, REG (*func)(INS, UINT32)) {
    //---------------------------------------------//     
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
    char* buf = memoryBuffer->store;
    size_t* used = &memoryBuffer->numUsedBytes;
    static sinuca::traceGenerator::DataMEM data;

    data.addr = addr;
    data.size = size;
    copy(buf, used, &data, sizeof(data));
}

VOID appendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* acessInfo) {
    char* buf = memoryBuffer->store;
    size_t* used = &memoryBuffer->numUsedBytes;
    unsigned short numMemOps, numReadings, numWritings;
    PIN_MEM_ACCESS_INFO* memop;

    static sinuca::traceGenerator::DataMEM readings[MAX_MEM_OPERATIONS];
    static sinuca::traceGenerator::DataMEM writings[MAX_MEM_OPERATIONS];
    
    numMemOps = *(unsigned short*)(&acessInfo->numberOfMemops);
    memoryBuffer->setMinNecessary(sizeof(numMemOps) + numMemOps*sizeof(*readings));
    if (memoryBuffer->isBufFull()) {
        memoryBuffer->loadBufToFile(memoryTrace);
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
    memoryBuffer->setMinNecessary(sizeof(sinuca::traceGenerator::DataMEM)*3);
    if (memoryBuffer->isBufFull()) {
        memoryBuffer->loadBufToFile(memoryTrace);
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

VOID x86ToStaticBuf(const INS* ins, sinuca::traceGenerator::DataINS *data) {
    sinuca::Branch branchType;
    char* buf = staticBuffer->store;
    size_t* used = &staticBuffer->numUsedBytes;
    static unsigned short int readRegs[64];
    static unsigned short int writeRegs[64];
    
    unsigned int maxRRegs = INS_MaxNumRRegs(*ins);
    unsigned int maxWRegs = INS_MaxNumWRegs(*ins);
    std::string name = INS_Mnemonic(*ins);
    staticBuffer->setMinNecessary(name.size()+24+maxRRegs+maxWRegs);
    if (staticBuffer->isBufFull()) {
        staticBuffer->loadBufToFile(staticTrace);
    }

    data->addr = static_cast<long>(INS_Address(*ins));
    data->size = static_cast<unsigned char>(INS_Size(*ins));
    data->baseReg = static_cast<unsigned short int>(INS_MemoryBaseReg(*ins));
    data->indexReg = static_cast<unsigned short int>(INS_MemoryIndexReg(*ins));
    data->booleanValues = 0;

    if (INS_IsPredicated(*ins)) {
        setBit(&data->booleanValues, 0, true);
    }
    if (INS_IsPrefetch(*ins)) {
        setBit(&data->booleanValues, 1, true);
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
        setBit(&data->booleanValues, 2, true);
        if (INS_IsIndirectControlFlow(*ins)) {
            setBit(&data->booleanValues, 3, true);
        }
    }

    instrumentMem(ins, data);
    data->numReadRegs = fillRegs(ins, readRegs, maxRRegs, INS_RegR);
    data->numWriteRegs = fillRegs(ins, writeRegs, maxWRegs, INS_RegW);
    // copy data
    copy(buf, used, data, sizeof(*data));
    // copy read regs
    copy(buf, used, readRegs, sizeof(*readRegs)*data->numReadRegs);
    // copy write regs
    copy(buf, used, writeRegs, sizeof(*writeRegs)*data->numWriteRegs);
    // copy mnemonic
    copy(buf, used, (void*)name.c_str(), name.size()+1);
    // copy branch type
    if (isBranch == true) {
        copy(buf, used, &branchType, sizeof(branchType));
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    char* buf = staticBuffer->store;
    size_t *usedStatic=&staticBuffer->numUsedBytes, bblInit;
    static unsigned int bblCount = 0, instCount = 0;
    static sinuca::traceGenerator::DataINS data;
    unsigned short numInstBbl;

    if (isInstrumentationOn == false) {return;}

    if (std::strstr(RTN_Name(TRACE_Rtn(trace)).c_str(), "trace_stop")) {
        return stopInstrumentation(bblCount, instCount);
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)appendToDynamicTrace, 
                      IARG_UINT32, bblCount, IARG_END);
        bblCount++;
        numInstBbl = 0;
        bblInit = *usedStatic;
        (*usedStatic) += sizeof(numInstBbl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            numInstBbl++;
            instCount++;
            x86ToStaticBuf(&ins, &data);
        }
        std::memcpy(buf+bblInit, &numInstBbl, sizeof(numInstBbl));
    }

    return;
}

VOID imageLoad(IMG img, VOID* ptr) {    
    if (IMG_IsMainExecutable(img) == false) return; 

    char fileName[64], parentPath[256] = "../../trace/";
    std::string nameImg = IMG_Name(img);
    size_t it = nameImg.find_last_of('/')+1;
    const char* subStr = &nameImg.c_str()[it];
    size_t originalSize = strlen(parentPath);

    if (access(parentPath, F_OK) != 0) {
        mkdir(parentPath, S_IRWXU | S_IRWXG | S_IROTH);
    }
    
    snprintf(fileName, sizeof(fileName), "static_%s.trace", subStr);
    staticTrace = fopen(strcat(parentPath, fileName), "wb");
    snprintf(fileName, sizeof(fileName), "dynamic_%s.trace", subStr);
    parentPath[originalSize] = '\0';
    dynamicTrace = fopen(strcat(parentPath, fileName), "wb");
    snprintf(fileName, sizeof(fileName), "memory_%s.trace", subStr);
    parentPath[originalSize] = '\0';
    memoryTrace = fopen(strcat(parentPath, fileName), "wb");

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();
            if (strstr(name, "trace_start")) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)initInstrumentation, IARG_END);
            }
            RTN_Close(rtn);
        }
    }
}

VOID fini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    fclose(staticTrace);
    fclose(dynamicTrace);
    fclose(memoryTrace);
    free(staticBuffer);
    free(dynamicBuffer);
    free(memoryBuffer);
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return usage();
    }
    
    staticBuffer = new sinuca::traceGenerator::Buffer(0);
    dynamicBuffer = new sinuca::traceGenerator::Buffer(sizeof(unsigned short));
    memoryBuffer = new sinuca::traceGenerator::Buffer(sizeof(ADDRINT)+sizeof(INT32));
    isInstrumentationOn = false;

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
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