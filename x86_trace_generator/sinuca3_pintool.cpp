//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file sinuca3_pintool.cpp
 * @brief Implementation of the SiNUCA3 x86_64 tracer based on Intel Pin.
 */

#include <cassert>  // assert
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "pin.H"
#include "tracer/sinuca/file_handler.hpp"
#include "types_base.PH"
#include "types_core.PH"
#include "types_vmapi.PH"
#include "utils/logging.hpp"

extern "C" {
#include <sys/stat.h>  // mkdir
#include <unistd.h>    // access
}

#include <sinuca3.hpp>
#include <utils/dynamic_trace_writer.hpp>
#include <utils/memory_trace_writer.hpp>
#include <utils/static_trace_writer.hpp>

const int MEMREAD_EA = IARG_MEMORYREAD_EA;
const int MEMREAD_SIZE = IARG_MEMORYREAD_SIZE;
const int MEMWRITE_EA = IARG_MEMORYWRITE_EA;
const int MEMWRITE_SIZE = IARG_MEMORYWRITE_SIZE;
const int MEMREAD2_EA = IARG_MEMORYREAD2_EA;

/**
 * @brief Set this to 1 to print all rotines that name begins with "gomp",
 * case insensitive (Statically linking GOMP is recommended).
 */
const int DEBUG_PRINT_GOMP_RNT = 0;

/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief When enabled, this flag allows the pintool to record all instructions
 * static info into a static trace file, and allows the instrumentation phase
 * (e.g., in OnTrace) to insert analysis code into the target program. However,
 * the inserted analysis code will only execute at runtime if the corresponding
 * thread has its threadInstrumentationEnabled flag set to true.
 */
static bool isInstrumentating;
/**
 * @note Instrumentation is the process of deciding where and what code should
 * be inserted into the target program, while analysis refers to the code that
 * is actually executed at those insertion points to gather information about
 * the program’s behavior.
 *
 * @brief Indicates, for each thread, whether it is allowed to execute
 * previously inserted analysis code.
 *
 * This flag does not control the instrumentation process itself (i.e., whether
 * code is inserted into the target program), but rather whether a specific
 * thread is permitted to execute that analysis code at runtime.
 *
 * The insertion of analysis code occurs only when the global
 * `isInstrumentating` flag is enabled. Later, during program execution, the
 * inserted code will only be executed by a thread if its corresponding entry in
 * this vector is set to true.
 *
 * When executed, the analysis code records dynamic and memory trace information
 * into files associated with the executing thread.
 */
static std::vector<bool> isThreadAnalysisEnabled;

char* imageName = NULL;
const char* folderPath;
bool wasInstrumented =
    false;  // true, if program was been instrumented once at least.

sinucaTracer::StaticTraceFile* staticTrace;
std::vector<sinucaTracer::DynamicTraceFile*> dynamicTraces;
std::vector<sinucaTracer::MemoryTraceFile*> memoryTraces;

PIN_LOCK pinLock;
std::vector<const char*> OMP_ignore;

KNOB<INT> KnobNumberIns(KNOB_MODE_WRITEONCE, "pintool", "number_max_inst", "-1",
                        "Maximum number of instructions to be traced");
KNOB<std::string> KnobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Path to store the trace files");
KNOB<BOOL> KnobForceInstrumentation(
    KNOB_MODE_WRITEONCE, "pintool", "f", "0",
    "Force instrumentation for the entire execution for all created threads");

KNOB<std::string> KnobIntrinsics(KNOB_MODE_APPEND, "pintool", "i", "",
                                 "Intrinsic instructions in the format "
                                 "name:readregs:writeregs:numreads:numwrites");

struct IntrinsicInfo {
    char name[sinucaTracer::MAX_INSTRUCTION_NAME_LENGTH];
    REG read[sinucaTracer::MAX_REG_OPERANDS];
    REG write[sinucaTracer::MAX_REG_OPERANDS];
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
    unsigned char isRead : 1;
    unsigned char isRead2 : 1;
    unsigned char isWrite : 1;
};

std::vector<IntrinsicInfo> intrinsics;

int Usage() {
    SINUCA3_LOG_PRINTF("Tool knob summary: %s\n",
                       KNOB_BASE::StringKnobSummary().c_str());
    return 1;
}

bool StrStartsWithGomp(const char* s) {
    const char* prefix = "gomp";
    for (int i = 0; prefix[i] != '\0'; ++i) {
        if (std::tolower(s[i]) != prefix[i]) {
            return false;
        }
    }
    return true;
}

