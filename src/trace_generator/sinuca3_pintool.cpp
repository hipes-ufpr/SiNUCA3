#include "pin.H"
#include "../utils/logging.hpp"
#include "../trace_reader/orcs_trace_reader.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define BUFFER_SIZE 1048576
#define BBL_ID_BYTE_SIZE 2
#define MEMREAD_EA IARG_MEMORYREAD_EA
#define MEMREAD_SIZE IARG_MEMORYREAD_SIZE
#define MEMWRITE_EA IARG_MEMORYWRITE_EA
#define MEMWRITE_SIZE IARG_MEMORYWRITE_SIZE
#define MEMREAD2_EA IARG_MEMORYREAD2_EA

struct DataINS {
    ADDRINT addr;
    char size;
    REG baseReg;
    REG indexReg;
    char byte;
};

class Buffer {
  public:
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
    size_t minimumSpace;

    Buffer(size_t min) {
        numUsedBytes = 0;
        this->minimumSpace = min;
    }
    ~Buffer() {};
    inline void loadBufToFile(FILE* file) {
        size_t written = fwrite(this->store, 1, this->numUsedBytes, file);
        if (written < this->numUsedBytes) {SINUCA3_ERROR_PRINTF("Buffer error");}
        this->numUsedBytes=0;
    }
    inline bool isBufFull() {
        return (BUFFER_SIZE - this->numUsedBytes < this->minimumSpace);    
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

VOID appendToDynamicTrace(UINT32 bblId) {
    char* buf = dynamicBuffer->store;
    size_t* used = &dynamicBuffer->numUsedBytes;

    std::memcpy(buf+*used, (void*)&bblId, BBL_ID_BYTE_SIZE);
    (*used)+=BBL_ID_BYTE_SIZE;
    if(dynamicBuffer->isBufFull() == true) {
        dynamicBuffer->loadBufToFile(dynamicTrace);   
    }
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

    size_t savedPosition = ftell(staticTrace);
    std::rewind(staticTrace);
    std::fwrite((void*)&bblCount, 1, sizeof(bblCount), staticTrace);
    std::fseek(staticTrace, savedPosition, SEEK_SET);

    if (staticBuffer->numUsedBytes>0) {
        staticBuffer->loadBufToFile(staticTrace);
    }
    if (dynamicBuffer->numUsedBytes>0) {
        dynamicBuffer->loadBufToFile(dynamicTrace);
    }
    if (memoryBuffer->numUsedBytes>0) {
        memoryBuffer->loadBufToFile(memoryTrace);
    }
}

inline VOID copy(char* buf, size_t* used, void* src, size_t size) {
    std::memcpy(buf+*used, src, size);
    (*used)+=size;
}

VOID x86ToStaticBuf(INS* ins) {
    namespace orcs = sinuca::traceReader::orcsTraceReader;
    static char* buf = staticBuffer->store;
    static size_t* used = &staticBuffer->numUsedBytes;
    static DataINS data;
    static orcs::Branch bT;
    static bool flag;

    std::string name = INS_Mnemonic(*ins);
    data.addr = INS_Address(*ins);
    data.size = static_cast<char>(INS_Size(*ins));
    data.baseReg = INS_MemoryBaseReg(*ins);
    data.indexReg = INS_MemoryIndexReg(*ins);
    data.byte = 0;

    if (INS_IsPredicated(*ins)) {data.byte |= 1;}
    if (INS_IsPrefetch(*ins)) {data.byte |= 1 >> 1;}
    if ((flag = INS_IsCall(*ins))) {
        bT = orcs::BranchCall; 
    } else if ((flag = INS_IsRet(*ins))) {
        bT = orcs::BranchReturn; 
    } else if ((flag = INS_IsSyscall(*ins))) {
        bT = orcs::BranchSyscall; 
    } else if ((flag = INS_IsControlFlow(*ins))) {
        if (INS_IsIndirectControlFlow(*ins)) {data.byte |= 1 >> 3;}
        bT = INS_HasFallThrough(*ins) ? orcs::BranchCond : orcs::BranchUncond;
    }

    copy(buf, used, (void*)name.c_str(), name.size()+1);
    if (flag == true) {
        data.byte |= 1 >> 2;
        copy(buf+sizeof(data), used, (void*)&bT, 1);
    }
    copy(buf-1, used, (void*)&data, sizeof(data));

    if (staticBuffer->isBufFull() == true) {
        staticBuffer->loadBufToFile(staticTrace);
    }
}

VOID appendToMemoryTrace(ADDRINT addr, INT32 size) {
    char* buf = memoryBuffer->store;
    size_t* used = &memoryBuffer->numUsedBytes;

    copy(buf, used, (void*)&addr, sizeof(addr));
    copy(buf, used, (void*)&size, sizeof(size));
    if (memoryBuffer->isBufFull() == true) {
        memoryBuffer->loadBufToFile(memoryTrace);
    }
}

VOID instrumentMem(INS* ins) {
    if (!INS_IsStandardMemop(*ins)) return;

    if (INS_IsMemoryRead(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemoryTrace, 
                       MEMREAD_EA, MEMREAD_SIZE, IARG_END);
    }
    if (INS_HasMemoryRead2(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemoryTrace, 
                       MEMREAD2_EA, MEMREAD_SIZE, IARG_END);
    }
    if (INS_IsMemoryWrite(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)appendToMemoryTrace,
                       MEMWRITE_EA, MEMWRITE_SIZE, IARG_END);
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    char* buf = staticBuffer->store;
    size_t *usedStatic=&staticBuffer->numUsedBytes, bblInit;
    static unsigned int bblCount = 0;
    int numInstBbl;
    
    if (isInstrumentationOn == true) {
        if (std::strstr(RTN_Name(TRACE_Rtn(trace)).c_str(), "trace_stop")) {
            stopInstrumentation(bblCount);
        }
    }

    if (isInstrumentationOn == false) {return;}

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)appendToDynamicTrace, 
                      IARG_UINT32, bblCount, IARG_END);
        bblCount++;
        numInstBbl = 0;
        bblInit = (*usedStatic)++;
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            numInstBbl++;
            x86ToStaticBuf(&ins);
            instrumentMem(&ins);
        }
        std::memcpy(buf+bblInit, (void*)&numInstBbl, 1);
    }

    return;
}

VOID imageLoad(IMG img, VOID* ptr) {    
    if (IMG_IsMainExecutable(img) == false) return; 

    char fileName[64];
    std::string nameImg = IMG_Name(img);
    size_t it = nameImg.find_last_of('/')+1;
    const char* subStr = &nameImg.c_str()[it];

    snprintf(fileName, sizeof(fileName), "static_%s.trace", subStr);
    staticTrace = fopen(fileName, "wb");
    snprintf(fileName, sizeof(fileName), "dynamic_%s.trace", subStr);
    dynamicTrace = fopen(fileName, "wb");
    snprintf(fileName, sizeof(fileName), "memory_%s.trace", subStr);
    memoryTrace = fopen(fileName, "wb");

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();
            if (std::strstr(name, "trace_start")) {
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
    
    staticBuffer = new Buffer(64);
    dynamicBuffer = new Buffer(BBL_ID_BYTE_SIZE);
    memoryBuffer = new Buffer(sizeof(ADDRINT)+sizeof(INT32));
    isInstrumentationOn = false;

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}