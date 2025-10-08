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

#include "dynamic_trace_reader.hpp"

#include <cstdio>
#include <cstdlib>

#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
}

namespace sinucaTracer {

int DynamicTraceReader::OpenFile(const char *sourceDir, const char *imageName,
                                 int tid) {
    unsigned long bufferSize;
    char *path;

    bufferSize = GetPathTidInSize(sourceDir, "dynamic", imageName);
    path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imageName, tid, bufferSize);
    this->file = fopen(path, "rb");
    if (this->file == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to open dynamic trace file!\n");
        return 1;
    }
    if (this->header.LoadHeader(this->file)) {
        SINUCA3_ERROR_PRINTF("Failed to read dynamic trace header!\n");
        return 1;
    }

    return 0;
}

int DynamicTraceReader::ReadDynamicRecord() {
    if (this->ReachedDynamicTraceEnd()) {
        SINUCA3_ERROR_PRINTF("Already reached trace end!\n");
        return 1;
    }
    if (this->recordArrayIndex == this->recordArraySize) {
        if (this->LoadRecordArray()) {
            SINUCA3_ERROR_PRINTF("Failed to read new dynamic record array!\n");
            return 1;
        }
    }

    ++this->recordArrayIndex;

    return 0;
}

int DynamicTraceReader::LoadRecordArray() {
    if (this->file == NULL) {
        return 1;
    }

    this->recordArrayIndex = -1;
    this->recordArraySize =
        fread(this->recordArray, 1, sizeof(this->recordArray), this->file) /
        sizeof(*this->recordArray);

    int totalRecords =
        sizeof(this->recordArray) / sizeof(*this->recordArray);

    if (this->recordArraySize != totalRecords) {
        if (this->reachedEnd) {
            SINUCA3_ERROR_PRINTF("Reached dynamic file end twice!\n");
            return 1;
        }
        this->reachedEnd = true;
    }

    return 0;
}

}  // namespace sinucaTracer
