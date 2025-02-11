#include <types.h>
#include "pin.H"
#include "/home/fbc23/Programas/SiNUCA3/src/utils/logging.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>

#define BUFFER_SIZE 1048576
#define BBL_ID_BYTE_SIZE 2

// globals
KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "o", "-1", "Number of instructions to be executed");
FILE *staticTrace, *memoryTrace, *dynamicTrace;
static bool isInstrumentationOn;

static const size_t OP_SIZE = sizeof(USIZE);

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

VOID appendToDynamicTrace(UINT32 bblId) {
    char* buf = dynamicBuffer->store;
    size_t* used = &dynamicBuffer->numUsedBytes;

    std::memcpy(buf+*used, (void*)&bblId, sizeof(bblId));
    *used+=sizeof(bblId);
    if(BUFFER_SIZE-*used<sizeof(bblId)) {
        size_t written = fwrite(buf, 1, *used, dynamicTrace);
        if (written < *used) {SINUCA3_ERROR_PRINTF("Buffer error");}
        *used=0;
    }
}

VOID trace(TRACE trace, VOID *ptr) {
    size_t *used=&staticBuffer->numUsedBytes;
    int *bblCount = static_cast<int*>(ptr);
    char *buf=staticBuffer->store;

    if (isInstrumentationOn == false) return;

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)appendToDynamicTrace, IARG_UINT32, *bblCount, IARG_END);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            USIZE size = INS_Size(ins);
            std::memcpy(buf+*used, (void*)&size, OP_SIZE);
            *used+=OP_SIZE;
            if (BUFFER_SIZE - *used < OP_SIZE) {
                size_t written = fwrite(buf, 1, *used, staticTrace);
                if (written < *used) {SINUCA3_ERROR_PRINTF("Buffer error");}
                *used=0;
            }
        }
        (*bblCount)++;
    }

    return;
}

VOID initInstrumentation() {
    SINUCA3_LOG_PRINTF("Start of tool instrumentation\n");
    isInstrumentationOn = true;
}

VOID stopInstrumentation() {
    SINUCA3_LOG_PRINTF("End of tool instrumentation\n");
    isInstrumentationOn = false;

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

VOID imageLoad(IMG img, VOID* ptr) {
    isInstrumentationOn = false;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();
            if (std::strstr(name, "trace_start")) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)initInstrumentation, IARG_END);
            } else if (std::strstr(name, "trace_stop")) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)stopInstrumentation, IARG_END);
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

    staticTrace = fopen("static_sinuca3.ftf", "wb");
    staticBuffer = new buffer;
    dynamicTrace = fopen("dynamic_sinuca3.ftf", "wb");
    dynamicBuffer = new buffer;
    memoryTrace = fopen("memory_sinuca3.ftf", "wb");
    memoryBuffer = new buffer;

    int bblCount = 0;
    IMG_AddInstrumentFunction(imageLoad, nullptr);
    TRACE_AddInstrumentFunction(trace, (VOID*)&bblCount);
    PIN_AddFiniFunction(fini, nullptr);

    // Never returns
    PIN_StartProgram();

    return 0;
}