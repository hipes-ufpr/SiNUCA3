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

#include <cstdio>

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

int MemoryTraceReader::ReadMemoryOperations() {
    if (this->ReachedMemoryTraceEnd()) {
        SINUCA3_ERROR_PRINTF("Already reached trace end!\n");
        return 1;
    }
    if (this->recordArrayIndex == this->recordArraySize) {
        if (this->LoadRecordArray()) {
            SINUCA3_ERROR_PRINTF("[1] Failed to read mem record array!\n");
            return 1;
        }
    }

    ++this->recordArrayIndex;

    if (this->recordArray[this->recordArrayIndex].recordType !=
        MemoryRecordOperationHeader) {
        SINUCA3_ERROR_PRINTF("Expected memory operation header!\n");
        return 1;
    }

    int totalMemOps = this->recordArray[this->recordArrayIndex]
                          .data.opHeader.numberOfMemoryOps;

    for (int i = 0; i < totalMemOps; ++i) {
        ++this->recordArrayIndex;

        if (this->recordArrayIndex == this->recordArraySize) {
            if (this->LoadRecordArray()) {
                SINUCA3_ERROR_PRINTF("[2] Failed to read mem record array!\n");
                return 1;
            }
        }
        if (this->recordArray[this->recordArrayIndex].recordType !=
            MemoryRecordOperation) {
            SINUCA3_ERROR_PRINTF("Expected memory operation!\n");
            return 1;
        }

        unsigned char type =
            this->recordArray[this->recordArrayIndex].data.operation.type;

        if (type == MemoryOperationLoad) {
            this->loadOperations.Enqueue(
                &this->recordArray[this->recordArrayIndex]);
        } else if (type == MemoryOperationStore) {
            this->storeOperations.Enqueue(
                &this->recordArray[this->recordArrayIndex]);
        } else {
            SINUCA3_ERROR_PRINTF("Unkown memory operation! [%d]\n", type);
            return 1;
        }
    }

    this->totalLoadOps = this->loadOperations.GetOccupation();
    this->totalStoreOps = this->storeOperations.GetOccupation();

    return 0;
}

int MemoryTraceReader::CopyOperations(unsigned long* addrsArray,
                                      int addrsArraySize,
                                      unsigned int* sizesArray,
                                      int sizesArraySize, bool isOpLoad) {
    if (sizesArraySize != addrsArraySize) {
        SINUCA3_ERROR_PRINTF("Expected array sizes to be equal!\n");
        return 1;
    }
    if (this->totalLoadOps > addrsArraySize) {
        SINUCA3_ERROR_PRINTF("Array size does not fit mem ops!\n");
        return 1;
    }

    MemoryTraceRecord record;
    CircularBuffer* fifo;
    int iterator = 0;

    if (isOpLoad) {
        fifo = &this->loadOperations;
    } else {
        fifo = &this->storeOperations;
    }

    while (fifo->Dequeue(&record) == 0) {
        addrsArray[iterator] = record.data.operation.address;
        sizesArray[iterator] = record.data.operation.size;
    }

    return 0;
}

int MemoryTraceReader::CopyLoadOperations(unsigned long* addrsArray,
                                          int addrsArraySize,
                                          unsigned int* sizesArray,
                                          int sizesArraySize) {
    if (this->CopyOperations(addrsArray, addrsArraySize, sizesArray,
                             sizesArraySize, true)) {
        SINUCA3_ERROR_PRINTF("Failed to copy load operations!\n");
        return 1;
    }
    return 0;
}

int MemoryTraceReader::CopyStoreOperations(unsigned long* addrsArray,
                                           int addrsArraySize,
                                           unsigned int* sizesArray,
                                           int sizesArraySize) {
    if (this->CopyOperations(addrsArray, addrsArraySize, sizesArray,
                             sizesArraySize, false)) {
        SINUCA3_ERROR_PRINTF("Failed to copy store operations!\n");
        return 1;
    }
    return 0;
}

int MemoryTraceReader::LoadRecordArray() {
    if (this->file == NULL) {
        return 1;
    }

    this->recordArrayIndex = -1;
    this->recordArraySize =
        fread(this->recordArray, 1, sizeof(this->recordArray), this->file) /
        sizeof(*this->recordArray);

    int totalRecords = sizeof(this->recordArray) / sizeof(*this->recordArray);

    if (this->recordArraySize != totalRecords) {
        if (this->reachedEnd) {
            SINUCA3_ERROR_PRINTF("Reached memory file end twice!\n");
            return 1;
        }
        this->reachedEnd = true;
    }

    return 0;
}
