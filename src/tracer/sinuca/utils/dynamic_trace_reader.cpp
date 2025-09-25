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
#include <cstring>
#include <sinuca3.hpp>

extern "C" {
#include <alloca.h>
}

int sinucaTracer::DynamicTraceFile::OpenFile(const char *sourceDir,
                                             const char *imgName,
                                             THREADID tid) {
    unsigned long bufferSize;
    char *path;

    bufferSize = GetPathTidInSize(sourceDir, "dynamic", imgName);
    path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imgName, tid, bufferSize);
    this->file = fopen(path, "rb");

    return !this->file;
}

int sinucaTracer::DynamicTraceFile::ReadHeaderFromFile() {
    if (this->file == NULL) return 1;
    unsigned long size = fread(&header, sizeof(header), 1, this->file);
    return size != sizeof(header);
}

int sinucaTracer::DynamicTraceFile::ReadRecordFromFile() {
    if (this->file == NULL) return 1;
    unsigned long size = fread(&record, sizeof(record), 1, this->file);
    return size != sizeof(record);
}
