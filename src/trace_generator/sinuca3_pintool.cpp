#include "pin.H"
#include <cstddef>
#include <cstdio>
#include <cstring>
extern "C" {
    #include <sys/stat.h> // mkdir
    #include <unistd.h>   // access
}

#include "sinuca3_pintool.hpp"
#include "../utils/logging.hpp"

#define MEMREAD_EA IARG_MEMORYREAD_EA
#define MEMREAD_SIZE IARG_MEMORYREAD_SIZE
#define MEMWRITE_EA IARG_MEMORYWRITE_EA
#define MEMWRITE_SIZE IARG_MEMORYWRITE_SIZE
#define MEMREAD2_EA IARG_MEMORYREAD2_EA

struct Buffer {
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
    size_t minSpaceNecessary;

    Buffer(size_t min) {
        numUsedBytes = 0;
        this->minSpaceNecessary = min;
    }
    
    inline void loadBufToFile(FILE* file) {
        fwrite(&numUsedBytes, 1, sizeof(size_t), file);
        size_t written = fwrite(this->store, 1, this->numUsedBytes, file);
        if (written < this->numUsedBytes) {SINUCA3_ERROR_PRINTF("Buffer error");}
        this->numUsedBytes = 0;
    }

    inline bool isBufFull() {
        return ((BUFFER_SIZE) - this->numUsedBytes < this->minSpaceNecessary);    
    }

    inline void setMinNecessary(size_t num) {
        this->minSpaceNecessary = num;
    }
};

// globals
KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", 
                        "-1", "Maximum number of instructions to be traced");
FILE *staticTrace, *memoryTrace, *dynamicTrace;
Buffer *staticBuffer, *memoryBuffer, *dynamicBuffer;
static bool isInstrumentationOn;
static const size_t ADDR_SIZE = sizeof(ADDRINT);

int usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n", KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}


VOID initInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");
    isInstrumentationOn = true;
    fseek(staticTrace, 4, SEEK_SET);
}

VOID stopInstrumentation(unsigned int bblCount) {
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

    std::rewind(staticTrace);
    std::fwrite(&bblCount, 1, sizeof(bblCount), staticTrace);
}

VOID appendToDynamicTrace(UINT32 bblId) {
    char* buf = dynamicBuffer->store;
    size_t* used = &dynamicBuffer->numUsedBytes;

    copy(buf, used, &bblId, sizeof(unsigned short));
    if(dynamicBuffer->isBufFull() == true) {
        dynamicBuffer->loadBufToFile(dynamicTrace);   
    }
}

VOID copyRegs(const INS *ins, unsigned int maxRegs, REG (*func)(INS, UINT32)) {
    char *buf = staticBuffer->store;
    size_t *used = &staticBuffer->numUsedBytes;
    size_t saveUsed = *used;
    unsigned short int reg, it, cont;

    (*used) += sizeof(cont);
    for (it = 0, cont = 0; it < maxRegs; it++) {
        reg = static_cast<unsigned short int>(func(*ins, it));
        if (reg != REG_INVALID()) {
            copy(buf, used, &reg, sizeof(reg));
            cont++;
        }
    }
    memcpy(buf+saveUsed, &it, sizeof(cont));
}

VOID x86ToStaticBuf(const INS* ins, DataINS *data) {
    namespace reader = sinuca::traceReader::sinuca3TraceReader;
    char* buf = staticBuffer->store;
    size_t* used = &staticBuffer->numUsedBytes;
    
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
        setBit(&data->booleanValues, 0);
    }
    if (INS_IsPrefetch(*ins)) {
        setBit(&data->booleanValues, 1);
    }
    bool flag;
    if ((flag = INS_IsCall(*ins))) {
        data->branchType = reader::BranchCall; 
    } else if ((flag = INS_IsRet(*ins))) {
        data->branchType = reader::BranchReturn; 
    } else if ((flag = INS_IsSyscall(*ins))) {
        data->branchType = reader::BranchSyscall; 
    } else if ((flag = INS_IsControlFlow(*ins))) {
        if (INS_HasFallThrough(*ins)) {
            data->branchType = reader::BranchCond;
        } else {
            data->branchType = reader::BranchUncond;
        }
    }
    if (flag == true) {
        setBit(&data->booleanValues, 2);
        if (INS_IsIndirectControlFlow(*ins)) {
            setBit(&data->booleanValues, 3);
        }
    }

    // copy data
    copy(buf, used, data, SIZE_DATA_INS);

    // copy branch type
    if (flag == true) {
        copy(buf, used, &data->branchType, sizeof(data->branchType));
    }
    // copy mnemonic
    copy(buf, used, (void*)name.c_str(), name.size()+1);
    // copy read regs
    copyRegs(ins, maxRRegs, INS_RegR);
    // copy write regs
    copyRegs(ins, maxWRegs, INS_RegW);
}

VOID appendToMemTraceStd(ADDRINT addr, INT32 size) {
    char* buf = memoryBuffer->store;
    size_t* used = &memoryBuffer->numUsedBytes;
    static DataMEM data;

    data.addr = static_cast<long>(addr);
    data.size = static_cast<int>(size);
    copy(buf, used, &data, sizeof(data));

    if (memoryBuffer->isBufFull()) {
        memoryBuffer->loadBufToFile(memoryTrace);
    }
}

VOID appendToMemTraceNonStd() {}

VOID instrumentMem(INS* ins, DataINS *data) {
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceNonStd, 
                       MEMREAD_EA, MEMREAD_SIZE, IARG_END);
        
        return;
    }

    if (INS_IsMemoryRead(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd, 
                       MEMREAD_EA, MEMREAD_SIZE, IARG_END);
    }
    if (INS_HasMemoryRead2(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd, 
                       MEMREAD2_EA, MEMREAD_SIZE, IARG_END);
    }
    if (INS_IsMemoryWrite(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemTraceStd,
                       MEMWRITE_EA, MEMWRITE_SIZE, IARG_END);
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    char* buf = staticBuffer->store;
    size_t *usedStatic=&staticBuffer->numUsedBytes, bblInit;
    static unsigned int bblCount = 0;
    static DataINS data;
    unsigned short numInstBbl;

    if (isInstrumentationOn == false) {return;}

    if (std::strstr(RTN_Name(TRACE_Rtn(trace)).c_str(), "trace_stop")) {
        stopInstrumentation(bblCount);
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
            x86ToStaticBuf(&ins, &data);
            instrumentMem(&ins, &data);
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

    SINUCA3_DEBUG_PRINTF("SIZE OF DataINS => %lu\n", sizeof(DataINS));
    SINUCA3_DEBUG_PRINTF("SIZE OF DataMEM => %lu\n", sizeof(DataMEM));
    
    staticBuffer = new Buffer(0);
    dynamicBuffer = new Buffer(sizeof(unsigned short));
    memoryBuffer = new Buffer(sizeof(ADDRINT)+sizeof(INT32));
    isInstrumentationOn = false;

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}