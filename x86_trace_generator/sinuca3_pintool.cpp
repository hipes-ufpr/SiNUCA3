//
// Copyright (C) 2024 HiPES - Universidade Federal do Paraná
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
 *
 * @note Instrumentation is the process of deciding where and what code
 * should be inserted into the target program, while analysis refers to the
 * code that is actually executed at those insertion points to gather
 * information about the program’s behavior.
 *
 * @details To enable instrumentation, wrap the target code with
 * BeginInstrumentationBlock() and EndInstrumentationBlock(). Instrumentation
 * code is only inserted within these blocks.
 *
 * Example command:
 * ./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -- ./my_program
 *
 */

// #define NDEBUG

#include <climits>
#include <sinuca3.hpp>

#include "engine/default_packets.hpp"
#include "pin.H"
#include "tracer/sinuca/file_handler.hpp"
#include "utils/dynamic_trace_writer.hpp"
#include "utils/logging.hpp"
#include "utils/memory_trace_writer.hpp"
#include "utils/static_trace_writer.hpp"

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
}

/**
 * @brief When enabled, this flag allows the pintool to record all
 * instructions static info into a static trace file, and allows the
 * instrumentation phase (e.g., in OnTrace) to insert analysis code into the
 * target program.
 */
bool isInstrumentating;
/**
 * @brief The static trace file is shared among threads. It stores all basic
 * blocks that are eventually reached during execution.
 */
StaticTraceWriter* staticTrace;

/** @brief Directory where the trace files will be saved. */
const char* traceDir = NULL;
/** @brief Name of the executable being instrumented. */
const char* imageName = NULL;
/** @brief Set to true when InitInstrumentation is called. */
bool wasInitInstrumentationCalled = false;
/** @brief Number of executed instructions. */
unsigned long numberOfExecInst = 0;
/** @brief Lock used to prevent race conditions when executing analysis code */
PIN_LOCK threadAnalysisLock;

/*
 * A KNOB is a class that encapsulates a command line argument. When the
 * argument is not provided, the default value declared is used.
 */
KNOB<std::string> knobFolder(KNOB_MODE_WRITEONCE, "pintool", "o", "./",
                             "Directory to store the trace files.");
KNOB<BOOL> knobForceInstrumentation(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                                    "Force instrumentation.");
KNOB<UINT32> knobNumberOfInstructions(KNOB_MODE_WRITEONCE, "pintool", "n", "-1",
                                      "Set maximum of instructions.");
KNOB<std::string> KnobIntrinsics(KNOB_MODE_APPEND, "pintool", "i", "",
                                 "Intrinsic instructions in the format "
                                 "name:readregs:writeregs");

std::vector<const char*> ignoreRtnsVec;

struct ThreadData {
    DynamicTraceWriter dynamicTrace;
    MemoryTraceWriter memoryTrace;
    /** @brief The instrumentation may be disabled in a specific thread. */
    bool isInstrumentating;
};

std::vector<ThreadData*> threadDataVec;

struct IntrinsicInfo {
    char name[INST_MNEMONIC_LEN - 1];
    char loaderName[INST_MNEMONIC_LEN + sizeof("__Loader")];  // Cache.
    REG read[MAX_REGISTERS];
    REG write[MAX_REGISTERS];
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
};

std::vector<IntrinsicInfo> intrinsics;

int Usage() {
    SINUCA3_LOG_PRINTF(
        "Example command: "
        "\t./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -- ./my_program\n"
        "------------------------------------------------------------"
        "-f: force instrumentation even when no blocks are defined.\n"
        "-o: output directory.\n"
        "-n: set maximum number of instructions to append to trace.\n"
        "-i: set intrinsics.\n");

    return 1;
}

bool WasThreadCreated(THREADID tid) {
    return ((threadDataVec.size() - tid) > 0);
}

/** @brief Enables instrumentation. */
VOID InitInstrumentation() {
    if (isInstrumentating) return;
    SINUCA3_DEBUG_PRINTF("-----------------------------------\n");
    SINUCA3_DEBUG_PRINTF("Start of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("-----------------------------------\n");
    wasInitInstrumentationCalled = true;
    isInstrumentating = true;
}

/** @brief Resume instrumentation in a thread. */
VOID ResumeInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid)) {
        SINUCA3_ERROR_PRINTF("[ResumeInstrumentationInThread] thr not created");
        return;
    }
    threadDataVec[tid]->isInstrumentating = true;
}

