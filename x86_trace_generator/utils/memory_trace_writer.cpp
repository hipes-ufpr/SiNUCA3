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
 * @file memory_trace_writer.cpp
 * @details Implementation of MemoryTraceFile class.
 */

#include "memory_trace_writer.hpp"

#include <cassert>
#include <cstddef>

extern "C" {
#include <alloca.h>
}

int sinucaTracer::MemoryTraceFile::OpenFile(const char* sourceDir,
                                            const char* imgName, THREADID tid) {
    unsigned long bufferSize;

    bufferSize = sinucaTracer::GetPathTidInSize(sourceDir, "memory", imgName);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "memory", imgName, tid, bufferSize);
    this->file = fopen(path, "wb");

    return this->file == NULL;
}

int sinucaTracer::MemoryTraceFile::WriteMemoryRecordToFile() {
    if (this->file == NULL) return 1;
    unsigned long written =
        fwrite(&this->record, sizeof(this->record), 1, this->file);
    return written != sizeof(this->record);
}
