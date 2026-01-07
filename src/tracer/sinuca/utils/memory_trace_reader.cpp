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

#include "tracer/sinuca/file_handler.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

int MemoryTraceReader::OpenFile(const char* sourceDir, const char* imageName,
                                int tid) {
    unsigned long bufferSize;
    char* path;

    bufferSize = GetPathTidInSize(sourceDir, "memory", imageName);
    path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imageName, tid, bufferSize);
    this->file = fopen(path, "rb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to open memory trace file!\n");
        return 1;
    }
    if (this->header.LoadHeader(this->file)) {
        SINUCA3_ERROR_PRINTF("Failed to read memory trace header!\n");
        return 1;
    }

    return 0;
}

int MemoryTraceReader::ReadMemoryOperations(InstructionPacket* inst) {
    if (this->reachedEnd) {
        SINUCA3_ERROR_PRINTF(
            "[ReadMemoryOperations] already reached end in mem trace file!\n");
        return 1;
    }

    if (this->recordArrayIndex == this->numberOfRecordsRead) {
        if (this->LoadRecordArray()) {
            this->reachedEnd = true;
            return 1;
        }
    }

    if (this->recordArray[this->recordArrayIndex].recordType !=
        MemoryRecordHeader) {
        SINUCA3_ERROR_PRINTF(
            "[ReadMemoryOperations] Expected memory operation header!\n");
        SINUCA3_ERROR_PRINTF(
            "[ReadMemoryOperations] recordType is [%d] and record array index "
            "is [%d]\n",
            this->recordArray[this->recordArrayIndex].recordType,
            this->recordArrayIndex);
        return 1;
    }

    int totalMemOps =
        this->recordArray[this->recordArrayIndex].data.numberOfMemoryOps;

    ++this->recordArrayIndex;

    unsigned char recordType;
    for (int i = 0; i < totalMemOps; ++i, ++this->recordArrayIndex) {
        if (this->recordArrayIndex == this->numberOfRecordsRead) {
            if (this->LoadRecordArray()) {
                SINUCA3_ERROR_PRINTF(
                    "[ReadMemoryOperations] invalid number of mem ops!\n");
                this->reachedEnd = true;
                return 1;
            }
        }

        recordType = this->recordArray[this->recordArrayIndex].recordType;
        if (recordType == MemoryRecordLoad) {
            inst->dynamicInfo.readsAddr[inst->dynamicInfo.numReadings] =
                this->recordArray[this->recordArrayIndex]
                    .data.operation.address;
            inst->dynamicInfo.readsSize[inst->dynamicInfo.numReadings] =
                this->recordArray[this->recordArrayIndex].data.operation.size;
            inst->dynamicInfo.numReadings++;
        } else if (recordType == MemoryRecordStore) {
            inst->dynamicInfo.writesAddr[inst->dynamicInfo.numWritings] =
                this->recordArray[this->recordArrayIndex]
                    .data.operation.address;
            inst->dynamicInfo.writesSize[inst->dynamicInfo.numWritings] =
                this->recordArray[this->recordArrayIndex].data.operation.size;
            inst->dynamicInfo.numWritings++;
        } else {
            SINUCA3_ERROR_PRINTF(
                "[ReadMemoryOperations] unexpected record type!\n");
            return 1;
        }
    }

    return 0;
}

int MemoryTraceReader::LoadRecordArray() {
    this->recordArrayIndex = 0;

    unsigned long readBytes = 0;
    readBytes =
        fread(this->recordArray, 1, sizeof(this->recordArray), this->file);

    this->numberOfRecordsRead = readBytes / sizeof(*this->recordArray);

    return (this->numberOfRecordsRead == 0);
}
