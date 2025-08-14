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

tracer::traceGenerator::DynamicTraceFile::DynamicTraceFile(const char* source,
                                                          const char* img,
                                                          THREADID tid) {
    unsigned long bufferSize = tracer::GetPathTidInSize(source, "dynamic", img);
    char* path = (char*)alloca(bufferSize);

    FormatPathTidIn(path, source, "dynamic", img, tid, bufferSize);
    this->UseFile(path);

    this->totalExecInst = 0;
    /*
     * This space will be used to store the total of instruction executed per
     * thread.
     */
    fseek(this->tf.file, 1 * sizeof(this->totalExecInst), SEEK_SET);
}

tracer::traceGenerator::DynamicTraceFile::~DynamicTraceFile() {
    SINUCA3_DEBUG_PRINTF("Last DynamicTraceFile flush\n");
    if (this->tf.offsetInBytes > 0) {
        this->FlushBuffer();
    }

    rewind(this->tf.file);
    fwrite(&this->totalExecInst, sizeof(this->totalExecInst), 1, this->tf.file);
    SINUCA3_DEBUG_PRINTF("totalExecInst [%lu]\n", this->totalExecInst);
}

void tracer::traceGenerator::DynamicTraceFile::PrepareId(BBLID id) {
    this->bblId = id;
}

void tracer::traceGenerator::DynamicTraceFile::IncTotalExecInst(int ins) {
    this->totalExecInst += ins;
}

void tracer::traceGenerator::DynamicTraceFile::AppendToBufferId() {
    this->DynamicAppendToBuffer(&this->bblId, sizeof(this->bblId));
}

void tracer::traceGenerator::DynamicTraceFile::DynamicAppendToBuffer(
    void* ptr, unsigned long len) {
    if (this->AppendToBuffer(ptr, len)) {
        this->FlushBuffer();
        this->AppendToBuffer(ptr, len);
    }
}
