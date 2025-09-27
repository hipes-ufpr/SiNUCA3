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

#include <cassert>

extern "C" {
#include <alloca.h>
}

int sinucaTracer::DynamicTraceFile::OpenFile(const char *sourceDir, const char *imgName, THREADID tid) {
    unsigned long bufferSize;
    char* path;

    bufferSize = sinucaTracer::GetPathTidInSize(sourceDir, "dynamic", imgName);
    path = (char *)alloca(bufferSize);
    FormatPathTidIn(path, sourceDir, "dynamic", imgName, tid, bufferSize);
    this->file = fopen(path, "wb");
    if (this->file == NULL) return 1;
    /* This space will be used to store the file header. */
    fseek(this->file, sizeof(this->header), SEEK_SET);
    return 0;
}

int sinucaTracer::DynamicTraceFile::WriteHeaderToFile() {
    if (this->file == NULL) return 1;
    rewind(this->file);
    unsigned long written = fwrite(&this->header, sizeof(this->header), 1, this->file);
    return written != sizeof(this->header);
}

int sinucaTracer::DynamicTraceFile::WriteDynamicRecordToFile() {
    if (this->file == NULL) return 1;
    unsigned long written = fwrite(&this->record, sizeof(this->record), 1, this->file);
    return written != sizeof(this->record);
}
