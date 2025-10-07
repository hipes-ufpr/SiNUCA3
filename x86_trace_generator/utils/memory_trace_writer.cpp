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

#include "utils/logging.hpp"

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
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc this->file\n");
        return 1;
    }
    this->header.ReserveHeaderSpace(this->file);

    return 0;
}

int MemoryTraceWriter::AddNumberOfMemOperations(unsigned int memOps) {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[1] File pointer is nil in mem trace!\n");
        return 1;
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordOperationHeader;
    this->recordArray[this->recordArrayOccupation]
        .data.opHeader.numberOfMemoryOps = memOps;

    ++this->recordArrayOccupation;

    if (this->IsRecordArrayFull()) {
        if (this->FlushRecordArray()) {
            SINUCA3_ERROR_PRINTF("[1] Failed to flush mem record array!\n")
            return 1;
        }
        this->ResetRecordArray();
    }

    return 0;
}

int MemoryTraceWriter::AddMemoryOperation(unsigned long address,
                                          unsigned int size,
                                          unsigned char type) {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[2] File pointer is nil in mem trace!\n");
        return 1;
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        MemoryRecordOperation;
    this->recordArray[this->recordArrayOccupation].data.operation.type = type;
    this->recordArray[this->recordArrayOccupation].data.operation.address =
        address;
    this->recordArray[this->recordArrayOccupation].data.operation.size = size;

    ++this->recordArrayOccupation;

    if (this->IsRecordArrayFull()) {
        if (this->FlushRecordArray()) {
            SINUCA3_ERROR_PRINTF("[1] Failed to flush mem record array!\n")
            return 1;
        }
        this->ResetRecordArray();
    }

    return 0;
}

int MemoryTraceWriter::FlushRecordArray() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[3] File pointer is nil in mem trace!\n");
        return 1;
    }

    unsigned long occupationInBytes =
        this->recordArrayOccupation * sizeof(*this->recordArray);

    if (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
        occupationInBytes) {
        SINUCA3_ERROR_PRINTF("[1] Failed to flush memory records!\n");
        return 1;
    }

    return 0;
}

}  // namespace sinucaTracer
