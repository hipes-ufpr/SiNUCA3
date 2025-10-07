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
#include <string>

#include "utils/logging.hpp"

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
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc this->file\n");
        return 1;
    }
    this->header.ReserveHeaderSpace(this->file);

    return 0;
}

int StaticTraceWriter::ReallocBasicBlock() {
    if (this->basicBlockArraySize == 0) {
        this->basicBlockArraySize = 128;
    }
    this->basicBlockArraySize <<= 1;
    this->basicBlock = (StaticTraceRecord*)realloc(this->basicBlock,
                                                   this->basicBlockArraySize);

    return (this->basicBlock == NULL);
}

int StaticTraceWriter::AddBasicBlockSize(unsigned int basicBlockSize) {
    if (this->IsBasicBlockArrayFull()) {
        if (this->ReallocBasicBlock()) {
            SINUCA3_ERROR_PRINTF("[1] Failed to realloc basic block!\n");
            return 1;
        }
    }

    if (!this->WasBasicBlockReset()) {
        SINUCA3_ERROR_PRINTF("Basic block control variables were not reset!\n");
    }
    this->basicBlock[0].recordType = StaticRecordBasicBlockSize;
    this->basicBlock[0].data.basicBlockSize = basicBlockSize;
    this->currentBasicBlockSize = basicBlockSize;

    if (this->IsBasicBlockReadyToFlush()) {
        if (this->FlushBasicBlock()) {
            SINUCA3_ERROR_PRINTF("[1] Failed to flush basic block!\n");
            return 1;
        }
        this->ResetBasicBlock();
    }

    return 0;
}

int StaticTraceWriter::TranslatePinInst(Instruction* inst, const INS* pinInst) {
    memset(inst, 0, sizeof(*inst));

    std::string mnemonic = INS_Mnemonic(*pinInst);
    unsigned long size = sizeof(inst->instructionMnemonic) - 1;
    strncpy(inst->instructionMnemonic, mnemonic.c_str(), size);
    if (size < mnemonic.size()) {
        SINUCA3_WARNING_PRINTF("Insufficient space to store inst mnemonic\n");
    }

    inst->instructionAddress = INS_Address(*pinInst);
    /* 16, 32 or 64 bits */
    inst->effectiveAddressWidth = INS_EffectiveAddressWidth(*pinInst);
    /* at most 15 bytes len (for now) */
    inst->instructionSize = INS_Size(*pinInst);
    /* manual flush with CLFLUSH/CLFLUSHOPT/CLWB/WBINVD/INVD */
    /* or cache coherence induced flush */
    inst->instCausesCacheLineFlush = INS_IsCacheLineFlush(*pinInst);
    /* false for any instruction which in practice is a system call */
    inst->isCallInstruction = INS_IsCall(*pinInst);
    inst->isSyscallInstruction = INS_IsSyscall(*pinInst);
    /* i guess its false if inst is a sysret, need to test */
    inst->isRetInstruction = INS_IsRet(*pinInst);
    inst->isSysretInstruction = INS_IsSysret(*pinInst);
    /* false for unconditional branches and calls */
    inst->instHasFallthrough = INS_HasFallThrough(*pinInst);
    /* false for any instruction which in practice is a system call */
    inst->isBranchInstruction = INS_IsBranch(*pinInst);
    inst->isIndirectCtrlFlowInst = INS_IsIndirectControlFlow(*pinInst);
    /* field checked before reading from memory trace */
    inst->instReadsMemory = INS_IsMemoryRead(*pinInst);
    inst->instWritesMemory = INS_IsMemoryWrite(*pinInst);
    /* e.g. CMOV */
    inst->isPredicatedInst = INS_IsPredicated(*pinInst);
    if (INS_IsPredicated(*pinInst)) {
        inst->instructionPredicate = INS_GetPredicate(*pinInst);
    }

    for (unsigned int i = 0; i < INS_OperandCount(*pinInst); ++i) {
        /* interest only in register operands */
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
            inst->readRegsArray[inst->rRegsArrayOccupation] = reg;
            ++inst->rRegsArrayOccupation;
        }
        if (INS_OperandWritten(*pinInst, i)) {
            if (inst->wRegsArrayOccupation >= sizeof(inst->writtenRegsArray)) {
                SINUCA3_ERROR_PRINTF(
                    "More registers written than writtenRegsArray can "
                    "store\n");
                return 1;
            }
            inst->writtenRegsArray[inst->wRegsArrayOccupation] = reg;
            ++inst->wRegsArrayOccupation;
        }
    }

    return 0;
}

int StaticTraceWriter::AddInstruction(const INS* pinInst) {
    if (this->IsBasicBlockArrayFull()) {
        if (this->ReallocBasicBlock()) {
            SINUCA3_ERROR_PRINTF("[2] Failed to realloc basic block!\n");
            return 1;
        }
    }

    Instruction* inst =
        &this->basicBlock[this->basicBlockOccupation].data.instruction;

    this->basicBlock[this->basicBlockOccupation].recordType =
        StaticRecordInstruction;
    if (this->TranslatePinInst(inst, pinInst)) {
        SINUCA3_ERROR_PRINTF("Failed to properly translate instruction!\n");
    }

    ++this->basicBlockOccupation;

    if (this->IsBasicBlockReadyToFlush()) {
        if (this->FlushBasicBlock()) {
            SINUCA3_ERROR_PRINTF("[2] Failed to flush basic block!\n");
            return 1;
        }
        this->ResetBasicBlock();
    }

    return 0;
}

int StaticTraceWriter::FlushBasicBlock() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[3] File pointer is nil in static trace obj!\n");
        return 1;
    }

    unsigned long occupationInBytes =
        this->basicBlockOccupation * sizeof(*basicBlock);

    if (fwrite(this->basicBlock, 1, occupationInBytes, file) !=
        occupationInBytes) {
        SINUCA3_ERROR_PRINTF("Failed to flush static records!\n");
        return 1;
    }

    return 0;
}

}  // namespace sinucaTracer