void InitGompHandling() {
    // All these functions have a PAUSE instruction (Spin-lock hint)
    OMP_ignore.push_back("gomp_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_end");
    OMP_ignore.push_back("gomp_team_barrier_wait_cancel_end");
    OMP_ignore.push_back("gomp_mutex_lock_slow");
    OMP_ignore.push_back("GOMP_doacross_wait");
    OMP_ignore.push_back("GOMP_doacross_ull_wait");
    OMP_ignore.push_back("gomp_ptrlock_get_slow");
    OMP_ignore.push_back("gomp_sem_wait_slow");
}

void DebugPrintRtnName(const char* s, THREADID tid) {
    PIN_GetLock(&pinLock, tid);
    SINUCA3_DEBUG_PRINTF("RNT called: %s\n", s);
    PIN_ReleaseLock(&pinLock);
}

VOID InitInstrumentation() {
    if (isInstrumentating) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("Start of tool instrumentation block\n");
    isInstrumentating = true;
    wasInstrumented = true;
    PIN_ReleaseLock(&pinLock);
}

VOID StopInstrumentation() {
    if (!isInstrumentating || KnobForceInstrumentation.Value()) return;
    PIN_GetLock(&pinLock, PIN_ThreadId());
    SINUCA3_LOG_PRINTF("End of tool instrumentation block\n");
    isInstrumentating = false;
    PIN_ReleaseLock(&pinLock);
}

VOID EnableInstrumentationInThread(THREADID tid) {
    if (isThreadAnalysisEnabled[tid]) return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Enabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = true;
    PIN_ReleaseLock(&pinLock);
}

VOID DisableInstrumentationInThread(THREADID tid) {
    if (!isThreadAnalysisEnabled[tid] || KnobForceInstrumentation.Value())
        return;
    PIN_GetLock(&pinLock, tid);
    SINUCA3_LOG_PRINTF("Disabling tool instrumentation in thread [%d]\n", tid);
    isThreadAnalysisEnabled[tid] = false;
    PIN_ReleaseLock(&pinLock);
}

VOID AppendToDynamicTrace(UINT32 bblId, UINT32 numInst) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    dynamicTraces[tid]->PrepareId(bblId);
    dynamicTraces[tid]->AppendToBufferId();
    dynamicTraces[tid]->IncTotalExecInst(numInst);
}

VOID AppendToMemTraceStd(ADDRINT addr, UINT32 size) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataStdMemAccess(addr, size);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

VOID AppendToMemTraceNonStd(PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    THREADID tid = PIN_ThreadId();
    if (!isThreadAnalysisEnabled[tid]) return;
    memoryTraces[tid]->PrepareDataNonStdAccess(accessInfo);
    memoryTraces[tid]->AppendToBufferLastMemoryAccess();
}

VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    PIN_GetLock(&pinLock, tid);
    SINUCA3_DEBUG_PRINTF("New thread created! N => %d (%s)\n", tid, imageName);
    staticTrace->IncThreadCount();
    isThreadAnalysisEnabled.push_back(
        KnobForceInstrumentation.Value());  // Init with instru. disabled (or
                                            // enabled if forced with "-f")
    dynamicTraces.push_back(
        new sinucaTracer::DynamicTraceFile(folderPath, imageName, tid));
    memoryTraces.push_back(
        new sinucaTracer::MemoryTraceFile(folderPath, imageName, tid));
    PIN_ReleaseLock(&pinLock);
}

VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    PIN_GetLock(&pinLock, tid);
    delete dynamicTraces[tid];
    delete memoryTraces[tid];
    PIN_ReleaseLock(&pinLock);
}

