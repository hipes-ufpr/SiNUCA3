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
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

namespace sinucaTracer {

int DynamicTraceWriter::OpenFile(const char *sourceDir, const char *imageName,
                                 int tid) {
    unsigned long bufferSize;
    char *path;

    bufferSize = GetPathTidInSize(sourceDir, "dynamic", imageName);
    path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imageName, tid, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc file in dynamic trace!");
        return 1;
    }
    this->header.ReserveHeaderSpace(this->file);

    return 0;
}

int DynamicTraceWriter::AddThreadEvent(unsigned char event, int tid) {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[1] File pointer is nil in mem trace!\n");
        return 1;
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        DynamicRecordThreadEvent;
    this->recordArray[this->recordArrayOccupation].data.thr.event = event;
    this->recordArray[this->recordArrayOccupation].data.thr.threadId = tid;

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

int DynamicTraceWriter::AddBasicBlockId(int id) {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[2] File pointer is nil in mem trace!\n");
        return 1;
    }

    this->recordArray[this->recordArrayOccupation].recordType =
        DynamicRecordBasicBlockIdentifier;
    this->recordArray[this->recordArrayOccupation]
        .data.bbl.basicBlockIdentifier = id;

    ++this->recordArrayOccupation;

    if (this->IsRecordArrayFull()) {
        if (this->FlushRecordArray()) {
            SINUCA3_ERROR_PRINTF("[2] Failed to flush mem record array!\n")
            return 1;
        }
        this->ResetRecordArray();
    }

    return 0;
}

int DynamicTraceWriter::FlushRecordArray() {
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("[3] File pointer is nil in mem trace!\n");
        return 1;
    }

    unsigned long occupationInBytes =
        this->recordArrayOccupation * sizeof(*this->recordArray);

    if (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
        occupationInBytes) {
        SINUCA3_ERROR_PRINTF("[3] Failed to flush memory records!\n");
        return 1;
    }

    return 0;
}

}  // namespace sinucaTracer
