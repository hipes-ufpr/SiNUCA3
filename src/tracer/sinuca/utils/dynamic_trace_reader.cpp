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
    path = (char *)malloc(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imageName, tid, bufferSize);
    this->file = fopen(path, "rb");
    if (this->file == NULL) return 1;

    unsigned long bytesRead;
    bytesRead = fread(&this->header, 1, sizeof(this->header), this->file);

    return (bytesRead != sizeof(this->header));
}

int sinucaTracer::DynamicTraceReader::ReadDynamicRecord() {
    if (this->file == NULL) {
        return 1;
    }

    unsigned long bytesRead;
    bytesRead = fread(&record, 1, sizeof(record), this->file);
    if (bytesRead == 0) {
        this->reachedEnd = true;
    } else if (bytesRead != sizeof(record)) {
        SINUCA3_ERROR_PRINTF("Failed to read record properly\n");
        return 1;
    }

    return 0;
}

}  // namespace sinucaTracer
