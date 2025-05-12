#include "x86_generator_file_handler.hpp"

#include <cstddef>

#include "../src/utils/logging.hpp"

trace::traceGenerator::StaticTraceFile::StaticTraceFile(std::string source,
                                                        std::string img)
    : TraceFileWriter(FormatPathTidOut(source, "static", img)) {
    this->threadCount = 0;
    this->bblCount = 0;
    this->instCount = 0;

    /*
     * This space will be used to store the total amount of BBLs,
     * number of instructions, and total number of threads
     */
    fseek(this->tf.file, 3 * sizeof(unsigned int), SEEK_SET);
}

trace::traceGenerator::StaticTraceFile::~StaticTraceFile() {
    this->FlushBuffer();
    rewind(this->tf.file);
    fwrite(&this->threadCount, sizeof(this->threadCount), 1, this->tf.file);
    fwrite(&this->bblCount, sizeof(this->bblCount), 1, this->tf.file);
    fwrite(&this->instCount, sizeof(this->instCount), 1, this->tf.file);
}

void trace::traceGenerator::StaticTraceFile::PrepareData(struct DataINS* data,
                                                         const INS* ins) {
    std::string insName = INS_Mnemonic(*ins);
    size_t nameSize = insName.size();
    if (nameSize >= MAX_INSTRUCTION_NAME_LENGTH) {
        nameSize = MAX_INSTRUCTION_NAME_LENGTH - 1;
    }
    memcpy(data->name, insName.c_str(), nameSize);
    data->name[nameSize] = '\0';

    data->addr = INS_Address(*ins);
    data->size = INS_Size(*ins);
    data->baseReg = INS_MemoryBaseReg(*ins);
    data->indexReg = INS_MemoryIndexReg(*ins);

    this->ResetFlags(data);
    this->SetFlags(data, ins);
    this->SetBranchFields(data, ins);
    this->FillRegs(data, ins);
}

void trace::traceGenerator::StaticTraceFile::StAppendToBuffer(void* ptr,
                                                              size_t len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

trace::traceGenerator::DynamicTraceFile::DynamicTraceFile(std::string source,
                                                          std::string img,
                                                          THREADID tid)
    : TraceFileWriter(FormatPathTidIn(source, "dynamic", img, tid)) {}

trace::traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last DynamicTraceFile flush\n");
    if (this->tf.offset > 0) {
        this->FlushBuffer();
    }
}

void trace::traceGenerator::DynamicTraceFile::DynAppendToBuffer(void* ptr,
                                                                size_t len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

trace::traceGenerator::MemoryTraceFile::MemoryTraceFile(std::string source,
                                                        std::string img,
                                                        THREADID tid)
    : TraceFileWriter(FormatPathTidIn(source, "memory", img, tid)) {}

trace::traceGenerator::MemoryTraceFile::~MemoryTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last MemoryTraceFile flush\n");
    if (this->tf.offset > 0) {
        this->FlushLenBytes(&this->tf.offset, sizeof(this->tf.offset));
        this->FlushBuffer();
    }
}

void trace::traceGenerator::MemoryTraceFile::PrepareDataNonStdAccess(
    unsigned short* numR, struct DataMEM r[], unsigned short* numW,
    struct DataMEM w[], PIN_MULTI_MEM_ACCESS_INFO* info) {
    *numR = *numW = 0;
    for (unsigned short it = 0; it < info->numberOfMemops; it++) {
        PIN_MEM_ACCESS_INFO* memOp = &info->memop[it];
        if (memOp->memopType == PIN_MEMOP_LOAD) {
            r[*numR].addr = memOp->memoryAddress;
            r[*numR].size = memOp->bytesAccessed;
            (*numR)++;
        } else {
            w[*numW].addr = memOp->memoryAddress;
            w[*numW].size = memOp->bytesAccessed;
            (*numW)++;
        }
    }
}

void trace::traceGenerator::MemoryTraceFile::MemAppendToBuffer(void* ptr,
                                                               size_t len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushLenBytes(&this->tf.offset, sizeof(this->tf.offset));
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

void trace::traceGenerator::StaticTraceFile::ResetFlags(struct DataINS* data) {
    // Maybe memset is an alternative IDK
    data->isControlFlow = 0;
    data->isPredicated = 0;
    data->isPrefetch = 0;
    data->isIndirectControlFlow = 0;
    data->isNonStandardMemOp = 0;
    data->isRead = 0;
    data->isRead2 = 0;
    data->isWrite = 0;
}

void trace::traceGenerator::StaticTraceFile::SetFlags(struct DataINS* data,
                                                      const INS* ins) {
    if (INS_IsPredicated(*ins)) {
        data->isPredicated = 1;
    }
    if (INS_IsPrefetch(*ins)) {
        data->isPrefetch = 1;
    }

    /*
     * INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise
     */
    if (!INS_IsStandardMemop(*ins)) {
        data->isNonStandardMemOp = 1;
    } else {
        if (INS_IsMemoryRead(*ins)) {
            data->isRead = 1;
        }
        if (INS_HasMemoryRead2(*ins)) {
            data->isRead2 = 1;
        }
        if (INS_IsMemoryWrite(*ins)) {
            data->isWrite = 1;
        }
    }
}

void trace::traceGenerator::StaticTraceFile::SetBranchFields(
    struct DataINS* data, const INS* ins) {
    bool isSyscall = INS_IsSyscall(*ins);
    bool isControlFlow = INS_IsControlFlow(*ins) || isSyscall;

    if (isControlFlow) {
        if (isSyscall)
            data->branchType = sinuca::BranchSyscall;
        else if (INS_IsCall(*ins))
            data->branchType = sinuca::BranchCall;
        else if (INS_IsRet(*ins))
            data->branchType = sinuca::BranchReturn;
        else if (INS_HasFallThrough(*ins))
            data->branchType = sinuca::BranchCond;
        else
            data->branchType = sinuca::BranchUncond;

        data->isControlFlow = 1;
        if (INS_IsIndirectControlFlow(*ins)) {
            data->isIndirectControlFlow = 1;
        }
    }
}

void trace::traceGenerator::StaticTraceFile::FillRegs(struct DataINS* data,
                                                      const INS* ins) {
    unsigned int operandCount = INS_OperandCount(*ins);
    data->numReadRegs = data->numWriteRegs = 0;
    for (unsigned int i = 0; i < operandCount; ++i) {
        if (!INS_OperandIsReg(*ins, i)) {
            continue;
        }

        if (INS_OperandWritten(*ins, i)) {
            assert(data->numWriteRegs < MAX_REG_OPERANDS && "[FillRegs] Error");
            data->writeRegs[data->numWriteRegs++] = INS_OperandReg(*ins, i);
        }
        if (INS_OperandRead(*ins, i)) {
            assert(data->numReadRegs < MAX_REG_OPERANDS && "[FillRegs] Error");
            data->readRegs[data->numReadRegs++] = INS_OperandReg(*ins, i);
        }
    }

    SINUCA3_DEBUG_PRINTF("Number Read Regs [%d]\n", data->numReadRegs);
    SINUCA3_DEBUG_PRINTF("Number Write Regs [%d]\n", data->numWriteRegs);
}
