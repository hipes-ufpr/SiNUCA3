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

#include <cstdlib>

#include "tracer/sinuca/file_handler.hpp"

extern "C" {
#include <alloca.h>
}

int sinucaTracer::MemoryTraceWriter::OpenFile(const char* sourceDir,
                                              const char* imgName, int tid) {
    unsigned long bufferSize;

    bufferSize = GetPathTidInSize(sourceDir, "memory", imgName);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imgName, tid, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    /* help maintain compatibility with future versions of the memory trace
     * file. */
    fseek(this->file, sizeof(this->header), SEEK_SET);

    return 0;
}

int sinucaTracer::MemoryTraceWriter::DuplicateRecordArraySize() {
    if (this->recordArraySize == 0) this->recordArraySize = 0x10;
    this->recordArraySize <<= 1;
    this->recordArray =
        (MemoryTraceRecord*)realloc(this->recordArray, this->recordArraySize);
    return (this->recordArray == NULL);
}

int sinucaTracer::MemoryTraceWriter::AddNonStdOpHeader(
    unsigned int memoryOperations) {
    if (this->file == NULL) return 1;
    if (this->recordArrayOccupation >= this->recordArraySize) {
        this->DuplicateRecordArraySize();
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordNonStdHeader;
    this->recordArray[this->recordArrayOccupation]
        .data.nonStdHeader.nonStdMemOps = memoryOperations;

    return 0;
}

int sinucaTracer::MemoryTraceWriter::AddMemoryOperation(unsigned long address,
                                                        unsigned int size,
                                                        unsigned char type) {
    if (this->file == NULL) return 1;
    if (this->recordArrayOccupation >= this->recordArraySize) {
        this->DuplicateRecordArraySize();
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordOperation;
    this->recordArray[this->recordArrayOccupation].data.operation.type = type;
    this->recordArray[this->recordArrayOccupation].data.operation.address =
        address;
    this->recordArray[this->recordArrayOccupation].data.operation.size = size;

    return 0;
}