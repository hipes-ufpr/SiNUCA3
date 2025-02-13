#include "pin.H"
#include "/home/fbc04/Programas/SiNUCA3/src/utils/logging.hpp"
#include "types_base.PH"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <iostream> // REMOVE 

#define BUFFER_SIZE 1048576
#define BBL_ID_BYTE_SIZE 2

// enumerations
enum flowType {
    NEXT_INST,
    BRANCH_SYSCALL,
    BRANCH_CALL,
    BRANCH_RETURN,
    BRANCH_UNCOND,
    BRANCH_COND
};

// globals
KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", 
                        "-1", "Maximum number of instructions to be traced");
FILE *staticTrace, *memoryTrace, *dynamicTrace;
static bool isInstrumentationOn;

static const size_t ADDR_SIZE = sizeof(ADDRINT);

// class definitions
class buffer {
  public:
    buffer() {numUsedBytes = 0;}
    ~buffer() {};
    char store[BUFFER_SIZE];
    size_t numUsedBytes;
};

buffer *staticBuffer, *memoryBuffer, *dynamicBuffer;

int usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n", KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

VOID loadBufToFile(buffer* bufObj, FILE* file) {
    char* buf = bufObj->store;
    size_t* used = &bufObj->numUsedBytes;
    size_t written = fwrite(buf, 1, *used, file);
    if (written < *used) {SINUCA3_ERROR_PRINTF("Buffer error");}
    *used=0;
}

VOID appendToDynamicTrace(UINT32 bblId) {
    char* buf = dynamicBuffer->store;
    size_t* used = &dynamicBuffer->numUsedBytes;

    std::memcpy(buf+*used, (void*)&bblId, BBL_ID_BYTE_SIZE);
    (*used)+=BBL_ID_BYTE_SIZE;
    if(BUFFER_SIZE-*used<sizeof(bblId)) {
        loadBufToFile(dynamicBuffer, dynamicTrace);    
    }
}

VOID initInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");
    isInstrumentationOn = true;
    std::cout << staticTrace << " " << dynamicTrace << std::endl;
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

    std::string name = INS_Mnemonic(*ins);
    std::memcpy(buf+*used, name.c_str(), name.size());
    (*used)+=name.size();
    USIZE opcode = 1;
    std::memcpy(buf+*used, (void*)&opcode, sizeof(opcode));
    (*used)+=sizeof(opcode);
    ADDRINT addr = INS_Address(*ins);
    std::memcpy(buf+*used, (void*)&addr, sizeof(addr));
    (*used)+=sizeof(addr);
    USIZE size = INS_Size(*ins);
    std::memcpy(buf+*used, (void*)&size, sizeof(size));
    (*used)+=sizeof(size);
    REG baseReg = INS_MemoryBaseReg(*ins);
    std::memcpy(buf+*used, (void*)&baseReg, sizeof(baseReg));
    (*used)+=sizeof(baseReg);
    REG indexReg = INS_MemoryIndexReg(*ins);
    std::memcpy(buf+*used, (void*)&indexReg, sizeof(indexReg));
    (*used)+=sizeof(indexReg);
    
    bool isPredicated = INS_IsPredicated(*ins);
    bool isPFetch = INS_IsPrefetch(*ins);
    bool isSyscall, isIndirect;
    enum flowType insFlow = NEXT_INST;
    if (INS_IsControlFlow(*ins) || (isSyscall=INS_IsSyscall(*ins))) {
        if (INS_IsCall(*ins)) {
            insFlow = BRANCH_CALL;
        } else if (INS_IsRet(*ins)) {
            insFlow = BRANCH_RETURN;
        } else if (isSyscall) {
            insFlow = BRANCH_SYSCALL;
        } else {
            if (INS_HasFallThrough(*ins)) {
                insFlow = BRANCH_COND;
            } else {
                insFlow = BRANCH_UNCOND;
            }
        }

        isIndirect = INS_IsIndirectControlFlow(*ins);
    }

    char byte=0;
    byte |= isPredicated;
    isPFetch >>= 1;
    byte |= isPFetch;
    int insFlowInt = static_cast<int>(insFlow);
    insFlowInt >>= 2;
    byte |= insFlowInt;
    if (insFlow != NEXT_INST) {
        isIndirect >>= 3;
        byte |= isIndirect;
    }
    std::memcpy(buf+*used, (void*)&byte, sizeof(byte));
    (*used)+=sizeof(byte);
}

VOID trace(TRACE trace, VOID *ptr) {
    char* buf = staticBuffer->store;
    size_t *used=&staticBuffer->numUsedBytes, bblInit;
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
        bblInit = (*used)++;
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins), numInstBbl++) {
            x86ToStaticBuf(&ins);
            if (BUFFER_SIZE - *used < 64) {
                loadBufToFile(staticBuffer, staticTrace);
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
        std::string staticName, dynamicName, memoryName;
        char subStr[32], extension[] = ".fft";

        size_t it = nameImg.find_last_of('/')+1;
        strncpy(subStr, &nameImg.c_str()[it], it-nameImg.size());
        staticName.append("static_");
        staticName.append(subStr);
        staticName.append(extension);
        staticTrace = fopen(staticName.c_str(), "wb");
        dynamicName.append("dynamic_");
        dynamicName.append(subStr);
        dynamicName.append(extension);
        dynamicTrace = fopen(dynamicName.c_str(), "wb");
        memoryName.append("memory_");
        memoryName.append(subStr);
        memoryName.append(extension);
        memoryTrace = fopen(memoryName.c_str(), "wb");
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
    
    staticBuffer = new buffer;
    dynamicBuffer = new buffer;
    memoryBuffer = new buffer;

    IMG_AddInstrumentFunction(imageLoad, NULL);
    TRACE_AddInstrumentFunction(trace, NULL);
    PIN_AddFiniFunction(fini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}