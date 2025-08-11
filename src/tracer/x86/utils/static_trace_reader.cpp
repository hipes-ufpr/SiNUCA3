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

extern "C" {
#include <alloca.h>
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

tracer::StaticTraceFile::StaticTraceFile(const char *folderPath, const char *img) {
    unsigned long bufferLen = GetPathTidOutSize(folderPath, "static", img);
    char *staticPath = (char *)alloca(bufferLen);
    FormatPathTidOut(staticPath, folderPath, "static", img, bufferLen);

    this->fd = open(staticPath, O_RDONLY);
    if (fd == -1) {
        printFileErrorLog(staticPath);
        this->isValid = false;
        return;
    }
    this->mmapSize = lseek(fd, 0, SEEK_END);
    SINUCA3_DEBUG_PRINTF("Mmap Size [%lu]\n", this->mmapSize);
    this->mmapPtr =
        (char *)mmap(0, this->mmapSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (this->mmapPtr == MAP_FAILED) {
        printFileErrorLog(staticPath);
        this->isValid = false;
        return;
    }

    this->mmapOffset = 3 * sizeof(unsigned int);

    // Get number of threads
    this->numThreads = *(unsigned int *)(this->mmapPtr + 0);
    SINUCA3_DEBUG_PRINTF("Number of Threads [%u]\n", this->numThreads);

    // Get total number of Basic Blocks
    this->totalBBLs =
        *(unsigned int *)(this->mmapPtr + sizeof(this->numThreads));
    SINUCA3_DEBUG_PRINTF("Number of BBLs [%u]\n", this->totalBBLs);
    // Get total number of instructions
    this->totalIns =
        *(unsigned int *)(this->mmapPtr + sizeof(this->numThreads) +
                          sizeof(this->totalBBLs));
    SINUCA3_DEBUG_PRINTF("Number of Instructions [%u]\n", this->totalIns);
    this->isValid = true;
}

void tracer::StaticTraceFile::ReadNextPackage(InstructionInfo *info) {
    DataINS *data = (DataINS *)(this->GetData(sizeof(DataINS)));

    unsigned long len = strlen(data->name);
    memcpy(info->staticInfo.opcodeAssembly, data->name, len + 1);

    info->staticInfo.opcodeSize = data->size;
    info->staticInfo.baseReg = data->baseReg;
    info->staticInfo.indexReg = data->indexReg;
    info->staticInfo.opcodeAddress = data->addr;

    this->GetFlagValues(info, data);
    this->GetBranchFields(&info->staticInfo, data);
    this->GetRegs(&info->staticInfo, data);
}

unsigned int tracer::StaticTraceFile::GetNewBBlSize() {
    unsigned int *numIns;
    numIns = (unsigned int *)(this->GetData(SIZE_NUM_BBL_INS));
    return *numIns;
}

void *tracer::StaticTraceFile::GetData(unsigned long len) {
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

void tracer::StaticTraceFile::GetFlagValues(InstructionInfo *info,
                                    struct DataINS *data) {
    info->staticInfo.isPredicated = static_cast<bool>(data->isPredicated);
    info->staticInfo.isPrefetch = static_cast<bool>(data->isPrefetch);
    info->staticInfo.isNonStdMemOp =
        static_cast<bool>(data->isNonStandardMemOp);
    if (!info->staticInfo.isNonStdMemOp) {
        info->staticNumReadings = data->isRead + data->isRead2;
        info->staticNumWritings = data->isWrite;
    }
}

void tracer::StaticTraceFile::GetBranchFields(StaticInstructionInfo *info,
                                      struct DataINS *data) {
    info->isIndirect = static_cast<bool>(data->isIndirectControlFlow);
    info->isControlFlow = static_cast<bool>(data->isControlFlow);
    switch (data->branchType) {
        case BRANCH_CALL:
            info->branchType = BranchCall;
            break;
        case BRANCH_SYSCALL:
            info->branchType = BranchSyscall;
            break;
        case BRANCH_RETURN:
            info->branchType = BranchReturn;
            break;
        case BRANCH_COND:
            info->branchType = BranchCond;
            break;
        case BRANCH_UNCOND:
            info->branchType = BranchUncond;
            break;
    }
}

void tracer::StaticTraceFile::GetRegs(StaticInstructionInfo *info,
                              struct DataINS *data) {
    info->numReadRegs = data->numReadRegs;
    memcpy(info->readRegs, data->readRegs,
           data->numReadRegs * sizeof(*data->readRegs));

    info->numWriteRegs = data->numWriteRegs;
    memcpy(info->writeRegs, data->writeRegs,
           data->numWriteRegs * sizeof(*data->writeRegs));
}

tracer::StaticTraceFile::~StaticTraceFile() {
    munmap(this->mmapPtr, this->mmapSize);
    close(this->fd);
}