VOID InsertInstrumentionOnMemoryOperations(const INS* ins) {
    /*
     * INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise
     */
    if (!INS_IsStandardMemop(*ins)) {
        INS_InsertCall(*ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTraceNonStd,
                       IARG_MULTI_MEMORYACCESS_EA, IARG_END);
        return;
    }

    bool isRead = INS_IsMemoryRead(*ins);
    bool hasRead2 = INS_HasMemoryRead2(*ins);
    bool isWrite = INS_IsMemoryWrite(*ins);

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

IntrinsicInfo* GetIntrinsicInfo(const INS* ins) {
    if (!INS_IsDirectControlFlow(*ins)) {
        return NULL;
    }

    ADDRINT targetAddr = INS_DirectControlFlowTargetAddress(*ins);
    RTN targetRtn = RTN_FindByAddress(targetAddr);
    if (RTN_Valid(targetRtn)) {
        const char* targetName = RTN_Name(targetRtn).c_str();
        for (unsigned int i = 0; i < intrinsics.size(); ++i) {
            if (strcmp(targetName, intrinsics[i].name) == 0) {
                return &intrinsics[i];
            }
        }
    }

    return NULL;
}

VOID AppendIntrinsic(const INS* originalCall, IntrinsicInfo* info) {
    bool read = info->numReadRegs >= 1;
    bool read2 = info->numReadRegs >= 2;
    bool write = info->numWriteRegs >= 1;
    staticTrace->PrepareDataIntrinsic(
        originalCall, info->name, strlen(info->name), read, read2, write,
        info->read, info->numReadRegs, info->write, info->numWriteRegs);
    staticTrace->AppendToBufferDataINS();
    staticTrace->IncInstCount();
}

VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    THREADID tid = PIN_ThreadId();
    RTN traceRtn = TRACE_Rtn(trace);

    if (RTN_Valid(traceRtn)) {
        const char* traceRtnName = RTN_Name(traceRtn).c_str();
#if DEBUG_PRINT_GOMP_RNT == 1
        if (StrStartsWithGomp(traceRtnName)) {
            TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)DebugPrintRtnName,
                             IARG_PTR, traceRtnName, IARG_THREAD_ID, IARG_END);
        }
#endif
        /*
         * This will make every function call from libgomp that have a
         * PAUSE instruction to be ignored
         * Still not sure if this is fully correct
         */
        for (unsigned int i = 0; i < OMP_ignore.size(); ++i) {
            if (strcmp(traceRtnName, OMP_ignore[i]) == 0) {
                // has SPIN_LOCK
                return;
            }
        }
    }

    PIN_GetLock(&pinLock, tid);

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        unsigned int numberInstBBl = BBL_NumIns(bbl);
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_UINT32, staticTrace->GetBBlCount(), IARG_UINT32,
                       numberInstBBl, IARG_END);

        staticTrace->IncBBlCount();
        staticTrace->AppendToBufferNumIns(numberInstBBl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            IntrinsicInfo* intrinsic = GetIntrinsicInfo(&ins);
            if (intrinsic != NULL) {
                AppendIntrinsic(&ins, intrinsic);
                continue;
            }
            staticTrace->PrepareDataINS(&ins);
            staticTrace->AppendToBufferDataINS();
            InsertInstrumentionOnMemoryOperations(&ins);
            staticTrace->IncInstCount();
        }
    }

    PIN_ReleaseLock(&pinLock);
    return;
}

static inline int SeparateStringInSections(char* str, char separator,
                                           char** sections,
                                           int numberOfSections) {
    if (str[0] == '\0') return 0;
    unsigned int index = 0;
    for (int i = 0; i < numberOfSections; ++i) {
        sections[i] = &str[index];
        while (str[index] != separator) {
            if (str[index] == '\0') return i + 1;
            ++index;
        }
        str[index] = '\0';
        ++index;
    }

    return numberOfSections;
}