/** @brief Disable instrumentation. */
VOID StopInstrumentation() {
    if (!isInstrumentating || knobForceInstrumentation.Value()) return;
    SINUCA3_DEBUG_PRINTF("---------------------------------\n");
    SINUCA3_DEBUG_PRINTF("End of tool instrumentation block\n");
    SINUCA3_DEBUG_PRINTF("---------------------------------\n");
    isInstrumentating = false;
}

/** @brief Disable instrumentation in a thread. */
VOID StopInstrumentationInThread(THREADID tid) {
    if (!WasThreadCreated(tid)) {
        SINUCA3_ERROR_PRINTF("[StopInstrumentationInThread] thr not created");
        return;
    }
    threadDataVec[tid]->isInstrumentating = false;
}

/** @brief Set up thread data */
VOID OnThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    struct ThreadData* threadData = new ThreadData;
    if (!threadData) {
        SINUCA3_ERROR_PRINTF("[OnThreadStart] Failed to alloc thread data.\n");
        return;
    }

    /* Create tracer files */
    if (threadData->dynamicTrace.OpenFile(traceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF(
            "[OnThreadStart] Failed to open dynamic trace file\n");
    }
    if (threadData->memoryTrace.OpenFile(traceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF(
            "[OnThreadStart] Failed to open memory trace file\n");
    }

    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnThreadStart] thread id [%d]\n", tid);
    threadData->isInstrumentating = true;
    threadDataVec.push_back(threadData);
    staticTrace->IncThreadCount();
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Destroy thread data. */
VOID OnThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    if (!WasThreadCreated(tid)) return;
    PIN_GetLock(&threadAnalysisLock, tid);
    SINUCA3_DEBUG_PRINTF("[OnThreadFini] thread id [%d]\n", tid);
    delete threadDataVec[tid];
    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Append basic block identifier to dynamic trace. */
VOID AppendToDynamicTrace(THREADID tid, UINT32 bblId, UINT32 numInst) {
    if (!WasThreadCreated(tid)) return;

    PIN_GetLock(&threadAnalysisLock, tid);

    if (knobNumberOfInstructions.Value() != UINT_MAX) {
        if (numberOfExecInst > knobNumberOfInstructions.Value()) {
            SINUCA3_WARNING_PRINTF("Reached maximum of instructions!\n");
            /*
             * This loop adds an abrupt end event to the dynamic trace, which
             * signals to the trace reader that all analysis code was abruptly
             * halted and that obtained locks may not be released by an unlock
             * thread event for example.
             */
            for (unsigned int tid = 0; tid < threadDataVec.size(); ++tid) {
                threadDataVec[tid]->dynamicTrace.AddThreadEvent(
                    ThreadEventAbruptEnd);
            }
            PIN_ReleaseLock(&threadAnalysisLock);
            PIN_ExitApplication(0);
        }
    }

    numberOfExecInst += numInst;

    threadDataVec[tid]->dynamicTrace.IncExecutedInstructions(numInst);

    SINUCA3_DEBUG_PRINTF("Thr [%d] adding bbl index [%u]\n", tid, bblId);
    SINUCA3_DEBUG_PRINTF("Bbl [%u] has [%d] instructions\n", bblId, numInst);

    if (threadDataVec[tid]->dynamicTrace.AddBasicBlockId(bblId)) {
        SINUCA3_ERROR_PRINTF(
            "[AppendToDynamicTrace] Failed to add basic block id to file\n");
    }

    PIN_ReleaseLock(&threadAnalysisLock);
}

/** @brief Add memory operations to trace. */
VOID AppendToMemTrace(THREADID tid, PIN_MULTI_MEM_ACCESS_INFO* accessInfo) {
    if (!WasThreadCreated(tid)) return;

    /*
     * The reader must know the number of memory operations to fetch from the
     * trace. Beware that the number of accesses is not fixed.
     */
    int totalOps = accessInfo->numberOfMemops;
    int totalOpsCopy = totalOps;

    for (int i = 0; i < totalOpsCopy; ++i) {
        if (!accessInfo->memop[i].maskOn) {
            --totalOps;
        }
    }

    if (threadDataVec[tid]->memoryTrace.AddNumberOfMemOperations(totalOps)) {
        SINUCA3_ERROR_PRINTF(
            "[AppendToMemTrace] Failed to add number of mem ops to file\n");
    }

    for (int i = 0; i < totalOpsCopy; i++) {
        if (!accessInfo->memop[i].maskOn) {
            continue;
        }

        bool isLoadOp = (accessInfo->memop[i].memopType == PIN_MEMOP_LOAD);
        int failed = threadDataVec[tid]->memoryTrace.AddMemOp(
            accessInfo->memop[i].memoryAddress,
            accessInfo->memop[i].bytesAccessed, isLoadOp);
        if (failed) {
            SINUCA3_ERROR_PRINTF(
                "[AppendToMemTrace] Failed to add memory operation!\n");
        }
    }
}

int TranslatePinInst(Instruction* inst, const INS* pinInst) {
    if (inst == NULL) {
        SINUCA3_ERROR_PRINTF("[TranslatePinInst] inst is nil\n");
        return 1;
    }
    if (pinInst == NULL) {
        SINUCA3_ERROR_PRINTF("[TranslatePinInst] pinInst is nil\n");
        return 1;
    }

    memset(inst, 0, sizeof(*inst));

    std::string mnemonic = INS_Mnemonic(*pinInst);
    unsigned long size = sizeof(inst->instructionMnemonic) - 1;
    strncpy(inst->instructionMnemonic, mnemonic.c_str(), size);
    if (size < mnemonic.size()) {
        SINUCA3_WARNING_PRINTF(
            "[TranslatePinInst] Insufficient space to store inst mnemonic\n");
    }

    inst->instructionAddress = INS_Address(*pinInst);
    /* at most 15 bytes len (for now) */
    inst->instructionSize = INS_Size(*pinInst);
    /* manual flush with CLFLUSH/CLFLUSHOPT/CLWB/WBINVD/INVD */
    /* or cache coherence induced flush */
    inst->instCausesCacheLineFlush = INS_IsCacheLineFlush(*pinInst);
    /* false for any instruction which in practice is a system call */
    inst->isCallInstruction = INS_IsCall(*pinInst);
    inst->isSyscallInstruction = INS_IsSyscall(*pinInst);
    inst->isRetInstruction = INS_IsRet(*pinInst);
    inst->isSysretInstruction = INS_IsSysret(*pinInst);
    /* false for unconditional branches and calls */
    inst->instHasFallthrough = INS_HasFallThrough(*pinInst);
    /* false for system call */
    inst->isBranchInstruction = INS_IsBranch(*pinInst);
    inst->isIndirectCtrlFlowInst = INS_IsIndirectControlFlow(*pinInst);
    /* field checked before reading from memory trace */
    inst->instReadsMemory = INS_IsMemoryRead(*pinInst);
    inst->instWritesMemory = INS_IsMemoryWrite(*pinInst);
    /* e.g. CMOV */
    inst->isPredicatedInst = INS_IsPredicated(*pinInst);

    for (unsigned int i = 0; i < INS_OperandCount(*pinInst); ++i) {
        /* Interest only in register operands */
        if (!INS_OperandIsReg(*pinInst, i)) {
            continue;
        }

        const unsigned long readRegsArraySize =
            sizeof(inst->readRegsArray) / sizeof(*inst->readRegsArray);
        const unsigned long writeRegsArraySize =
            sizeof(inst->writtenRegsArray) / sizeof(*inst->writtenRegsArray);

        unsigned short reg = INS_OperandReg(*pinInst, i);
        if (INS_OperandRead(*pinInst, i)) {
            if (inst->rRegsArrayOccupation >= readRegsArraySize) {
                SINUCA3_ERROR_PRINTF(
                    "[TranslatePinInst] More registers read than readRegsArray "
                    "can store\n");
                return 1;
            }
            inst->readRegsArray[inst->rRegsArrayOccupation] = reg;
            ++inst->rRegsArrayOccupation;
        }
        if (INS_OperandWritten(*pinInst, i)) {
            if (inst->wRegsArrayOccupation >= writeRegsArraySize) {
                SINUCA3_ERROR_PRINTF(
                    "[TranslatePinInst] More registers written than "
                    "writtenRegsArray can "
                    "store\n");
                return 1;
            }
            inst->writtenRegsArray[inst->wRegsArrayOccupation] = reg;
            ++inst->wRegsArrayOccupation;
        }
    }

    return 0;
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
            if (strcmp(targetName, intrinsics[i].loaderName) == 0)
                return &intrinsics[i];
        }
    }

    return NULL;
}

