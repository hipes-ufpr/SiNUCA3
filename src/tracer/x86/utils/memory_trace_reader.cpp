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

#include "memory_trace_reader.hpp"

#include <cstring>

extern "C" {
#include <alloca.h>
}

tracer::MemoryTraceFile::MemoryTraceFile(const char *folderPath,
                                         const char *img, THREADID tid) {
    unsigned long bufferSize = GetPathTidInSize(folderPath, "memory", img);
    char *path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, folderPath, "memory", img, tid, bufferSize);

    if (this->UseFile(path) == NULL) {
        this->isValid = false;
        return;
    }

    this->RetrieveLenBytes((void *)&this->bufActiveSize, sizeof(unsigned long));
    this->RetrieveBuffer();
    this->isValid = true;
}

void tracer::MemoryTraceFile::MemRetrieveBuffer() {
    this->RetrieveLenBytes(&this->bufActiveSize, sizeof(unsigned long));
    this->RetrieveBuffer();
}

unsigned short tracer::MemoryTraceFile::GetNumOps() {
    unsigned short numOps;

    numOps = *(unsigned short *)this->GetData(SIZE_NUM_MEM_R_W);
    if (this->tf.offsetInBytes >= this->bufActiveSize) {
        this->MemRetrieveBuffer();
    }
    return numOps;
}

tracer::DataMEM *tracer::MemoryTraceFile::GetDataMemArr(unsigned short len) {
    DataMEM *arrPtr;

    arrPtr = (DataMEM *)(this->GetData(len * sizeof(DataMEM)));
    if (this->tf.offsetInBytes >= this->bufActiveSize) {
        this->MemRetrieveBuffer();
    }
    return arrPtr;
}

void tracer::MemoryTraceFile::ReadNextMemAccess(
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
}