static inline REG RegisterNameToREG(const char* name) {
    // Probably every register used in user code.
    if (strcmp(name, "rax") == 0) return LEVEL_BASE::REG_RAX;
    if (strcmp(name, "rbx") == 0) return LEVEL_BASE::REG_RBX;
    if (strcmp(name, "rcx") == 0) return LEVEL_BASE::REG_RCX;
    if (strcmp(name, "rdx") == 0) return LEVEL_BASE::REG_RDX;
    if (strcmp(name, "rsi") == 0) return LEVEL_BASE::REG_RSI;
    if (strcmp(name, "rdi") == 0) return LEVEL_BASE::REG_RDI;
    if (strcmp(name, "rsp") == 0) return LEVEL_BASE::REG_RSP;
    if (strcmp(name, "rbp") == 0) return LEVEL_BASE::REG_RBP;
    if (strcmp(name, "r8") == 0) return LEVEL_BASE::REG_R8;
    if (strcmp(name, "r9") == 0) return LEVEL_BASE::REG_R9;
    if (strcmp(name, "r10") == 0) return LEVEL_BASE::REG_R10;
    if (strcmp(name, "r11") == 0) return LEVEL_BASE::REG_R11;
    if (strcmp(name, "r12") == 0) return LEVEL_BASE::REG_R12;
    if (strcmp(name, "r13") == 0) return LEVEL_BASE::REG_R13;
    if (strcmp(name, "r14") == 0) return LEVEL_BASE::REG_R14;
    if (strcmp(name, "r15") == 0) return LEVEL_BASE::REG_R15;
    if (strcmp(name, "eax") == 0) return LEVEL_BASE::REG_EAX;
    if (strcmp(name, "ebx") == 0) return LEVEL_BASE::REG_EBX;
    if (strcmp(name, "ecx") == 0) return LEVEL_BASE::REG_ECX;
    if (strcmp(name, "edx") == 0) return LEVEL_BASE::REG_EDX;
    if (strcmp(name, "esi") == 0) return LEVEL_BASE::REG_ESI;
    if (strcmp(name, "edi") == 0) return LEVEL_BASE::REG_EDI;
    if (strcmp(name, "esp") == 0) return LEVEL_BASE::REG_ESP;
    if (strcmp(name, "ebp") == 0) return LEVEL_BASE::REG_EBP;
    if (strcmp(name, "r8d") == 0) return LEVEL_BASE::REG_R8D;
    if (strcmp(name, "r9d") == 0) return LEVEL_BASE::REG_R9D;
    if (strcmp(name, "r10d") == 0) return LEVEL_BASE::REG_R10D;
    if (strcmp(name, "r11d") == 0) return LEVEL_BASE::REG_R11D;
    if (strcmp(name, "r12d") == 0) return LEVEL_BASE::REG_R12D;
    if (strcmp(name, "r13d") == 0) return LEVEL_BASE::REG_R13D;
    if (strcmp(name, "r14d") == 0) return LEVEL_BASE::REG_R14D;
    if (strcmp(name, "r15d") == 0) return LEVEL_BASE::REG_R15D;
    if (strcmp(name, "ax") == 0) return LEVEL_BASE::REG_AX;
    if (strcmp(name, "bx") == 0) return LEVEL_BASE::REG_BX;
    if (strcmp(name, "cx") == 0) return LEVEL_BASE::REG_CX;
    if (strcmp(name, "dx") == 0) return LEVEL_BASE::REG_DX;
    if (strcmp(name, "si") == 0) return LEVEL_BASE::REG_SI;
    if (strcmp(name, "di") == 0) return LEVEL_BASE::REG_DI;
    if (strcmp(name, "sp") == 0) return LEVEL_BASE::REG_SP;
    if (strcmp(name, "bp") == 0) return LEVEL_BASE::REG_BP;
    if (strcmp(name, "r8w") == 0) return LEVEL_BASE::REG_R8W;
    if (strcmp(name, "r9w") == 0) return LEVEL_BASE::REG_R9W;
    if (strcmp(name, "r10w") == 0) return LEVEL_BASE::REG_R10W;
    if (strcmp(name, "r11w") == 0) return LEVEL_BASE::REG_R11W;
    if (strcmp(name, "r12w") == 0) return LEVEL_BASE::REG_R12W;
    if (strcmp(name, "r13w") == 0) return LEVEL_BASE::REG_R13W;
    if (strcmp(name, "r14w") == 0) return LEVEL_BASE::REG_R14W;
    if (strcmp(name, "r15w") == 0) return LEVEL_BASE::REG_R15W;
    if (strcmp(name, "al") == 0) return LEVEL_BASE::REG_AL;
    if (strcmp(name, "bl") == 0) return LEVEL_BASE::REG_BL;
    if (strcmp(name, "cl") == 0) return LEVEL_BASE::REG_CL;
    if (strcmp(name, "dl") == 0) return LEVEL_BASE::REG_DL;
    if (strcmp(name, "sil") == 0) return LEVEL_BASE::REG_SIL;
    if (strcmp(name, "dil") == 0) return LEVEL_BASE::REG_DIL;
    if (strcmp(name, "spl") == 0) return LEVEL_BASE::REG_SPL;
    if (strcmp(name, "bpl") == 0) return LEVEL_BASE::REG_BPL;
    if (strcmp(name, "r8b") == 0) return LEVEL_BASE::REG_R8B;
    if (strcmp(name, "r9b") == 0) return LEVEL_BASE::REG_R9B;
    if (strcmp(name, "r10b") == 0) return LEVEL_BASE::REG_R10B;
    if (strcmp(name, "r11b") == 0) return LEVEL_BASE::REG_R11B;
    if (strcmp(name, "r12b") == 0) return LEVEL_BASE::REG_R12B;
    if (strcmp(name, "r13b") == 0) return LEVEL_BASE::REG_R13B;
    if (strcmp(name, "r14b") == 0) return LEVEL_BASE::REG_R14B;
    if (strcmp(name, "r15b") == 0) return LEVEL_BASE::REG_R15B;
    if (strcmp(name, "rip") == 0) return LEVEL_BASE::REG_RIP;
    if (strcmp(name, "eip") == 0) return LEVEL_BASE::REG_EIP;
    if (strcmp(name, "ip") == 0) return LEVEL_BASE::REG_IP;
    if (strcmp(name, "mm0") == 0) return LEVEL_BASE::REG_MM0;
    if (strcmp(name, "mm1") == 0) return LEVEL_BASE::REG_MM1;
    if (strcmp(name, "mm2") == 0) return LEVEL_BASE::REG_MM2;
    if (strcmp(name, "mm3") == 0) return LEVEL_BASE::REG_MM3;
    if (strcmp(name, "mm4") == 0) return LEVEL_BASE::REG_MM4;
    if (strcmp(name, "mm5") == 0) return LEVEL_BASE::REG_MM5;
    if (strcmp(name, "mm6") == 0) return LEVEL_BASE::REG_MM6;
    if (strcmp(name, "mm7") == 0) return LEVEL_BASE::REG_MM7;
    if (strcmp(name, "xmm0") == 0) return LEVEL_BASE::REG_XMM0;
    if (strcmp(name, "xmm1") == 0) return LEVEL_BASE::REG_XMM1;
    if (strcmp(name, "xmm2") == 0) return LEVEL_BASE::REG_XMM2;
    if (strcmp(name, "xmm3") == 0) return LEVEL_BASE::REG_XMM3;
    if (strcmp(name, "xmm4") == 0) return LEVEL_BASE::REG_XMM4;
    if (strcmp(name, "xmm5") == 0) return LEVEL_BASE::REG_XMM5;
    if (strcmp(name, "xmm6") == 0) return LEVEL_BASE::REG_XMM6;
    if (strcmp(name, "xmm7") == 0) return LEVEL_BASE::REG_XMM7;
    if (strcmp(name, "xmm8") == 0) return LEVEL_BASE::REG_XMM8;
    if (strcmp(name, "xmm9") == 0) return LEVEL_BASE::REG_XMM9;
    if (strcmp(name, "xmm10") == 0) return LEVEL_BASE::REG_XMM10;
    if (strcmp(name, "xmm11") == 0) return LEVEL_BASE::REG_XMM11;
    if (strcmp(name, "xmm12") == 0) return LEVEL_BASE::REG_XMM12;
    if (strcmp(name, "xmm13") == 0) return LEVEL_BASE::REG_XMM13;
    if (strcmp(name, "xmm14") == 0) return LEVEL_BASE::REG_XMM14;
    if (strcmp(name, "xmm15") == 0) return LEVEL_BASE::REG_XMM15;
    if (strcmp(name, "xmm16") == 0) return LEVEL_BASE::REG_XMM16;
    if (strcmp(name, "xmm17") == 0) return LEVEL_BASE::REG_XMM17;
    if (strcmp(name, "xmm18") == 0) return LEVEL_BASE::REG_XMM18;
    if (strcmp(name, "xmm19") == 0) return LEVEL_BASE::REG_XMM19;
    if (strcmp(name, "xmm20") == 0) return LEVEL_BASE::REG_XMM20;
    if (strcmp(name, "xmm21") == 0) return LEVEL_BASE::REG_XMM21;
    if (strcmp(name, "xmm22") == 0) return LEVEL_BASE::REG_XMM22;
    if (strcmp(name, "xmm23") == 0) return LEVEL_BASE::REG_XMM23;
    if (strcmp(name, "xmm24") == 0) return LEVEL_BASE::REG_XMM24;
    if (strcmp(name, "xmm25") == 0) return LEVEL_BASE::REG_XMM25;
    if (strcmp(name, "xmm26") == 0) return LEVEL_BASE::REG_XMM26;
    if (strcmp(name, "xmm27") == 0) return LEVEL_BASE::REG_XMM27;
    if (strcmp(name, "xmm28") == 0) return LEVEL_BASE::REG_XMM28;
    if (strcmp(name, "xmm29") == 0) return LEVEL_BASE::REG_XMM29;
    if (strcmp(name, "xmm30") == 0) return LEVEL_BASE::REG_XMM30;
    if (strcmp(name, "xmm31") == 0) return LEVEL_BASE::REG_XMM31;
    if (strcmp(name, "ymm0") == 0) return LEVEL_BASE::REG_YMM0;
    if (strcmp(name, "ymm1") == 0) return LEVEL_BASE::REG_YMM1;
    if (strcmp(name, "ymm2") == 0) return LEVEL_BASE::REG_YMM2;
    if (strcmp(name, "ymm3") == 0) return LEVEL_BASE::REG_YMM3;
    if (strcmp(name, "ymm4") == 0) return LEVEL_BASE::REG_YMM4;
    if (strcmp(name, "ymm5") == 0) return LEVEL_BASE::REG_YMM5;
    if (strcmp(name, "ymm6") == 0) return LEVEL_BASE::REG_YMM6;
    if (strcmp(name, "ymm7") == 0) return LEVEL_BASE::REG_YMM7;
    if (strcmp(name, "ymm8") == 0) return LEVEL_BASE::REG_YMM8;
    if (strcmp(name, "ymm9") == 0) return LEVEL_BASE::REG_YMM9;
    if (strcmp(name, "ymm10") == 0) return LEVEL_BASE::REG_YMM10;
    if (strcmp(name, "ymm11") == 0) return LEVEL_BASE::REG_YMM11;
    if (strcmp(name, "ymm12") == 0) return LEVEL_BASE::REG_YMM12;
    if (strcmp(name, "ymm13") == 0) return LEVEL_BASE::REG_YMM13;
    if (strcmp(name, "ymm14") == 0) return LEVEL_BASE::REG_YMM14;
    if (strcmp(name, "ymm15") == 0) return LEVEL_BASE::REG_YMM15;
    if (strcmp(name, "ymm16") == 0) return LEVEL_BASE::REG_YMM16;
    if (strcmp(name, "ymm17") == 0) return LEVEL_BASE::REG_YMM17;
    if (strcmp(name, "ymm18") == 0) return LEVEL_BASE::REG_YMM18;
    if (strcmp(name, "ymm19") == 0) return LEVEL_BASE::REG_YMM19;
    if (strcmp(name, "ymm20") == 0) return LEVEL_BASE::REG_YMM20;
    if (strcmp(name, "ymm21") == 0) return LEVEL_BASE::REG_YMM21;
    if (strcmp(name, "ymm22") == 0) return LEVEL_BASE::REG_YMM22;
    if (strcmp(name, "ymm23") == 0) return LEVEL_BASE::REG_YMM23;
    if (strcmp(name, "ymm24") == 0) return LEVEL_BASE::REG_YMM24;
    if (strcmp(name, "ymm25") == 0) return LEVEL_BASE::REG_YMM25;
    if (strcmp(name, "ymm26") == 0) return LEVEL_BASE::REG_YMM26;
    if (strcmp(name, "ymm27") == 0) return LEVEL_BASE::REG_YMM27;
    if (strcmp(name, "ymm28") == 0) return LEVEL_BASE::REG_YMM28;
    if (strcmp(name, "ymm29") == 0) return LEVEL_BASE::REG_YMM29;
    if (strcmp(name, "ymm30") == 0) return LEVEL_BASE::REG_YMM30;
    if (strcmp(name, "ymm31") == 0) return LEVEL_BASE::REG_YMM31;
    if (strcmp(name, "zmm0") == 0) return LEVEL_BASE::REG_ZMM0;
    if (strcmp(name, "zmm1") == 0) return LEVEL_BASE::REG_ZMM1;
    if (strcmp(name, "zmm2") == 0) return LEVEL_BASE::REG_ZMM2;
    if (strcmp(name, "zmm3") == 0) return LEVEL_BASE::REG_ZMM3;
    if (strcmp(name, "zmm4") == 0) return LEVEL_BASE::REG_ZMM4;
    if (strcmp(name, "zmm5") == 0) return LEVEL_BASE::REG_ZMM5;
    if (strcmp(name, "zmm6") == 0) return LEVEL_BASE::REG_ZMM6;
    if (strcmp(name, "zmm7") == 0) return LEVEL_BASE::REG_ZMM7;
    if (strcmp(name, "zmm8") == 0) return LEVEL_BASE::REG_ZMM8;
    if (strcmp(name, "zmm9") == 0) return LEVEL_BASE::REG_ZMM9;
    if (strcmp(name, "zmm10") == 0) return LEVEL_BASE::REG_ZMM10;
    if (strcmp(name, "zmm11") == 0) return LEVEL_BASE::REG_ZMM11;
    if (strcmp(name, "zmm12") == 0) return LEVEL_BASE::REG_ZMM12;
    if (strcmp(name, "zmm13") == 0) return LEVEL_BASE::REG_ZMM13;
    if (strcmp(name, "zmm14") == 0) return LEVEL_BASE::REG_ZMM14;
    if (strcmp(name, "zmm15") == 0) return LEVEL_BASE::REG_ZMM15;
    if (strcmp(name, "zmm16") == 0) return LEVEL_BASE::REG_ZMM16;
    if (strcmp(name, "zmm17") == 0) return LEVEL_BASE::REG_ZMM17;
    if (strcmp(name, "zmm18") == 0) return LEVEL_BASE::REG_ZMM18;
    if (strcmp(name, "zmm19") == 0) return LEVEL_BASE::REG_ZMM19;
    if (strcmp(name, "zmm20") == 0) return LEVEL_BASE::REG_ZMM20;
    if (strcmp(name, "zmm21") == 0) return LEVEL_BASE::REG_ZMM21;
    if (strcmp(name, "zmm22") == 0) return LEVEL_BASE::REG_ZMM22;
    if (strcmp(name, "zmm23") == 0) return LEVEL_BASE::REG_ZMM23;
    if (strcmp(name, "zmm24") == 0) return LEVEL_BASE::REG_ZMM24;
    if (strcmp(name, "zmm25") == 0) return LEVEL_BASE::REG_ZMM25;
    if (strcmp(name, "zmm26") == 0) return LEVEL_BASE::REG_ZMM26;
    if (strcmp(name, "zmm27") == 0) return LEVEL_BASE::REG_ZMM27;
    if (strcmp(name, "zmm28") == 0) return LEVEL_BASE::REG_ZMM28;
    if (strcmp(name, "zmm29") == 0) return LEVEL_BASE::REG_ZMM29;
    if (strcmp(name, "zmm30") == 0) return LEVEL_BASE::REG_ZMM30;
    if (strcmp(name, "zmm31") == 0) return LEVEL_BASE::REG_ZMM31;
    if (strcmp(name, "k0") == 0) return LEVEL_BASE::REG_K0;
    if (strcmp(name, "k1") == 0) return LEVEL_BASE::REG_K1;
    if (strcmp(name, "k2") == 0) return LEVEL_BASE::REG_K2;
    if (strcmp(name, "k3") == 0) return LEVEL_BASE::REG_K3;
    if (strcmp(name, "k4") == 0) return LEVEL_BASE::REG_K4;
    if (strcmp(name, "k5") == 0) return LEVEL_BASE::REG_K5;
    if (strcmp(name, "k6") == 0) return LEVEL_BASE::REG_K6;
    if (strcmp(name, "k7") == 0) return LEVEL_BASE::REG_K7;
    if (strcmp(name, "tmm0") == 0) return LEVEL_BASE::REG_TMM0;
    if (strcmp(name, "tmm1") == 0) return LEVEL_BASE::REG_TMM1;
    if (strcmp(name, "tmm2") == 0) return LEVEL_BASE::REG_TMM2;
    if (strcmp(name, "tmm3") == 0) return LEVEL_BASE::REG_TMM3;
    if (strcmp(name, "tmm4") == 0) return LEVEL_BASE::REG_TMM4;
    if (strcmp(name, "tmm5") == 0) return LEVEL_BASE::REG_TMM5;
    if (strcmp(name, "tmm6") == 0) return LEVEL_BASE::REG_TMM6;
    if (strcmp(name, "tmm7") == 0) return LEVEL_BASE::REG_TMM7;

    return LEVEL_BASE::REG_INVALID();
}

