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

#include <cstdlib>
#include <cstring>

#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

#define COPY(dest, source, boolean) \
{                                   \
    if (boolean) {                  \
        dest = (source) ? 1 : 0;    \
    } else{                         \
        dest = source;              \
    }                               \
}

namespace sinucaTracer {

int StaticTraceWriter::OpenFile(const char* sourceDir, const char* imageName) {
    unsigned long bufferSize;
    char* path;

    bufferSize = GetPathTidOutSize(sourceDir, "static", imageName);
    path = (char*)alloca(bufferSize);
    FormatPathTidOut(path, sourceDir, "static", imageName, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    /* reserve space for header */
    fseek(this->file, sizeof(this->header), SEEK_SET);

    return 0;
}

int StaticTraceWriter::IsBasicBlockFull() {
    return (this->basicBlockOccupation >= this->basicBlockSize);
}

int StaticTraceWriter::ReallocBasicBlock() {
    if (this->basicBlockSize == 0) {
        this->basicBlockSize = 128;
    }
    this->basicBlockSize <<= 1;
    this->basicBlock =
        (StaticTraceRecord*)realloc(this->basicBlock, this->basicBlockSize);
    return (this->basicBlock == NULL);
}

int StaticTraceWriter::AddBasicBlockSize(unsigned int basicBlockSize) {
    if (this->IsBasicBlockFull()) {
        if (this->ReallocBasicBlock()) {
            SINUCA3_ERROR_PRINTF("Failed to realloc basic block in st trace\n");
            return 1;
        }
    }

    this->basicBlock[0].recordType =
        StaticRecordBasicBlockSize;
    this->basicBlock[0].data.basicBlockSize =
        basicBlockSize;
    ++this->basicBlockOccupation;

    return 0;
}

int StaticTraceWriter::TranslatePinInst(Instruction* inst, const PIN* pinInst) {
     const char* mnemonic = INS_Mnemonic(*pinInst).c_str();
    unsigned long size = sizeof(inst->instructionMnemonic) - 1;
    strncpy(inst->instructionMnemonic, mnemonic, size);
    inst->instructionMnemonic[size] = '\0';

    COPY(inst->instructionOpcode, INS_Opcode(*pinInst), 0);
    COPY(inst->instructionExtension, INS_Extension(*pinInst), 0);
    COPY(inst->instructionOpcode, INS_Opcode(*pinInst), 0);
    COPY(inst->instructionExtension, INS_Extension(*pinInst), 0);
    COPY(inst->effectiveAddressWidth, INS_EffectiveAddressWidth(*pinInst), 0);
    COPY(inst->effectiveAddressWidth, INS_EffectiveAddressWidth(*pinInst), 0);
    COPY(inst->instructionSize, INS_Size(*pinInst), 0);
    COPY(inst->wRegsArrayOccupation, 0, 0);
    COPY(inst->rRegsArrayOccupation, 0, 0);

    COPY(inst->instCausesCacheLineFlush, INS_IsCacheLineFlush(*pinInst), 1);
    COPY(inst->isCallInstruction, INS_IsCall(*pinInst), 1);
    COPY(inst->isSyscallInstruction, INS_IsSyscall(*pinInst), 1);
    COPY(inst->isRetInstruction, INS_IsRet(*pinInst), 1);
    COPY(inst->isSysretInstruction, INS_IsSysret(*pinInst), 1);
    COPY(inst->instHasFallthrough, INS_HasFallThrough(*pinInst), 1);
    COPY(inst->isBranchInstruction, INS_IsBranch(*pinInst), 1);
    COPY(inst->isIndirectCtrlFlowInst, INS_IsIndirectControlFlow(*pinInst),1);
    COPY(inst->instReadsMemory, INS_IsMemoryRead(*pinInst), 1);
    COPY(inst->instWritesMemory, INS_IsMemoryWrite(*pinInst), 1);
    COPY(inst->isPredicatedInst, INS_IsPredicated(*pinInst), 1);

    if (INS_IsPredicated(*pinInst)) {
        COPY(inst->instructionPredicate, INS_GetPredicate(*pinInst), 0);
    }

    for (unsigned int i = 0; i < INS_OperandCount(*pinInst); ++i) {
        if (!INS_OperandIsReg(*pinInst, i)) {
            continue;
        }

        unsigned short reg = INS_OperandReg(*pinInst, i);
        if (INS_OperandRead(*pinInst, i)) {
            if (inst->rRegsArrayOccupation >= sizeof(inst->readRegsArray)) {
                SINUCA3_ERROR_PRINTF(
                    "More registers read than readRegsArray can store\n");
                return 1;
            }
            COPY(inst->readRegsArray[inst->rRegsArrayOccupation], reg, 0);
            ++inst->rRegsArrayOccupation;
        }
        if (INS_OperandWritten(*pinInst, i)) {
            if (inst->wRegsArrayOccupation >= sizeof(inst->writtenRegsArray)) {
                SINUCA3_ERROR_PRINTF(
                    "More registers written than writtenRegsArray can "
                    "store\n");
                return 1;
            }
            COPY(inst->writtenRegsArray[inst->wRegsArrayOccupation], reg, 0);
            ++inst->wRegsArrayOccupation;
        }
    }
}

int StaticTraceWriter::AddInstruction(const INS* pinInst) {
    if (this->IsBasicBlockFull()) {
        if (this->ReallocBasicBlock()) {
            SINUCA3_ERROR_PRINTF("Failed to realloc basic block in st trace\n");
            return 1;
        }
    }

    ++this->basicBlockOccupation;
    this->basicBlock[this->basicBlockOccupation].recordType =
        StaticRecordInstruction;
    Instruction* inst =
        &this->basicBlock[this->basicBlockOccupation].data.instruction;
    this->TranslatePinInst(inst, pinInst);

    return 0;
}

}  // namespace sinucaTracer