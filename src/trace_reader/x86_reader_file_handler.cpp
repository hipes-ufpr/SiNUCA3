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

#include "x86_reader_file_handler.hpp"

#include <cstring>

extern "C" {
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

#include "../utils/logging.hpp"

inline void printFileErrorLog(const char *path) {
    SINUCA3_ERROR_PRINTF("Could not open [%s]: %s\n", path, strerror(errno));
}

sinuca::traceReader::StaticTraceFile::StaticTraceFile(const char *folderPath,
                                                      const char *img) {
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

void sinuca::traceReader::StaticTraceFile::ReadNextPackage(
    InstructionInfo *info) {
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

unsigned int sinuca::traceReader::StaticTraceFile::GetNewBBlSize() {
    unsigned int *numIns;
    numIns = (unsigned int *)(this->GetData(SIZE_NUM_BBL_INS));
    return *numIns;
}

bool sinuca::traceReader::StaticTraceFile::Valid() { return this->isValid; }

sinuca::traceReader::StaticTraceFile::~StaticTraceFile() {
    munmap(this->mmapPtr, this->mmapSize);
    close(this->fd);
}

sinuca::traceReader::DynamicTraceFile::DynamicTraceFile(const char *folderPath,
                                                        const char *img,
                                                        THREADID tid) {
    unsigned long bufferSize = GetPathTidInSize(folderPath, "dynamic", img);
    char *path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, folderPath, "dynamic", img, tid, bufferSize);

    if (this->::TraceFileReader::UseFile(path) == NULL) {
        this->isValid = false;
        return;
    }

    this->bufActiveSize =
        (unsigned int)(BUFFER_SIZE / sizeof(BBLID)) * sizeof(BBLID);
    this->RetrieveBuffer();  // First buffer read
    this->isValid = true;
}

int sinuca::traceReader::DynamicTraceFile::ReadNextBBl(BBLID *bbl) {
    if (this->eofFound && this->tf.offset == this->eofLocation) {
        return 1;
    }
    if (this->tf.offset >= this->bufActiveSize) {
        this->RetrieveBuffer();
    }
    *bbl = *(BBLID *)(this->GetData(sizeof(BBLID)));

    return 0;
}

bool sinuca::traceReader::DynamicTraceFile::Valid() { return this->isValid; }

sinuca::traceReader::MemoryTraceFile::MemoryTraceFile(const char *folderPath,
                                                      const char *img,
                                                      THREADID tid) {
    unsigned long bufferSize = GetPathTidInSize(folderPath, "memory", img);
    char *path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, folderPath, "memory", img, tid, bufferSize);

    if (this->::TraceFileReader::UseFile(path) == NULL) {
        this->isValid = false;
        return;
    }

    this->RetrieveLenBytes((void *)&this->bufActiveSize,
                           sizeof(this->tf.offset));
    this->RetrieveBuffer();
    this->isValid = true;
}

void sinuca::traceReader::MemoryTraceFile::MemRetrieveBuffer() {
    this->RetrieveLenBytes(&this->bufActiveSize, sizeof(unsigned long));
    this->RetrieveBuffer();
}

unsigned short sinuca::traceReader::MemoryTraceFile::GetNumOps() {
    unsigned short numOps;

    numOps = *(unsigned short *)this->GetData(SIZE_NUM_MEM_R_W);
    if (this->tf.offset >= this->bufActiveSize) {
        this->MemRetrieveBuffer();
    }
    return numOps;
}

DataMEM *sinuca::traceReader::MemoryTraceFile::GetDataMemArr(
    unsigned short len) {
    DataMEM *arrPtr;

    arrPtr = (DataMEM *)(this->GetData(len * sizeof(DataMEM)));
    if (this->tf.offset >= this->bufActiveSize) {
        this->MemRetrieveBuffer();
    }
    return arrPtr;
}

int sinuca::traceReader::MemoryTraceFile::ReadNextMemAccess(
    InstructionInfo *insInfo, DynamicInstructionInfo *dynInfo) {
    DataMEM *writeOps;
    DataMEM *readOps;

    /*
     * In case the instruction performs non standard memory operations
     * with variable number of operands, the number of readings/writings
     * is written directly to the memory trace file
     *
     * Otherwise, it was written in the static trace file.
     */
    if (insInfo->staticInfo.isNonStdMemOp) {
        dynInfo->numReadings = this->GetNumOps();
        dynInfo->numWritings = this->GetNumOps();
    } else {
        dynInfo->numReadings = insInfo->staticNumReadings;
        dynInfo->numWritings = insInfo->staticNumWritings;
    }

    readOps = this->GetDataMemArr(dynInfo->numReadings);
    writeOps = this->GetDataMemArr(dynInfo->numWritings);
    for (unsigned short it = 0; it < dynInfo->numReadings; it++) {
        dynInfo->readsAddr[it] = readOps[it].addr;
        dynInfo->readsSize[it] = readOps[it].size;
    }
    for (unsigned short it = 0; it < dynInfo->numWritings; it++) {
        dynInfo->writesAddr[it] = writeOps[it].addr;
        dynInfo->writesSize[it] = writeOps[it].size;
    }

    return 0;
}

bool sinuca::traceReader::MemoryTraceFile::Valid() { return this->isValid; }

void *sinuca::traceReader::StaticTraceFile::GetData(unsigned long len) {
    void *ptr = (void *)(this->mmapPtr + this->mmapOffset);
    this->mmapOffset += len;
    return ptr;
}

void sinuca::traceReader::StaticTraceFile::GetFlagValues(InstructionInfo *info,
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

void sinuca::traceReader::StaticTraceFile::GetBranchFields(
    sinuca::StaticInstructionInfo *info, struct DataINS *data) {
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

void sinuca::traceReader::StaticTraceFile::GetRegs(
    sinuca::StaticInstructionInfo *info, struct DataINS *data) {
    info->numReadRegs = data->numReadRegs;
    memcpy(info->readRegs, data->readRegs,
           data->numReadRegs * sizeof(*data->readRegs));

    info->numWriteRegs = data->numWriteRegs;
    memcpy(info->writeRegs, data->writeRegs,
           data->numWriteRegs * sizeof(*data->writeRegs));
}