static inline void SetRegistersInIntrinsicsInfo(REG* arr, unsigned char* num,
                                                char* str) {
    char* sections[sinucaTracer::MAX_REG_OPERANDS];
    *num = SeparateStringInSections(str, ',', sections,
                                    sinucaTracer::MAX_REG_OPERANDS);
    for (unsigned char i = 0; i < *num; ++i) {
        arr[i] = RegisterNameToREG(sections[i]);
    }
}

VOID LoadIntrinsics() {
    // This also fails silently. TODO: Make all of this better and crash with a
    // good error message when needed.
    for (unsigned int knobIdx = 0; knobIdx < KnobIntrinsics.NumberOfValues();
         ++knobIdx) {
        std::string value = KnobIntrinsics.Value(knobIdx);
        char* strValue = (char*)alloca(sizeof(char) * (value.size() + 1));
        memcpy((void*)strValue, (void*)value.c_str(),
               sizeof(char) * (value.size() + 1));

        char* sections[5];
        SeparateStringInSections(strValue, ':', sections, 5);
        char* name = sections[0];
        char* readRegs = sections[1];
        char* writeRegs = sections[2];
        char* numReads = sections[3];
        char* numWrites = sections[4];

        SINUCA3_LOG_PRINTF(
            "Using intrinsic: %s readRegs: %s writeRegs: %s numReads: %s "
            "numWrites: %s\n",
            name, readRegs, writeRegs, numReads, numWrites);

        intrinsics.push_back(IntrinsicInfo{});
        IntrinsicInfo* i = &intrinsics[intrinsics.size() - 1];

        // Copy the name.
        unsigned int nameSize = strlen(name);
        if (nameSize > sinucaTracer::MAX_INSTRUCTION_NAME_LENGTH)
            nameSize = sinucaTracer::MAX_INSTRUCTION_NAME_LENGTH - 1;
        memcpy(&i->name, name, nameSize + 1);

        // Copy registers.
        SetRegistersInIntrinsicsInfo(i->read, &i->numReadRegs, readRegs);
        SetRegistersInIntrinsicsInfo(i->write, &i->numWriteRegs, writeRegs);

        // Copy numbers.
        int nReads = atoi(numReads);
        i->isRead = nReads > 0;
        i->isRead2 = nReads > 1;
        i->isWrite = atoi(numWrites) > 0;
    }
}