int IntrinsicToSinucaInst(const INS* originalCall, IntrinsicInfo* info,
                          Instruction* inst) {
    memset(inst, 0, sizeof(*inst));

    unsigned long size = sizeof(inst->instructionMnemonic) - 1;
    strncpy(inst->instructionMnemonic, info->name, size);
    if (size < strlen(info->name)) {
        SINUCA3_WARNING_PRINTF(
            "[IntrinsicToSinucaInst] Insufficient space to store mnemonic\n");
    }

    inst->instructionAddress = INS_Address(*originalCall);
    inst->instructionSize = INS_Size(*originalCall);
    inst->rRegsArrayOccupation = info->numReadRegs;
    inst->wRegsArrayOccupation = info->numWriteRegs;

    // outros campos nao preenchidos

    const unsigned long readRegsArraySize =
        sizeof(inst->readRegsArray) / sizeof(*inst->readRegsArray);
    if (readRegsArraySize < info->numReadRegs) {
        SINUCA3_WARNING_PRINTF(
            "[IntrinsicToSinucaInst] Insufficient space to store read regs\n");
        return 1;
    }
    memcpy(inst->readRegsArray, info->read,
           info->numReadRegs * sizeof(*info->read));

    const unsigned long writeRegsArraySize =
        sizeof(inst->writtenRegsArray) / sizeof(*inst->writtenRegsArray);
    if (writeRegsArraySize < info->numWriteRegs) {
        SINUCA3_WARNING_PRINTF(
            "[IntrinsicToSinucaInst] Insufficient space to store write regs\n");
        return 1;
    }
    memcpy(inst->writtenRegsArray, info->write,
           info->numWriteRegs * sizeof(*info->write));

    return 0;
}

