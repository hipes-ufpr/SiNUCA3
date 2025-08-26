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
 * @file memory_trace_writer.cpp
 * @details Implementation of MemoryTraceFile class.
 */

#include "memory_trace_writer.hpp"

#include <cassert>
#include <cstddef>

#include "../../src/utils/logging.hpp"
extern "C" {
#include <alloca.h>
}

sinucaTracer::MemoryTraceFile::MemoryTraceFile(const char* source,
                                               const char* img, THREADID tid) {
    unsigned long bufferSize =
        sinucaTracer::GetPathTidInSize(source, "memory", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, source, "memory", img, tid, bufferSize);

    this->::sinucaTracer::TraceFileWriter::UseFile(path);
}

sinucaTracer::MemoryTraceFile::~MemoryTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last MemoryTraceFile flush\n");
    if (this->tf.offsetInBytes > 0) {
        this->FlushLenBytes(&this->tf.offsetInBytes,
                            sizeof(this->tf.offsetInBytes));
        this->FlushBuffer();
    }
}

void sinucaTracer::MemoryTraceFile::PrepareDataNonStdAccess(
    PIN_MULTI_MEM_ACCESS_INFO* pinNonStdInfo) {
    this->numReadOps = 0;
    this->numWriteOps = 0;

    for (unsigned int it = 0; it < pinNonStdInfo->numberOfMemops; ++it) {
        if (pinNonStdInfo->memop[it].memopType == PIN_MEMOP_LOAD) {
            this->readOps[this->numReadOps].addr =
                pinNonStdInfo->memop[it].memoryAddress;
            this->readOps[this->numReadOps].size =
                pinNonStdInfo->memop[it].bytesAccessed;
            this->numReadOps++;
        } else {
            this->readOps[this->numReadOps].addr =
                pinNonStdInfo->memop[it].memoryAddress;
            this->writeOps[this->numWriteOps].size =
                pinNonStdInfo->memop[it].bytesAccessed;
            this->numWriteOps++;
        }
    }
    // This variable is checked in AppendToBufferLastMemoryAccess
    this->wasLastOperationStd = false;
}

void sinucaTracer::MemoryTraceFile::PrepareDataStdMemAccess(
    unsigned long addr, unsigned int opSize) {
    this->stdAccessOp.addr = addr;
    this->stdAccessOp.size = opSize;
    // This variable is checked in AppendToBufferLastMemoryAccess
    this->wasLastOperationStd = true;
}

void sinucaTracer::MemoryTraceFile::AppendToBufferLastMemoryAccess() {
    if (this->wasLastOperationStd) {
        this->MemoryAppendToBuffer(&this->stdAccessOp,
                                   sizeof(this->stdAccessOp));
    } else {
        // Append number of read operations
        this->MemoryAppendToBuffer(&this->numReadOps, SIZE_NUM_MEM_R_W);
        // Append number of write operations
        this->MemoryAppendToBuffer(&this->numWriteOps, SIZE_NUM_MEM_R_W);
        // Append read operations' buffer
        this->MemoryAppendToBuffer(this->readOps,
                                   this->numReadOps * sizeof(*this->readOps));
        // Append write operations' buffer
        this->MemoryAppendToBuffer(this->writeOps,
                                   this->numWriteOps * sizeof(*this->writeOps));
    }
}

void sinucaTracer::MemoryTraceFile::MemoryAppendToBuffer(void* ptr,
                                                         size_t len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushLenBytes(&this->tf.offsetInBytes, sizeof(unsigned long));
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}