VOID OnImageLoad(IMG img, VOID* ptr) {
    if (IMG_IsMainExecutable(img) == false) return;

    std::string completeImgPath = IMG_Name(img);
    unsigned long imgPathLen = completeImgPath.size();
    const char* completeImgPathPtr = completeImgPath.c_str();
    // As Pin gives us the absolute path for the executable, it always have at
    // least a '/' (the root of the filesystem).
    // We need to find the word after the last '/'.
    int idx = 0;
    for (int i = imgPathLen - 1; i >= 0; --i) {
        if (completeImgPathPtr[i] == '/') {
            idx = i + 1;
            break;
        }
    }
    unsigned long imageNameLen = imgPathLen - idx + sizeof('\0');
    imageName = (char*)malloc(imageNameLen);  // freed in Fini()
    memcpy(imageName, completeImgPathPtr + idx, imageNameLen);

    staticTrace = new sinucaTracer::StaticTraceFile(folderPath, imageName);
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            const char* name = RTN_Name(rtn).c_str();

            if (strcmp(name, "BeginInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)InitInstrumentation,
                               IARG_END);
            } else if (strcmp(name, "EndInstrumentationBlock") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)StopInstrumentation,
                               IARG_END);
            } else if (strcmp(name, "EnableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)EnableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            } else if (strcmp(name, "DisableThreadInstrumentation") == 0) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)DisableInstrumentationInThread,
                               IARG_THREAD_ID, IARG_END);
            } else {
                for (unsigned int i = 0; i < intrinsics.size(); ++i) {
                    if (strcmp(name, intrinsics[i].name) == 0) {
                        RTN_InsertCall(rtn, IPOINT_BEFORE,
                                       (AFUNPTR)DisableInstrumentationInThread,
                                       IARG_THREAD_ID, IARG_END);
                        RTN_InsertCall(rtn, IPOINT_AFTER,
                                       (AFUNPTR)EnableInstrumentationInThread,
                                       IARG_THREAD_ID, IARG_END);
                        break;
                    }
                }
            }

            RTN_Close(rtn);
        }
    }
}

