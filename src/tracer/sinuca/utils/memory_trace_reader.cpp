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

#include <cstring>

extern "C" {
#include <alloca.h>
}

int sinucaTracer::MemoryTraceFile::OpenFile(const char *sourceDir,
                                            const char *imgName, THREADID tid) {
    unsigned long bufferSize;
    char *path;

    bufferSize = GetPathTidInSize(sourceDir, "memory", imgName);
    path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imgName, tid, bufferSize);
    this->file = fopen(path, "rb");

    return !this->file;
}

int sinucaTracer::MemoryTraceFile::ReadStandardMemoryAccess() {
    if (this->file == NULL) return 1;
    unsigned long size =
        fread(&this->stdAccess, sizeof(this->stdAccess), 1, this->file);
    return size != sizeof(this->stdAccess);
}

int sinucaTracer::MemoryTraceFile::ReadNonStandardMemoryAccess() {
    if (this->file == NULL) return 1;
    unsigned long size =
        fread(&this->nonStdAccess, sizeof(this->nonStdAccess), 1, this->file);
    return size != sizeof(this->nonStdAccess);
}
