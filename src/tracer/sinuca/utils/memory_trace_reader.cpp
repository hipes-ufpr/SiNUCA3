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

extern "C" {
#include <alloca.h>
}

namespace sinucaTracer {

int MemoryTraceReader::OpenFile(const char* sourceDir, const char* imageName,
                                int tid) {
    unsigned long bufferSize;
    char* path;

    bufferSize = GetPathTidInSize(sourceDir, "memory", imageName);
    path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imageName, tid, bufferSize);
    this->file = fopen(path, "rb");
    if (this->file == NULL) return 1;

    unsigned long bytesRead;
    bytesRead = fread(&this->header, 1, sizeof(this->header), this->file);

    return (bytesRead != sizeof(this->header));
}

int MemoryTraceReader::ReadMemoryOperations() {
    if (this->file == NULL) return 1;

    unsigned long readBytes;
    readBytes = fread(&this->record, 1, sizeof(this->record), this->file);
    if (readBytes != sizeof(this->record)) {
        SINUCA3_ERROR_PRINTF("Failed to read record from memory file\n");
        return 1;
    }
    if (this->record.recordType != MemoryRecordOperationHeader) {
        SINUCA3_ERROR_PRINTF("Expected mem operation header\n");
        return 1;
    }

    unsigned long address;
    unsigned int size;

    int totalMemOps = this->record.data.opHeader.numberOfMemoryOps;
    for (int i = 0; i < totalMemOps; ++i) {
        readBytes = fread(&this->record, 1, sizeof(this->record), this->file);
        if (readBytes != sizeof(this->record)) {
            SINUCA3_ERROR_PRINTF("Failed to read record from memory file\n");
            return 1;
        }
        if (this->record.recordType != MemoryRecordOperation) {
            SINUCA3_ERROR_PRINTF("Expected memory operation\n");
            return 1;
        }

        address = this->record.data.operation.address;
        size = this->record.data.operation.size;

        if (this->record.data.operation.type == MemoryOperationLoad) {
            ++this->numberOfMemLoadOps;
            this->loadOpsAddressVec.push_back(address);
            this->loadOpsSizeVec.push_back(size);
        } else {
            ++this->numberOfMemStoreOps;
            this->storeOpsAddressVec.push_back(address);
            this->storeOpsSizeVec.push_back(size);
        }
    }

    return 0;
}

int MemoryTraceReader::CopyLoadOpsAddresses(unsigned long* array,
                                            unsigned long arraySize) {
    if (this->loadOpsAddressVec.size() > arraySize) {
        SINUCA3_ERROR_PRINTF("Too many load ops addresses\n");
        return 1;
    }

    for (unsigned long i = 0; i < this->loadOpsAddressVec.size(); ++i) {
        array[i] = this->loadOpsAddressVec[i];
    }

    this->loadOpsAddressVec.clear();
    return 0;
}

int MemoryTraceReader::CopyLoadOpsSizes(unsigned int* array,
                                        unsigned long arraySize) {
    if (this->loadOpsSizeVec.size() > arraySize) {
        SINUCA3_ERROR_PRINTF("Too many load ops sizes\n");
        return 1;
    }

    for (unsigned long i = 0; i < this->loadOpsSizeVec.size(); ++i) {
        array[i] = this->loadOpsSizeVec[i];
    }

    this->loadOpsSizeVec.clear();
    return 0;
}

int MemoryTraceReader::CopyStoreOpsAddresses(unsigned long* array,
                                             unsigned long arraySize) {
    if (this->storeOpsAddressVec.size() > arraySize) {
        SINUCA3_ERROR_PRINTF("Too many store ops addresses\n");
        return 1;
    }

    for (unsigned long i = 0; i < this->storeOpsAddressVec.size(); ++i) {
        array[i] = this->storeOpsAddressVec[i];
    }

    this->storeOpsAddressVec.clear();
    return 0;
}

int MemoryTraceReader::CopyStoreOpsSizes(unsigned int* array,
                                         unsigned long arraySize) {
    if (this->storeOpsSizeVec.size() > arraySize) {
        SINUCA3_ERROR_PRINTF("Too many store ops sizes\n");
        return 1;
    }

    for (unsigned long i = 0; i < this->storeOpsSizeVec.size(); ++i) {
        array[i] = this->storeOpsSizeVec[i];
    }

    this->storeOpsSizeVec.clear();
    return 0;
}

}  // namespace sinucaTracer