VOID OnFini(INT32 code, VOID* ptr) {
    SINUCA3_LOG_PRINTF("End of tool execution\n");
    SINUCA3_DEBUG_PRINTF("Number of BBLs [%u]\n", staticTrace->GetBBlCount());

    if (imageName != NULL) free(imageName);

    // Close static trace file
    delete staticTrace;

    if (!wasInstrumented) {
        SINUCA3_WARNING_PRINTF(
            "No instrumentation blocks were found in the target program.\n"
            "As result, no instruction data was recorded in the trace files.\n"
            "To enable instrumentation, wrap the target code with "
            "BeginInstrumentationBlock() and EndInstrumentationBlock().\n"
            "Instrumentation code is only inserted within these blocks, and "
            "analysis is only executed if the thread has\n"
            "called EnableThreadInstrumentation().\n"
            "Use the -f flag to force instrumentation even when no blocks are "
            "defined.\n"
            "Use the -f flag to force instrumentation even when no blocks are "
            "defined.\n");
    }
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    folderPath = KnobFolder.Value().c_str();
    if (access(folderPath, F_OK) != 0) {
        mkdir(folderPath, S_IRWXU | S_IRWXG | S_IROTH);
    }

    PIN_InitLock(&pinLock);

    LoadIntrinsics();

    // Init with instru. disabled (or enabled if forced with "-f");
    if (KnobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF(
            "Force flag (\"-f\") enabled: Instrumentating entire program for "
            "all created threads.\n");
        InitInstrumentation();
    } else {
        isInstrumentating = false;
    }

    InitGompHandling();

    IMG_AddInstrumentFunction(OnImageLoad, NULL);
    TRACE_AddInstrumentFunction(OnTrace, NULL);
    PIN_AddFiniFunction(OnFini, NULL);

    PIN_AddThreadStartFunction(OnThreadStart, NULL);
    PIN_AddThreadFiniFunction(OnThreadFini, NULL);

    // Never returns
    PIN_StartProgram();

    return 0;
}
