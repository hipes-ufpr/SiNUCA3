#include "pin.H"
#include "/home/fbc04/Programas/SiNUCA3/src/utils/logging.hpp"
#include "../trace_reader/orcs_trace_reader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream> // REMOVE

#define BUFFER_SIZE 1048576
#define BBL_ID_BYTE_SIZE 2

// class definitions
class buffer {
  public:
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
    size_t minimumSpace;

    buffer(size_t min) {
        numUsedBytes = 0;
        this->minimumSpace = min;
    }
    ~buffer() {};
    void loadBufToFile(FILE* file) {
        size_t written = fwrite(this->store, 1, this->numUsedBytes, file);
        if (written < this->numUsedBytes) {SINUCA3_ERROR_PRINTF("Buffer error");}
        this->numUsedBytes=0;
    }
    bool isBufFull() {
        return (BUFFER_SIZE - this->numUsedBytes < this->minimumSpace);    
    }
};

// globals
KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", 
                        "-1", "Maximum number of instructions to be traced");
FILE *staticTrace, *memoryTrace, *dynamicTrace;
buffer *staticBuffer, *memoryBuffer, *dynamicBuffer;
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
    if(dynamicBuffer->isBufFull()) {
        dynamicBuffer->loadBufToFile(dynamicTrace);   
    }
}

VOID initInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");
    isInstrumentationOn = true;
    fseek(staticTrace, 4, SEEK_SET);
}

VOID stopInstrumentation(int bblCount) {
    SINUCA3_LOG_PRINTF("End of tool instrumentation\n");
    isInstrumentationOn = false;

    size_t savedPosition = ftell(staticTrace);
    fseek(staticTrace, 0, SEEK_SET);
    fwrite((void*)&bblCount, 1, sizeof(bblCount), staticTrace);
    fseek(staticTrace, savedPosition, SEEK_SET);

    char *buf=staticBuffer->store;
    size_t *used=&staticBuffer->numUsedBytes; 
    if (*used>0) {
        fwrite(buf, 1, *used, staticTrace);
    }

    buf=dynamicBuffer->store;
    used=&dynamicBuffer->numUsedBytes;
    if (*used>0) {
        fwrite(buf, 1, *used, dynamicTrace);
    }
}

VOID x86ToStaticBuf(INS* ins) {
    char* buf = staticBuffer->store;
    size_t* used = &staticBuffer->numUsedBytes;
    namespace orcs = sinuca::traceReader::orcsTraceReader;

    // get mnemonic
    std::string name = INS_Mnemonic(*ins);
    std::memcpy(buf+*used, name.c_str(), name.size());
    (*used)+=name.size();
    // get address
    ADDRINT addr = INS_Address(*ins);
    std::memcpy(buf+*used, (void*)&addr, sizeof(addr));
    (*used)+=sizeof(addr);
    // get size
    char size = static_cast<char>(INS_Size(*ins));
    std::memcpy(buf+*used, (void*)&size, sizeof(size));
    (*used)+=sizeof(size);
    // get memory base reg
    REG baseReg = INS_MemoryBaseReg(*ins);
    std::memcpy(buf+*used, (void*)&baseReg, sizeof(baseReg));
    (*used)+=sizeof(baseReg);
    // get memory index reg
    REG indexReg = INS_MemoryIndexReg(*ins);
    std::memcpy(buf+*used, (void*)&indexReg, sizeof(indexReg));
    (*used)+=sizeof(indexReg);
    
    // conversion from bool to char may only guarantee one bit 0 or 1
    // the rest being crap
    char byte=0;
    byte |= INS_IsPredicated(*ins) ? 0x1 : 0x0;
    byte |= ((INS_IsPrefetch(*ins) ? 0x1 : 0x0) >> 1);

    bool isBranch = (INS_IsControlFlow(*ins) || INS_IsSyscall(*ins));
    orcs::Branch branchType;
    if (isBranch == true) {
        byte |= (0x1 >> 2);
        byte |= ((INS_IsIndirectControlFlow(*ins) ? 0x1 : 0x0) >> 3);
        if (INS_IsCall(*ins)) {
            branchType = orcs::BranchCall;
        } else if (INS_IsRet(*ins)) {
            branchType = orcs::BranchReturn;
        } else if (INS_IsSyscall(*ins)) {
            branchType = orcs::BranchSyscall;
        } else if (INS_HasFallThrough(*ins)) {
            branchType = orcs::BranchCond;
        } else {
            branchType = orcs::BranchUncond;
        }
    }
    
    std::memcpy(buf+*used, (void*)&byte, sizeof(byte));
    (*used)+=sizeof(byte);

    if (isBranch == true) {
        char branchTypeByte = static_cast<char>(branchType);
        std::memcpy(buf+*used, (void*)&branchTypeByte, sizeof(branchTypeByte));
        (*used)+=sizeof(branchTypeByte);
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    char* buf = staticBuffer->store;
    size_t *usedStatic=&staticBuffer->numUsedBytes, bblInit;
    static int bblCount = 1;
    int numInstBbl;
    
    if (isInstrumentationOn == true) {
        if (std::strstr(RTN_Name(TRACE_Rtn(trace)).c_str(), "trace_stop")) {
            stopInstrumentation(bblCount);
        }
    }

    if (isInstrumentationOn == false) {return;}

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)appendToDynamicTrace, IARG_UINT32, bblCount, IARG_END);
        numInstBbl = 0;
        bblInit = (*usedStatic)++;
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins), numInstBbl++) {
            x86ToStaticBuf(&ins);
            if (staticBuffer->isBufFull()) {
                staticBuffer->loadBufToFile(staticTrace);
            }

            if (!INS_IsStandardMemop(ins)) continue;
            if (INS_IsMemoryRead(ins)) {

            }
            if (INS_HasMemoryRead2(ins)) {

            }
            if (INS_IsMemoryWrite(ins)) {

            }
        }
        std::memcpy(buf+bblInit, (void*)&numInstBbl, 1);
        bblCount++;
    }

    return;
}

VOID imageLoad(IMG img, VOID* ptr) {
    isInstrumentationOn = false;
    
    if (IMG_IsMainExecutable(img) == true) {
        std::string nameImg = IMG_Name(img);
        char fileName[64], subStr[32];

        size_t it = nameImg.find_last_of('/')+1;
        strncpy(subStr, &nameImg.c_str()[it], nameImg.size()-it);
        snprintf(fileName, sizeof(fileName), "static_%s.fft", subStr);
        staticTrace = fopen(fileName, "wb");
        snprintf(fileName, sizeof(fileName), "dynamic_%s.fft", subStr);
        dynamicTrace = fopen(fileName, "wb");
        snprintf(fileName, sizeof(fileName), "memory_%s.fft", subStr);
        memoryTrace = fopen(fileName, "wb");
    }


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
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return usage();
    }
    
    staticBuffer = new buffer(64);
    dynamicBuffer = new buffer(BBL_ID_BYTE_SIZE);
    memoryBuffer = new buffer(sizeof(long));

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}