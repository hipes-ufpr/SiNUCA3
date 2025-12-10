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

#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

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

int MemoryTraceWriter::FlushRecordArray() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("File pointer is nil in mem trace!\n");
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

int MemoryTraceWriter::CheckRecordArray() {
    if (this->IsRecordArrayFull()) {
        if (this->FlushRecordArray()) {
            SINUCA3_ERROR_PRINTF("Failed to flush mem record array!\n")
            return 1;
        }
        this->ResetRecordArray();
    }
    return 0;
}

int MemoryTraceWriter::AddMemoryRecord(MemoryTraceRecord record) {
    this->recordArray[this->recordArrayOccupation] = record;
    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;
    return 0;
}

int MemoryTraceWriter::AddNumberOfMemOperations(unsigned int numMemOps) {
    MemoryTraceRecord record;
    record.recordType = MemoryRecordHeader;
    record.data.numberOfMemoryOps = numMemOps;
    return (this->AddMemoryRecord(record));
}

int MemoryTraceWriter::AddMemOp(unsigned long address, unsigned int size,
                                bool isLoadOp) {
    MemoryTraceRecord record;
    record.recordType = (isLoadOp) ? MemoryRecordLoad : MemoryRecordStore;
    record.data.operation.address = address;
    record.data.operation.size = size;
    return (this->AddMemoryRecord(record));
}