VOID OnTrace(TRACE trace, VOID* ptr) {
    if (!isInstrumentating) return;

    unsigned int threadId = PIN_ThreadId();

    if (!WasThreadCreated(threadId)) {
        return;
    }
    if (!threadDataVec[threadId]->isInstrumentating) {
        return;
    }

    RTN rtn = TRACE_Rtn(trace);
    if (!RTN_Valid(rtn)) {
        SINUCA3_ERROR_PRINTF("[OnTrace] Found invalid routine! Skipping...\n");
        return;
    }

    RTN_Open(rtn);
    std::string rtnName = RTN_Name(rtn);
    RTN_Close(rtn);

    /*
     * Remove unwanted spinlock.
     */
    for (unsigned int it = 0; it < ignoreRtnsVec.size(); it++) {
        if (rtnName == ignoreRtnsVec[it]) {
            SINUCA3_DEBUG_PRINTF("[OnTrace] Thread id [%d]: Ignoring [%s]!\n",
                                 threadId, rtnName.c_str());
            return;
        }
    }

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        unsigned int numberInstInBasicBlock = BBL_NumIns(bbl);
        unsigned int basicBlockIndex = staticTrace->GetBasicBlockCount();
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)AppendToDynamicTrace,
                       IARG_THREAD_ID, IARG_UINT32, basicBlockIndex,
                       IARG_UINT32, numberInstInBasicBlock, IARG_END);
        /*
         * The trace reader needs to know where the block begins and
         * ends to create the basic block dictionary.
         */
        if (staticTrace->AddBasicBlockSize(numberInstInBasicBlock)) {
            SINUCA3_ERROR_PRINTF(
                "[OnTrace] Failed to add basic block count to file\n");
        }

        staticTrace->IncBasicBlockCount();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            static Instruction sinucaInst;
            /*
             * The number of static instructions will later be useful while
             * reading the trace and instantiating the basic block dictionary.
             */
            staticTrace->IncStaticInstructionCount();

            IntrinsicInfo* intrinsic = GetIntrinsicInfo(&ins);
            bool isIntrinsic = (intrinsic != NULL);

            if (isIntrinsic) {
                IntrinsicToSinucaInst(&ins, intrinsic, &sinucaInst);
                if (staticTrace->AddInstruction(&sinucaInst)) {
                    SINUCA3_ERROR_PRINTF(
                        "[OnTrace] Failed to add intrinsic to file\n");
                }
                continue;
            }

            if (TranslatePinInst(&sinucaInst, &ins)) {
                SINUCA3_ERROR_PRINTF("[OnTrace] Failed to translate ins\n");
            }

            if (staticTrace->AddInstruction(&sinucaInst)) {
                SINUCA3_ERROR_PRINTF(
                    "[OnTrace] Failed to add instruction to file\n");
            }

            if (!INS_IsMemoryRead(ins) && !INS_IsMemoryWrite(ins)) {
                continue;
            }

            /*
             * Add call to AppendToMemTrace on every instruction that performs
             * one or more memory accesses.
             */
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AppendToMemTrace,
                           IARG_THREAD_ID, IARG_MULTI_MEMORYACCESS_EA,
                           IARG_END);
        }
    }
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
    // Every register we actually support.
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

    return LEVEL_BASE::REG_INVALID();
}

