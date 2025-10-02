#ifndef SINUCA3_GENERATOR_MEMORY_FILE_HPP_
#define SINUCA3_GENERATOR_MEMORY_FILE_HPP_

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
 * @file memory_trace_writer.hpp
 * @details All classes defined here inherit from TraceFileWriter and implement
 * the preparation and buffering/flush of data to each file making up a trace
 * (static, dynamic and memory files). All of them implement a PrepareData**
 * method and an AppendToBuffer** one. They should be called in the order:
 * PrepareData**, it fills data structures, and then AppendToBuffer** which
 * deals with buffering/flushing the data.
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

#include "pin.H"

const int MEM_PAD = 32;

namespace sinucaTracer {

class MemoryTraceWriter {
  private:
    FILE* file;
    MemoryTraceRecord record;
    char padding[MEM_PAD]; /**<Manual padding to avoid false sharing.> */

  public:
    MemoryTraceFile() : file(NULL) {}
    inline ~MemoryTraceFile() {
        if (this->file) fclose(this->file);
    };
    int OpenFile(const char* sourceDir, const char* imgName, THREADID tid);
    int WriteMemoryRecordToFile();

    inline void SetMemoryRecordOperation(unsigned long addr, unsigned int size,
                                  short type) {
        this->record.data.operation.addr = addr;
        this->record.data.operation.size = size;
        this->record.data.operation.type = type;
    }
    inline void SetMemoryRecordNonStdHeader(unsigned short nonStdReadOps,
                                     unsigned short nonStdWriteOps) {
        this->record.data.nonStdHeader.nonStdReadOps = nonStdReadOps;
        this->record.data.nonStdHeader.nonStdWriteOps = nonStdWriteOps;
    }
    inline void SetMemoryRecordType(short type) {
        this->record.recordType = type;
    }
};

}  // namespace sinucaTracer

#endif
