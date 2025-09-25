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

#include <cstring>

#include "engine/default_packets.hpp"
#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

int sinucaTracer::StaticTraceFile::OpenFile(const char *sourceDir,
                                            const char *imgName) {
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
    SINUCA3_DEBUG_PRINTF("Mmap Size [%lu]\n", this->mmapSize);
    /* map file to virtual memory */
    this->mmapPtr = (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE,
                                 this->fileDescriptor, 0);
    if (this->mmapPtr == MAP_FAILED) {
        printFileErrorLog(staticPath, "PROT_READ MAP_PRIVATE");
        return 1;
    }

    return 0;
}

sinucaTracer::StaticTraceFile::~StaticTraceFile() {
    if (this->mmapPtr == NULL) return;
    munmap(this->mmapPtr, this->mmapSize);
    if (this->fileDescriptor == -1) return;
    close(this->fileDescriptor);
}

void *sinucaTracer::StaticTraceFile::ReadData(unsigned long len) {
    if (this->mmapOffset >= this->mmapSize) return NULL;
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

int sinucaTracer::StaticTraceFile::ReadFileHeader() {
    if (this->fileDescriptor == -1) return 1;
    void *readData = this->ReadData(sizeof(FileHeader));
    if (readData == NULL) return 1;
    this->header = *(FileHeader *)(readData);
    return 0;
}

int sinucaTracer::StaticTraceFile::ReadBasicBlock() {
    if (this->fileDescriptor == -1) return 1;
    void *readData;
    readData = this->ReadData(sizeof(BasicBlock));
    if (readData == NULL) return 1;
    this->basicBlock = (BasicBlock *)readData;
    this->instructionIndex = 0;
    /* necessary to adjust offset */
    this->ReadData(sizeof(Instruction) * this->basicBlock->basicBlockSize);
    return 0;
}

int sinucaTracer::StaticTraceFile::GetInstruction(InstructionInfo *inst) {
    if (this->instructionIndex >= this->basicBlock->basicBlockSize) return 1;
    this->ConvertInstructionFormat(&inst->staticInfo);
    this->instructionIndex++;
    return 0;
}

void sinucaTracer::StaticTraceFile::ConvertInstructionFormat(
    StaticInstructionInfo *inst) {
    Instruction *instInRawFormat;

    instInRawFormat = &this->basicBlock->instructions[this->instructionIndex];
    strncpy(inst->opcodeAssembly, instInRawFormat->name, TRACE_LINE_SIZE - 1);
    inst->numReadRegs = instInRawFormat->numReadRegs;
    inst->numWriteRegs = instInRawFormat->numWriteRegs;
    /* copy registers used */
    memcpy(inst->readRegs, instInRawFormat->readRegs,
           sizeof(instInRawFormat->readRegs));
    memcpy(inst->writeRegs, instInRawFormat->writeRegs,
           sizeof(instInRawFormat->writeRegs));
    inst->opcodeSize = instInRawFormat->size;
    inst->baseReg = instInRawFormat->baseReg;
    inst->indexReg = instInRawFormat->indexReg;
    inst->opcodeAddress = instInRawFormat->addr;
    /* single bit fields */
    inst->isNonStdMemOp =
        static_cast<bool>(instInRawFormat->isNonStandardMemOp);
    inst->isControlFlow = static_cast<bool>(instInRawFormat->isControlFlow);
    inst->isPredicated = static_cast<bool>(instInRawFormat->isPredicated);
    inst->isPrefetch = static_cast<bool>(instInRawFormat->isPrefetch);
    inst->isIndirect =
        static_cast<bool>(instInRawFormat->isIndirectControlFlow);
    /* convert branch type to enum Branch */
    switch (instInRawFormat->branchType) {
        case BRANCH_CALL:
            inst->branchType = BranchCall;
            break;
        case BRANCH_SYSCALL:
            inst->branchType = BranchSyscall;
            break;
        case BRANCH_RETURN:
            inst->branchType = BranchReturn;
            break;
        case BRANCH_COND:
            inst->branchType = BranchCond;
            break;
        case BRANCH_UNCOND:
            inst->branchType = BranchUncond;
            break;
    }
}