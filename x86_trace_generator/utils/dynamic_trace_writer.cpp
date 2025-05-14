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

#include "../../src/utils/logging.hpp"
extern "C" {
#include <alloca.h>
}

trace::traceGenerator::DynamicTraceFile::DynamicTraceFile(const char* source,
                                                          const char* img,
                                                          THREADID tid) {
    unsigned long bufferSize = trace::GetPathTidInSize(source, "dynamic", img);
    char* path = (char*)alloca(bufferSize);
    FormatPathTidIn(path, source, "dynamic", img, tid, bufferSize);

    this->::trace::TraceFileWriter::UseFile(path);
}

trace::traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last DynamicTraceFile flush\n");
    if (this->tf.offset > 0) {
        this->FlushBuffer();
    }
}

void trace::traceGenerator::DynamicTraceFile::PrepareId(BBLID id) {
    this->bblId = id;
}

void trace::traceGenerator::DynamicTraceFile::AppendToBufferId() {
    this->DynamicAppendToBuffer(&this->bblId, sizeof(this->bblId));
}

void trace::traceGenerator::DynamicTraceFile::DynamicAppendToBuffer(
    void* ptr, unsigned long len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}
