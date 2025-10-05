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

extern "C" {
#include <alloca.h>
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

int StaticTraceWriter::DoubleRecordArraySize() {
    if (this->recordArraySize == 0) {
        this->recordArraySize = 128;
    }
    this->recordArraySize <<= 1;
    this->recordArray =
        (StaticTraceRecord*)realloc(this->recordArray, this->recordArraySize);
    return this->recordArray == NULL;
}

int StaticTraceWriter::AddBasicBlockSize(unsigned int basicBlockSize) {
    if (this->recordArrayOccupation >= this->recordArraySize) {
        if (this->DoubleRecordArraySize()) {
            return 1;
        }
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        StaticRecordBasicBlockSize;
    this->recordArray[this->recordArrayOccupation].data.basicBlockSize =
        basicBlockSize;
    ++this->recordArrayOccupation;

    return 0;
}

int StaticTraceWriter::AddInstruction(const INS* pinInst) {
    if (this->recordArrayOccupation >= this->recordArraySize) {
        if (this->DoubleRecordArraySize()) {
            return 1;
        }
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        StaticRecordInstruction;
    Instruction* inst =
        &this->recordArray[this->recordArrayOccupation].data.instruction;

    unsigned long writtenSize;
    const char* mnemonic = INS_Mnemonic(*pinInst).c_str();
    unsigned long mnemonicSize = strlen(mnemonic);
    writtenSize = std::min(mnemonicSize, sizeof(inst->instructionMnemonic));
    memcpy(inst->instructionMnemonic, mnemonic, writtenSize);
    inst->instructionMnemonic[writtenSize - 1] = '\0';

    inst->instructionOpcode = INS_Opcode(*pinInst);
    inst->instructionExtension = INS_Extension(*pinInst);
    inst->effectiveAddressWidth = INS_EffectiveAddressWidth(*pinInst);
    inst->instructionSize = INS_Size(*pinInst);

    inst->instCausesCacheLineFlush =
        this->ToUnsignedChar(INS_IsCacheLineFlush(*pinInst));
    inst->isCallInstruction = this->ToUnsignedChar(INS_IsCall(*pinInst));
    inst->isSyscallInstruction = this->ToUnsignedChar(INS_IsSyscall(*pinInst));
    inst->isRetInstruction = this->ToUnsignedChar(INS_IsRet(*pinInst));
    inst->isSysretInstruction = this->ToUnsignedChar(INS_IsSysret(*pinInst));
    inst->instHasFallthrough =
        this->ToUnsignedChar(INS_HasFallThrough(*pinInst));
    inst->isBranchInstruction = this->ToUnsignedChar(INS_IsBranch(*pinInst));
    inst->isIndirectControlFlowInst =
        this->ToUnsignedChar(INS_IsIndirectControlFlow(*pinInst));
    inst->instReadsMemory = this->ToUnsignedChar(INS_IsMemoryRead(*pinInst));
    inst->instWritesMemory = this->ToUnsignedChar(INS_IsMemoryWrite(*pinInst));
    inst->isPredicatedInst = this->ToUnsignedChar(INS_IsPredicated(*pinInst));

    if (inst->isPredicatedInst) {
        inst->instructionPredicate = INS_GetPredicate(*pinInst);
    }

    inst->wRegsArrayOccupation = 0;
    inst->rRegsArrayOccupation = 0;
    int wRegsArraySize = sizeof(inst->writtenRegsArray);
    int rRegsArraySize = sizeof(inst->readRegsArray);

    for (unsigned int i = 0; i < INS_OperandCount(*pinInst); ++i) {
        if (INS_OperandIsReg(*pinInst, i)) {
            unsigned short reg = INS_OperandReg(*pinInst, i);
            if (INS_OperandRead(*pinInst, i)) {
                if (inst->rRegsArrayOccupation >= rRegsArraySize) {
                    SINUCA3_ERROR_PRINTF(
                        "More registers read than readRegsArray can store\n");
                    return 1;
                }
                inst->readRegsArray[inst->rRegsArrayOccupation] = reg;
                ++inst->rRegsArrayOccupation;
            }
            if (INS_OperandWritten(*pinInst, i)) {
                if (inst->wRegsArrayOccupation >= wRegsArraySize) {
                    SINUCA3_ERROR_PRINTF(
                        "More registers written than writtenRegsArray can "
                        "store\n");
                    return 1;
                }
                inst->writtenRegsArray[inst->wRegsArrayOccupation] = reg;
                ++inst->wRegsArrayOccupation;
            }
        }
    }

    ++this->recordArrayOccupation;

    return 0;
}

unsigned char StaticTraceWriter::ToUnsignedChar(bool val) {
    return (val == true) ? BoolTypeTrue : BoolTypeFalse;
}

}  // namespace sinucaTracer