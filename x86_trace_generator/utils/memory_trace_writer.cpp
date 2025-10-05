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

extern "C" {
#include <alloca.h>
}

namespace sinucaTracer {

int MemoryTraceWriter::OpenFile(const char* sourceDir, const char* imageName,
                                int tid) {
    unsigned long bufferSize;

    bufferSize = GetPathTidInSize(sourceDir, "memory", imageName);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imageName, tid, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    fseek(this->file, sizeof(this->header), SEEK_SET);

    return 0;
}

int MemoryTraceWriter::DoubleRecordArraySize() {
    if (this->recordArraySize == 0) {
        this->recordArraySize = 512;
    }
    this->recordArraySize <<= 1;
    this->recordArray =
        (MemoryTraceRecord*)realloc(this->recordArray, this->recordArraySize);
    return this->recordArray == NULL;
}

int MemoryTraceWriter::AddNumberOfMemOperations(unsigned int memOps) {
    if (this->file == NULL) return 1;
    if (this->recordArrayOccupation >= this->recordArraySize) {
        if (this->DoubleRecordArraySize()) {
            return 1;
        }
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordOperationHeader;
    this->recordArray[this->recordArrayOccupation]
        .data.opHeader.numberOfMemoryOps = memOps;
    ++this->recordArrayOccupation;

    return 0;
}

int MemoryTraceWriter::AddMemoryOperation(unsigned long address,
                                          unsigned int size,
                                          unsigned char type) {
    if (this->file == NULL) return 1;
    if (this->recordArrayOccupation >= this->recordArraySize) {
        if (this->DoubleRecordArraySize()) {
            return 1;
        }
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordOperation;
    this->recordArray[this->recordArrayOccupation].data.operation.type = type;
    this->recordArray[this->recordArrayOccupation].data.operation.address =
        address;
    this->recordArray[this->recordArrayOccupation].data.operation.size = size;
    ++this->recordArrayOccupation;

    return 0;
}

}  // namespace sinucaTracer
