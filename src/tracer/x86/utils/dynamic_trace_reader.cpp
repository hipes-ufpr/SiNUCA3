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

#include <cstring>

extern "C" {
#include <alloca.h>
#include <errno.h>
}

inline void printFileErrorLog(const char *path) {
    SINUCA3_ERROR_PRINTF("Could not open [%s]: %s\n", path, strerror(errno));
}

tracer::DynamicTraceFile::DynamicTraceFile(const char *folderPath, const char *img,
                                   THREADID tid) {
    unsigned long bufferSize = GetPathTidInSize(folderPath, "dynamic", img);
    char *path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, folderPath, "dynamic", img, tid, bufferSize);

    if (this->UseFile(path) == NULL) {
        this->isValid = false;
        return;
    }

    this->bufActiveSize =
        (unsigned int)(BUFFER_SIZE / sizeof(BBLID)) * sizeof(BBLID);
    this->RetrieveBuffer();  // First buffer read
    this->isValid = true;
}

int tracer::DynamicTraceFile::ReadNextBBl(BBLID *bbl) {
    if (this->eofFound && this->tf.offsetInBytes == this->eofLocation) {
        return 1;
    }
    if (this->tf.offsetInBytes >= this->bufActiveSize) {
        this->RetrieveBuffer();
    }
    *bbl = *(BBLID *)(this->GetData(sizeof(BBLID)));

    return 0;
}