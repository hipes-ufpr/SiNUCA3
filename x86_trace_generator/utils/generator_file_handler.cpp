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
 * @file generator_file_handler.cpp
 * @details Implementation of the SiNUCA3 x86_64 tracer.
 */

#include "generator_file_handler.hpp"

#include <cassert>
#include <cstddef>
#include <string>

#include "../../src/utils/logging.hpp"
extern "C" {
#include <alloca.h>
}

trace::traceGenerator::StaticTraceFile::StaticTraceFile(const char* source,
                                                        const char* img) {
    unsigned long bufferSize = trace::GetPathTidOutSize(source, "static", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, source, "static", img, bufferSize);

    this->::trace::TraceFileWriter::UseFile(path);

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

void trace::traceGenerator::StaticTraceFile::PrepareDataINS(const INS* ins) {
    std::string insName = INS_Mnemonic(*ins);
    unsigned long nameSize = insName.size();
    if (nameSize >= MAX_INSTRUCTION_NAME_LENGTH) {
        nameSize = MAX_INSTRUCTION_NAME_LENGTH - 1;
    }
    memcpy(this->data.name, insName.c_str(), nameSize);
    this->data.name[nameSize] = '\0';

    this->data.addr = INS_Address(*ins);
    this->data.size = INS_Size(*ins);
    this->data.baseReg = INS_MemoryBaseReg(*ins);
    this->data.indexReg = INS_MemoryIndexReg(*ins);

    this->ResetFlags();
    this->SetFlags(ins);
    this->SetBranchFields(ins);
    this->FillRegs(ins);
}

void trace::traceGenerator::StaticTraceFile::AppendToBufferDataINS() {
    this->StaticAppendToBuffer(&this->data, sizeof(this->data));
}

void trace::traceGenerator::StaticTraceFile::AppendToBufferNumIns(
    unsigned int numIns) {
    this->StaticAppendToBuffer(&numIns, SIZE_NUM_BBL_INS);
}

void trace::traceGenerator::StaticTraceFile::StaticAppendToBuffer(
    void* ptr, unsigned long len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

void trace::traceGenerator::StaticTraceFile::ResetFlags() {
    this->data.isControlFlow = 0;
    this->data.isPredicated = 0;
    this->data.isPrefetch = 0;
    this->data.isIndirectControlFlow = 0;
    this->data.isNonStandardMemOp = 0;
    this->data.isRead = 0;
    this->data.isRead2 = 0;
    this->data.isWrite = 0;
}

void trace::traceGenerator::StaticTraceFile::SetFlags(const INS* ins) {
    if (INS_IsPredicated(*ins)) {
        this->data.isPredicated = 1;
    }
    if (INS_IsPrefetch(*ins)) {
        this->data.isPrefetch = 1;
    }

    /*
     * INS_IsStandardMemop() returns false if this instruction has a memory
     * operand which has unconventional meaning; returns true otherwise
     */
    if (!INS_IsStandardMemop(*ins)) {
        this->data.isNonStandardMemOp = 1;
    } else {
        if (INS_IsMemoryRead(*ins)) {
            this->data.isRead = 1;
        }
        if (INS_HasMemoryRead2(*ins)) {
            this->data.isRead2 = 1;
        }
        if (INS_IsMemoryWrite(*ins)) {
            this->data.isWrite = 1;
        }
    }
}

void trace::traceGenerator::StaticTraceFile::SetBranchFields(const INS* ins) {
    bool isSyscall = INS_IsSyscall(*ins);
    bool isControlFlow = INS_IsControlFlow(*ins) || isSyscall;

    if (isControlFlow) {
        if (isSyscall)
            this->data.branchType = BRANCH_SYSCALL;
        else if (INS_IsCall(*ins))
            this->data.branchType = BRANCH_CALL;
        else if (INS_IsRet(*ins))
            this->data.branchType = BRANCH_RETURN;
        else if (INS_HasFallThrough(*ins))
            this->data.branchType = BRANCH_COND;
        else
            this->data.branchType = BRANCH_UNCOND;

        this->data.isControlFlow = 1;
        if (INS_IsIndirectControlFlow(*ins)) {
            this->data.isIndirectControlFlow = 1;
        }
    }
}

void trace::traceGenerator::StaticTraceFile::FillRegs(const INS* ins) {
    unsigned int operandCount = INS_OperandCount(*ins);
    this->data.numReadRegs = this->data.numWriteRegs = 0;
    for (unsigned int i = 0; i < operandCount; ++i) {
        if (!INS_OperandIsReg(*ins, i)) {
            continue;
        }

        if (INS_OperandWritten(*ins, i)) {
            assert(this->data.numWriteRegs < MAX_REG_OPERANDS &&
                   "[FillRegs] Error");
            this->data.writeRegs[this->data.numWriteRegs++] =
                INS_OperandReg(*ins, i);
        }
        if (INS_OperandRead(*ins, i)) {
            assert(this->data.numReadRegs < MAX_REG_OPERANDS &&
                   "[FillRegs] Error");
            this->data.readRegs[this->data.numReadRegs++] =
                INS_OperandReg(*ins, i);
        }
    }
}

trace::traceGenerator::DynamicTraceFile::DynamicTraceFile(const char* source,
                                                          const char* img,
                                                          THREADID tid) {
    unsigned long bufferSize = trace::GetPathTidInSize(source, "dynamic", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, source, "dynamic", img, tid, bufferSize);

    this->::trace::TraceFileWriter::UseFile(path);
}

trace::traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last DynamicTraceFile flush\n");
    if (this->tf.offset > 0) {
        this->FlushBuffer();
    }
}

void trace::traceGenerator::DynamicTraceFile::PrepareId(BBLID id) {
    this->bblId = id;
}

void trace::traceGenerator::DynamicTraceFile::AppendToBufferId() {
    this->DynamicAppendToBuffer(&this->bblId, sizeof(this->bblId));
}

void trace::traceGenerator::DynamicTraceFile::DynamicAppendToBuffer(
    void* ptr, unsigned long len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

trace::traceGenerator::MemoryTraceFile::MemoryTraceFile(const char* source,
                                                        const char* img,
                                                        THREADID tid) {
    unsigned long bufferSize = trace::GetPathTidInSize(source, "memory", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, source, "memory", img, tid, bufferSize);

    this->::trace::TraceFileWriter::UseFile(path);
}

trace::traceGenerator::MemoryTraceFile::~MemoryTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last MemoryTraceFile flush\n");
    if (this->tf.offset > 0) {
        this->FlushLenBytes(&this->tf.offset, sizeof(this->tf.offset));
        this->FlushBuffer();
    }
}

void trace::traceGenerator::MemoryTraceFile::PrepareDataNonStdAccess(
    PIN_MULTI_MEM_ACCESS_INFO* pinNonStdInfo) {
    this->numReadOps = 0;
    this->numWriteOps = 0;

    for (unsigned int it = 0; it < pinNonStdInfo->numberOfMemops; ++it) {
        if (pinNonStdInfo->memop[it].memopType == PIN_MEMOP_LOAD) {
            this->readOps[this->numReadOps].addr =
                pinNonStdInfo->memop[it].memoryAddress;
            this->readOps[this->numReadOps].size =
                pinNonStdInfo->memop[it].bytesAccessed;
            this->numReadOps++;
        } else {
            this->readOps[this->numReadOps].addr =
                pinNonStdInfo->memop[it].memoryAddress;
            this->writeOps[this->numWriteOps].size =
                pinNonStdInfo->memop[it].bytesAccessed;
            this->numWriteOps++;
        }
    }
    // This variable is checked in AppendToBufferLastMemoryAccess
    this->wasLastOperationStd = false;
}

void trace::traceGenerator::MemoryTraceFile::PrepareDataStdMemAccess(
    unsigned long addr, unsigned int opSize) {
    this->stdAccessOp.addr = addr;
    this->stdAccessOp.size = opSize;
    // This variable is checked in AppendToBufferLastMemoryAccess
    this->wasLastOperationStd = true;
}

void trace::traceGenerator::MemoryTraceFile::AppendToBufferLastMemoryAccess() {
    if (this->wasLastOperationStd) {
        this->MemoryAppendToBuffer(&this->stdAccessOp,
                                   sizeof(this->stdAccessOp));
    } else {
        // Append number of read operations
        this->MemoryAppendToBuffer(&this->numReadOps, SIZE_NUM_MEM_R_W);
        // Append number of write operations
        this->MemoryAppendToBuffer(&this->numWriteOps, SIZE_NUM_MEM_R_W);
        // Append read operations' buffer
        this->MemoryAppendToBuffer(this->readOps,
                                   this->numReadOps * sizeof(*this->readOps));
        // Append write operations' buffer
        this->MemoryAppendToBuffer(this->writeOps,
                                   this->numWriteOps * sizeof(*this->writeOps));
    }
}

void trace::traceGenerator::MemoryTraceFile::MemoryAppendToBuffer(void* ptr,
                                                                  size_t len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushLenBytes(&this->tf.offset, sizeof(unsigned long));
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}