static inline void SetRegistersInIntrinsicsInfo(REG* arr, unsigned char* num,
                                                char* str) {
    char* sections[MAX_REGISTERS];
    *num = SeparateStringInSections(str, ',', sections,
                                    MAX_REGISTERS);
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

        char* sections[3];
        SeparateStringInSections(strValue, ':', sections, 3);
        char* name = sections[0];
        char* readRegs = sections[1];
        char* writeRegs = sections[2];

        SINUCA3_LOG_PRINTF(
            "Using intrinsic: %s readRegs: %s writeRegs: %s\n",
            name, readRegs, writeRegs);

        intrinsics.push_back(IntrinsicInfo{});
        IntrinsicInfo* i = &intrinsics[intrinsics.size() - 1];

        // Copy the name.
        unsigned int nameSize = strlen(name);
        if (nameSize > INST_MNEMONIC_LEN)
            nameSize = INST_MNEMONIC_LEN - 1;
        memcpy(&i->name[0], name, nameSize + 1);
        strcpy(&i->loaderName[0], "__");
        strcat(&i->loaderName[0], &i->name[0]);
        strcat(&i->loaderName[0], "Loader");

        // Copy registers.
        SetRegistersInIntrinsicsInfo(i->read, &i->numReadRegs, readRegs);
        SetRegistersInIntrinsicsInfo(i->write, &i->numWriteRegs, writeRegs);
    }
}

VOID OnThreadEvent(THREADID tid, UINT32 evType, BOOL isMasterThreadEv) {
    if (!WasThreadCreated(tid)) {
        SINUCA3_ERROR_PRINTF("[OnThreadEvent] thread [%d] not created!\n", tid);
        return;
    }

    PIN_GetLock(&threadAnalysisLock, tid);       

    ThreadEventType thrEv = (ThreadEventType)evType;
    if (threadDataVec[tid]->dynamicTrace.AddThreadEvent(thrEv)) {
        SINUCA3_ERROR_PRINTF("[OnThreadEvent] AddThreadEvent failed!\n");
        return;
    }
    SINUCA3_DEBUG_PRINTF("[OnThreadEvent] added [%u] event to thread "
        "[%d]\n", evType, tid);
    if (isMasterThreadEv) {
        for (unsigned int it = 1; it < threadDataVec.size(); it++) {
            if (threadDataVec[it]->dynamicTrace.AddThreadEvent(thrEv)) {
                SINUCA3_ERROR_PRINTF("[OnThreadEvent] AddThreadEvent fail!\n");
                return;
            }
            SINUCA3_DEBUG_PRINTF("[OnThreadEvent] added [%u] event to thread "
                "[%d]\n", evType, it);
        }
    }

    PIN_ReleaseLock(&threadAnalysisLock);
}

