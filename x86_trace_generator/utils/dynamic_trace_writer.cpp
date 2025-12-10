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
 * @file dynamic_trace_writer.cpp
 * @details Implementation of DynamicTraceFile class.
 */

#include "dynamic_trace_writer.hpp"

#include <cstdlib>

#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

int DynamicTraceWriter::OpenFile(const char* sourceDir, const char* imageName,
                                 int tid) {
    unsigned long bufferSize;
    char* path;

    bufferSize = GetPathTidInSize(sourceDir, "dynamic", imageName);
    path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imageName, tid, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc file in dynamic trace!");
        return 1;
    }
    this->header.ReserveHeaderSpace(this->file);

    return 0;
}

int DynamicTraceWriter::FlushRecordArray() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("File pointer is nil in mem trace!\n");
        return 1;
    }

    unsigned long occupationInBytes =
        this->recordArrayOccupation * sizeof(*this->recordArray);

    if (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
        occupationInBytes) {
        SINUCA3_ERROR_PRINTF("Failed to flush memory records!\n");
        return 1;
    }

    return 0;
}

int DynamicTraceWriter::CheckRecordArray() {
    if (this->IsRecordArrayFull()) {
        if (this->FlushRecordArray()) {
            SINUCA3_ERROR_PRINTF("Failed to flush mem record array!\n")
            return 1;
        }
        this->ResetRecordArray();
    }
    return 0;
}

int DynamicTraceWriter::AddDynamicRecord(DynamicTraceRecord record) {
    this->recordArray[this->recordArrayOccupation] = record;
    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;
    return 0;
}

int DynamicTraceWriter::AddThreadCreateEvent() {
    DynamicTraceRecord record;
    record.recordType = DynamicRecordCreateThread;
    return (this->AddDynamicRecord(record));
}

int DynamicTraceWriter::AddThreadDestructionEvent() {
    DynamicTraceRecord record;
    record.recordType = DynamicRecordDestroyThread;
    return (this->AddDynamicRecord(record));
}

int DynamicTraceWriter::AddMutexEvent(bool isLockReq, bool isGlobalMutex, unsigned long mutexAddr) {
    DynamicTraceRecord record;
    if (isLockReq) {
        record.recordType = DynamicRecordLockRequest;
    } else {
        record.recordType = DynamicRecordUnlockRequest;
    }
    record.data.lockInfo.mutexAddress = mutexAddr;
    record.data.lockInfo.isGlobalMutex = (isGlobalMutex) ? 1 : 0;
    return (this->AddDynamicRecord(record));
}

int DynamicTraceWriter::AddBarrierEvent() {
    DynamicTraceRecord record;
    record.recordType = DynamicRecordBarrier;
    return (this->AddDynamicRecord(record));
}

int DynamicTraceWriter::AddThreadAbruptEndEvent() {
    DynamicTraceRecord record;
    record.recordType = DynamicRecordAbruptEnd;
    return (this->AddDynamicRecord(record));
}

int DynamicTraceWriter::AddBasicBlockId(int identifier) {
    DynamicTraceRecord record;
    record.recordType = DynamicRecordBasicBlockIdentifier;
    record.data.basicBlockIdentifier = identifier;
    return (this->AddDynamicRecord(record));
}
