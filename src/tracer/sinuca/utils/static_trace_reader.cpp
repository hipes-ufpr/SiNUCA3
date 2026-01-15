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
 * @file x86_reader_file_handler.cpp
 * @brief Implementation of the x86_64 trace reader.
 */

#include "static_trace_reader.hpp"

extern "C" {
#include <alloca.h>
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

int StaticTraceReader::OpenFile(const char *sourceDir, const char *imgName) {
    unsigned long bufferLen;
    char *staticPath;

    bufferLen = GetPathTidOutSize(sourceDir, "static", imgName);
    staticPath = (char *)alloca(bufferLen);
    FormatPathTidOut(staticPath, sourceDir, "static", imgName, bufferLen);

    /* get file descriptor */
    this->fileDescriptor = open(staticPath, O_RDONLY);
    if (fileDescriptor == -1) {
        printFileErrorLog(staticPath, "O_RDONLY");
        return 1;
    }
    /* get file size */
    this->mmapSize = lseek(this->fileDescriptor, 0, SEEK_END);
    /* map file to virtual memory */
    this->mmapPtr = (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE,
                                 this->fileDescriptor, 0);
    if (this->mmapPtr == MAP_FAILED) {
        printFileErrorLog(staticPath, "PROT_READ MAP_PRIVATE");
        return 1;
    }

    this->header.LoadHeader(this->mmapPtr, &this->mmapOffset);

    return 0;
}

void *StaticTraceReader::ReadData(unsigned long len) {
    if (this->mmapOffset >= this->mmapSize) return NULL;
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

int StaticTraceReader::ReadStaticRecordFromFile() {
    if (this->fileDescriptor == -1) return 1;
    void *readData = this->ReadData(sizeof(*this->record));
    if (readData == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to read static trace record\n");
        return 1;
    }
    this->record = (StaticTraceRecord *)readData;
    return 0;
}

void StaticTraceReader::TranslateRawInstructionToSinucaInst(
    StaticInstructionInfo *instInfo) {
    if (instInfo == NULL) return;

    Instruction *rawInst = &this->record->data.instruction;

    strcpy(instInfo->instMnemonic, rawInst->instructionMnemonic);

    instInfo->instSize = rawInst->instructionSize;
    instInfo->instAddress = rawInst->instructionAddress;
    instInfo->instPerformsAtomicUpdate = rawInst->instPerformsAtomicUpdate;
    instInfo->instCausesCacheLineFlush = rawInst->instCausesCacheLineFlush;
    instInfo->isPredicatedInst = rawInst->isPredicatedInst;
    instInfo->instReadsMemory = rawInst->instReadsMemory;
    instInfo->instWritesMemory = rawInst->instWritesMemory;
    instInfo->isIndirectControlFlowInst = rawInst->isIndirectCtrlFlowInst;
    instInfo->numberOfReadRegs = rawInst->rRegsArrayOccupation;
    instInfo->numberOfWriteRegs = rawInst->wRegsArrayOccupation;

    unsigned long occupationInBytes;

    occupationInBytes =
        sizeof(*rawInst->readRegsArray) * rawInst->rRegsArrayOccupation;
    memcpy(instInfo->readRegsArray, rawInst->readRegsArray, occupationInBytes);

    occupationInBytes =
        sizeof(*rawInst->writtenRegsArray) * rawInst->wRegsArrayOccupation;
    memcpy(instInfo->writtenRegsArray, rawInst->writtenRegsArray,
           occupationInBytes);

    if (rawInst->isCallInstruction) {
        instInfo->branchType = BranchCall;
    } else if (rawInst->isSyscallInstruction) {
        instInfo->branchType = BranchSyscall;
    } else if (rawInst->isRetInstruction) {
        instInfo->branchType = BranchRet;
    } else if (rawInst->isSysretInstruction) {
        instInfo->branchType = BranchSysret;
    } else if (rawInst->isBranchInstruction) {
        if (rawInst->instHasFallthrough) {
            instInfo->branchType = BranchCond;
        } else {
            instInfo->branchType = BranchUncond;
        }
    }

    this->record = NULL;
}