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

int DynamicTraceWriter::AddThreadHaltEvent() {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventHaltThread;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddThreadCreateEvent(int tid) {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventCreateThread;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.thrCreate.tid = tid;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddThreadDestroyEvent() {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventDestroyThread;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddLockEventPrivateLock(unsigned long lockAddress,
                                                bool isNested, bool isTest) {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventLockRequest;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isDefaultLock = 0;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isNestedLock =
        (isNested == true) ? 1 : 0;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isTestLock = (isTest == true) ? 1 : 0;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.lockAddress = lockAddress;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}
int DynamicTraceWriter::AddUnlockEventPrivateLock(unsigned long lockAddress,
                                                  bool isNested) {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventUnlockRequest;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isDefaultLock = 0;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isNestedLock =
        (isNested == true) ? 1 : 0;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.lockAddress = lockAddress;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddLockEventGlobalLock() {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventLockRequest;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isDefaultLock = 1;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddUnlockEventGlobalLock() {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventUnlockRequest;
    this->recordArray[this->recordArrayOccupation]
        .data.thrEvent.eventData.lockInfo.isDefaultLock = 1;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddBarrierEvent() {
    this->SetRecordTypeThreadEvent();

    this->recordArray[this->recordArrayOccupation].data.thrEvent.eventType =
        ThreadEventBarrier;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

    return 0;
}

int DynamicTraceWriter::AddBasicBlockId(int id) {
    this->SetRecordTypeBasicBlockId();

    this->recordArray[this->recordArrayOccupation]
        .data.bbl.basicBlockIdentifier = id;

    ++this->recordArrayOccupation;
    if (this->CheckRecordArray()) return 1;

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