INS FindInstInRtn(RTN rtn, std::string instName) {
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
        if (INS_Mnemonic(ins) == instName) {
            return ins;
        }
    }
    return INS_Invalid();
}

VOID OnImageLoad(IMG img, VOID* ptr) {
    if (!IMG_IsMainExecutable(img)) return;

    std::string absoluteImgPath = IMG_Name(img);
    long size = absoluteImgPath.length() + sizeof('\0');
    char* name = (char*)malloc(size);
    long idx = absoluteImgPath.find_last_of('/') + 1;
    std::string sub = absoluteImgPath.substr(idx);
    strncpy(name, sub.c_str(), size);
    imageName = name;

    SINUCA3_DEBUG_PRINTF("[OnImageLoad] Image name is [%s]\n", imageName);

    /* Only thread master run these calls. */
    std::vector<const char*> ompBarrierMasterStartVec;
    ompBarrierMasterStartVec.push_back("gomp_team_start");
    /* Only thread master run these calls. */
    std::vector<const char*> ompBarrierMasterEndVec;
    ompBarrierMasterEndVec.push_back("gomp_team_end");
    /* All threads run these calls. */
    std::vector<const char*> ompBarrierSimpleVec;
    ompBarrierSimpleVec.push_back(("GOMP_barrier"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_dynamic_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_ordered_static_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_guided_start"));
    ompBarrierSimpleVec.push_back(("GOMP_loop_end"));
    ompBarrierSimpleVec.push_back(("GOMP_parallel_sections_start"));
    ompBarrierSimpleVec.push_back(("GOMP_sections_end"));
    /* Critical region begin. */
    std::vector<const char*> ompCriticalStartVec;
    ompCriticalStartVec.push_back(("GOMP_atomic_start"));
    ompCriticalStartVec.push_back(("GOMP_critical_start"));
    ompCriticalStartVec.push_back(("GOMP_critical_name_start"));
    /* Critical region end. */
    std::vector<const char*> ompCriticalEndVec;
    ompCriticalEndVec.push_back(("GOMP_atomic_end"));
    ompCriticalEndVec.push_back(("GOMP_critical_end"));
    ompCriticalEndVec.push_back(("GOMP_critical_name_end"));
    /* Routines to not instrument. */
    ignoreRtnsVec.push_back("gomp_mutex_lock_slow");
    ignoreRtnsVec.push_back("gomp_sem_wait_slow");
    ignoreRtnsVec.push_back("gomp_ptrlock_get_slow");
    ignoreRtnsVec.push_back("gomp_barrier_wait_end");
    ignoreRtnsVec.push_back("pthread_mutex_lock");
    ignoreRtnsVec.push_back("pthread_mutex_cond_lock");
    ignoreRtnsVec.push_back("pthread_spinlock");
    ignoreRtnsVec.push_back("pthread_mutex_timedlock");

    /* Instrumentation control. */
    const char* INST_START = "BeginInstrumentationBlock";
    const char* INST_END = "EndInstrumentationBlock";

    bool routineTreated;

    staticTrace = new StaticTraceWriter();
    if (staticTrace == NULL) {
        SINUCA3_DEBUG_PRINTF(
            "[OnImageLoad] Failed to create StaticTraceWriter.\n");
        return;
    }
    if (staticTrace->OpenFile(traceDir, imageName)) {
        SINUCA3_DEBUG_PRINTF(
            "[OnImageLoad] Failed to create static trace file.\n");
        return;
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);

            std::string rtnName = RTN_Name(rtn);
            routineTreated = false;
            unsigned int it;

            if (rtnName == INST_START) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                                (AFUNPTR)InitInstrumentation,
                                IARG_END);
                routineTreated = true;
            } else if (rtnName == INST_END) {
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                                (AFUNPTR)StopInstrumentation,
                                IARG_END);
                routineTreated = true;
            }

            if (rtnName.compare(0, 4, "gomp") && rtnName.compare(0, 4, "GOMP")) {
                RTN_Close(rtn);
                continue;
            }

            for (it = 0;
                 it < ompBarrierMasterStartVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompBarrierMasterStartVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_AFTER,
                                    (AFUNPTR)OnThreadEvent,
                                    IARG_THREAD_ID,
                                    IARG_UINT32, ThreadEventBarrierSync,
                                    IARG_BOOL, true,
                                    IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0;
                 it < ompBarrierMasterEndVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompBarrierMasterEndVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                    (AFUNPTR)OnThreadEvent,
                                    IARG_THREAD_ID,
                                    IARG_UINT32, ThreadEventBarrierSync,
                                    IARG_BOOL, true,
                                    IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0;
                 it < ompBarrierSimpleVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompBarrierSimpleVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                    (AFUNPTR)OnThreadEvent,
                                    IARG_THREAD_ID,
                                    IARG_UINT32, ThreadEventBarrierSync,
                                    IARG_BOOL, false,
                                    IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0;
                 it < ompCriticalStartVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompCriticalStartVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                    (AFUNPTR)OnThreadEvent,
                                    IARG_THREAD_ID,
                                    IARG_UINT32, ThreadEventCriticalStart,
                                    IARG_BOOL, false,
                                    IARG_END);
                    routineTreated = true;
                }
            }
            for (it = 0;
                 it < ompCriticalEndVec.size() && !routineTreated;
                 it++) {
                if (rtnName == ompCriticalEndVec[it]) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                    (AFUNPTR)OnThreadEvent,
                                    IARG_THREAD_ID,
                                    IARG_UINT32, ThreadEventCriticalEnd,
                                    IARG_BOOL, false,
                                    IARG_END);
                    routineTreated = true;
                }
            }

            if (!routineTreated) {
                SINUCA3_WARNING_PRINTF("[OnImageLoad] Routine [%s] wasnt " 
                                        "treated!\n", rtnName.c_str());
            }

            for (IntrinsicInfo& intrinsic : intrinsics) {
                if (rtnName == intrinsic.loaderName) {
                    RTN_InsertCall(rtn, IPOINT_BEFORE,
                                   (AFUNPTR)StopInstrumentationInThread,
                                   IARG_THREAD_ID, IARG_END);
                    RTN_InsertCall(rtn, IPOINT_AFTER,
                                   (AFUNPTR)ResumeInstrumentationInThread,
                                   IARG_THREAD_ID, IARG_END);
                    break;
                }
            }

            RTN_Close(rtn);
        }
    }
}

