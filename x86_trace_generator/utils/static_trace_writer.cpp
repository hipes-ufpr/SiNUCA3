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

#include <cstdio>
#include <sinuca3.hpp>

extern "C" {
#include <alloca.h>
}

int sinucaTracer::StaticTraceWriter::OpenFile(const char* sourceDir,
                                            const char* imgName) {
    unsigned long bufferSize;
    char* path;

    bufferSize = sinucaTracer::GetPathTidOutSize(sourceDir, "static", imgName);
    path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, sourceDir, "static", imgName, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    /* reserve space for header */
    fseek(this->file, sizeof(this->header), SEEK_SET);

    return 0;
}

void sinucaTracer::StaticTraceWriter::ConvertPinInstToRawInstFormat(
    const INS* pinInst) {

    /* fill instruction name */
    std::string instName = INS_Mnemonic(*pinInst);
    unsigned long instNameSize = instName.length();
    if (instNameSize >= MAX_INSTRUCTION_NAME_LENGTH) {
        instNameSize = MAX_INSTRUCTION_NAME_LENGTH - 1;
    }
    memcpy(rawInst->name, instName.c_str(), instNameSize);
    rawInst->name[instNameSize] = '\0';
    rawInst->addr = INS_Address(*pinInst);
    rawInst->size = INS_Size(*pinInst);
    rawInst->baseReg = INS_MemoryBaseReg(*pinInst);
    rawInst->indexReg = INS_MemoryIndexReg(*pinInst);
    /* fill single bit fields */
    rawInst->isPredicated = (INS_IsPredicated(*pinInst)) ? 1 : 0;
    rawInst->isPrefetch = (INS_IsPrefetch(*pinInst)) ? 1 : 0;
    rawInst->isNonStandardMemOp = (!INS_IsStandardMemop(*pinInst)) ? 1 : 0;
    if (!rawInst->isNonStandardMemOp) {
        rawInst->numStdMemReadOps = (INS_IsMemoryRead(*pinInst)) ? 1 : 0;
        rawInst->numStdMemReadOps += (INS_HasMemoryRead2(*pinInst)) ? 1 : 0;
        rawInst->numStdMemWriteOps = (INS_IsMemoryWrite(*pinInst)) ? 1 : 0;
    }
    /* fill branch related fields */
    bool isSyscall = INS_IsSyscall(*pinInst);
    rawInst->isControlFlow = (INS_IsControlFlow(*pinInst) || isSyscall) ? 1 : 0;
    if (rawInst->isControlFlow) {
        if (isSyscall)
            rawInst->branchType = BRANCH_SYSCALL;
        else if (INS_IsCall(*pinInst))
            rawInst->branchType = BRANCH_CALL;
        else if (INS_IsRet(*pinInst))
            rawInst->branchType = BRANCH_RETURN;
        else if (INS_HasFallThrough(*pinInst))
            rawInst->branchType = BRANCH_COND;
        else
            rawInst->branchType = BRANCH_UNCOND;
        rawInst->isIndirectControlFlow =
            INS_IsIndirectControlFlow(*pinInst) ? 1 : 0;
    }
    /* fill used register info */
    unsigned int operandCount = INS_OperandCount(*pinInst);
    rawInst->numReadRegs = rawInst->numWriteRegs = 0;
    for (unsigned int i = 0; i < operandCount; ++i) {
        if (!INS_OperandIsReg(*pinInst, i)) {
            continue;
        }
        if (INS_OperandWritten(*pinInst, i)) {
            if (rawInst->numWriteRegs > MAX_REG_OPERANDS) return;
            rawInst->writeRegs[rawInst->numWriteRegs++] =
                INS_OperandReg(*pinInst, i);
        }
        if (INS_OperandRead(*pinInst, i)) {
            if (rawInst->numReadRegs > MAX_REG_OPERANDS) return;
            rawInst->readRegs[rawInst->numReadRegs++] =
                INS_OperandReg(*pinInst, i);
        }
    }
}