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
 * @file static_trace_writer.cpp
 * @details Implementation of StaticTraceFile class.
 */

#include "static_trace_writer.hpp"

#include <cassert>
#include <string>

#include "../../src/utils/logging.hpp"
extern "C" {
#include <alloca.h>
}

tracer::traceGenerator::StaticTraceFile::StaticTraceFile(const char* source,
                                                        const char* img) {
    unsigned long bufferSize = tracer::GetPathTidOutSize(source, "static", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, source, "static", img, bufferSize);

    this->::tracer::TraceFileWriter::UseFile(path);

    this->threadCount = 0;
    this->bblCount = 0;
    this->instCount = 0;

    /*
     * This space will be used to store the total amount of BBLs,
     * number of instructions, and number of threads.
     */
    fseek(this->tf.file, 3 * sizeof(unsigned int), SEEK_SET);
}

tracer::traceGenerator::StaticTraceFile::~StaticTraceFile() {
    this->FlushBuffer();
    rewind(this->tf.file);
    fwrite(&this->threadCount, sizeof(this->threadCount), 1, this->tf.file);
    fwrite(&this->bblCount, sizeof(this->bblCount), 1, this->tf.file);
    fwrite(&this->instCount, sizeof(this->instCount), 1, this->tf.file);
}

void tracer::traceGenerator::StaticTraceFile::PrepareDataINS(const INS* ins) {
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

void tracer::traceGenerator::StaticTraceFile::AppendToBufferDataINS() {
    this->StaticAppendToBuffer(&this->data, sizeof(this->data));
}

void tracer::traceGenerator::StaticTraceFile::AppendToBufferNumIns(
    unsigned int numIns) {
    this->StaticAppendToBuffer(&numIns, SIZE_NUM_BBL_INS);
}

void tracer::traceGenerator::StaticTraceFile::StaticAppendToBuffer(
    void* ptr, unsigned long len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}

void tracer::traceGenerator::StaticTraceFile::ResetFlags() {
    this->data.isControlFlow = 0;
    this->data.isPredicated = 0;
    this->data.isPrefetch = 0;
    this->data.isIndirectControlFlow = 0;
    this->data.isNonStandardMemOp = 0;
    this->data.isRead = 0;
    this->data.isRead2 = 0;
    this->data.isWrite = 0;
}

void tracer::traceGenerator::StaticTraceFile::SetFlags(const INS* ins) {
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

void tracer::traceGenerator::StaticTraceFile::SetBranchFields(const INS* ins) {
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

void tracer::traceGenerator::StaticTraceFile::FillRegs(const INS* ins) {
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