VOID OnFini(INT32 code, VOID* ptr) {
    SINUCA3_DEBUG_PRINTF("[OnFini] Total of [%lu] inst exec and stored!\n",
                         numberOfExecInst);
    SINUCA3_DEBUG_PRINTF("[OnFini] End of tool execution!\n");

    if (imageName) {
        free((void*)imageName);
    }
    if (staticTrace) {
        delete staticTrace;
    }
    if (!wasInitInstrumentationCalled) {
        SINUCA3_DEBUG_PRINTF(
            "[OnFini] No instrumentation blocks were found in the target "
            "program!\n\n");
    }
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    traceDir = knobFolder.Value().c_str();
    if (access(traceDir, F_OK) != 0) {
        mkdir(traceDir, S_IRWXU | S_IRWXG | S_IROTH);
    }

    PIN_InitLock(&threadAnalysisLock);

    if (knobForceInstrumentation.Value()) {
        SINUCA3_WARNING_PRINTF("[main]: Instrumenting entire program\n");
        InitInstrumentation();
    } else {
        isInstrumentating = false;
    }

    LoadIntrinsics();

    IMG_AddInstrumentFunction(OnImageLoad, NULL);
    TRACE_AddInstrumentFunction(OnTrace, NULL);
    PIN_AddFiniFunction(OnFini, NULL);

    PIN_AddThreadStartFunction(OnThreadStart, NULL);
    PIN_AddThreadFiniFunction(OnThreadFini, NULL);

    PIN_StartProgram();

    return 0;
}