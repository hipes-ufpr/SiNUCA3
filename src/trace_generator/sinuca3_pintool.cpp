#include <types.h>
#include "pin.H"
#include "../utils/logging.hpp"
#include "types_base.PH"
#include "types_vmapi.PH"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctypes>

#define BUFFER_SIZE 1048576
#define BBL_ID_BYTE_SIZE 2

// globals
KNOB<int> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "o", -1, "Number of instructions to be executed");
FILE *static_trace, *memory_trace, *dynamic_trace;
class buffer *staticBuffer, *memoryBuffer, *dynamicBuffer;
bool isInstrumentationOn;

// class definitions
class buffer {
  public:
    buffer() {numUsedBytes = 0;}
    ~buffer() {};
    char store[BUFFER_SIZE];
    int numUsedBytes;
};

int usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n", KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

VOID appendToDynamicTrace(UINT32 bblId) {

}

VOID trace(TRACE trace, VOID *ptr) {
    if (isInstrumentationOn == false) return;

    int *bblCount = static_cast<int*>(ptr);
    LEVEL_BASE::OPCODE opcode;
    int opcodeSize = sizeof(opcode);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)appendToDynamicTrace, IARG_UINT32, *bblCount, IARG_END);
        std::memcpy(static_cast<void*>(&staticBuffer->store[staticBuffer->numUsedBytes]), static_cast<void*>(bblCount), BBL_ID_BYTE_SIZE);
        staticBuffer->numUsedBytes += BBL_ID_BYTE_SIZE;
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            opcode = INS_Opcode(ins);
            std::memcpy(static_cast<void*>(&staticBuffer->store[staticBuffer->numUsedBytes]), static_cast<void*>(&opcode), opcodeSize);
            staticBuffer->numUsedBytes += opcodeSize;
            if (BUFFER_SIZE - staticBuffer->numUsedBytes < opcodeSize + BBL_ID_BYTE_SIZE) {
                size_t written = fwrite(staticBuffer->store, 1, BUFFER_SIZE - staticBuffer->numUsedBytes, static_trace);
                if (written < BUFFER_SIZE) {
                    SINUCA3_ERROR_PRINTF("Buffer error");
                }
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
    PIN_Detach();
}

VOID imageLoad(IMG img, VOID* ptr) {
    isInstrumentationOn = false;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            if (RTN_Name(rtn) == "TRACE_START") {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)initInstrumentation, IARG_END);
            } else if (RTN_Name(rtn) == "TRACE_END") {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)stopInstrumentation, IARG_END);
            }
        }
    }
}

VOID fini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
}

VOID detach(VOID* v) {
    SINUCA3_LOG_PRINTF("End of tool instrumentation\n");
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return usage();
    }

    static_trace = fopen("static_sinuca3.ftf", "wb");
    staticBuffer = new buffer;
    dynamic_trace = fopen("dynamic_sinuca3.ftf", "wb");
    dynamicBuffer = new buffer;
    memory_trace = fopen("memory_sinuca3.ftf", "wb");
    memoryBuffer = new buffer;

    int bblCount = 0;
    IMG_AddInstrumentFunction(imageLoad, nullptr);
    TRACE_AddInstrumentFunction(trace, static_cast<VOID*>(&bblCount));
    PIN_AddDetachFunction(detach, nullptr);
    PIN_AddFiniFunction(fini, nullptr);

    // Never returns
    PIN_StartProgram();

    return 0;
}