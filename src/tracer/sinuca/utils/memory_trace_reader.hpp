#ifndef SINUCA3_SINUCA_TRACER_MEMORY_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_MEMORY_TRACE_READER_HPP_

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file memory_trace_reader.hpp
 * @brief Class implementation of the memory trace reader.
 * @details The x86 based memory trace is a binary file having a list of memory
 * operations in sequential order. Each operation is a pair of address and size.
 * Since the final buffer size is not a fixed value, the pintool also stores
 * this value in the file before each buffer flush.
 */

#include <sinuca3.hpp>
#include <tracer/sinuca/file_handler.hpp>

namespace sinucaTracer {

class MemoryTraceReader {
  private:
    FILE* file;
    MemoryRecord record;

  public:
    inline MemoryTraceFile() : file(NULL) {};
    inline ~MemoryTraceFile() {
        if (file) fclose(this->file);
    }
    int OpenFile(const char* sourceDir, const char* imgName, THREADID tid);
    int ReadMemoryRecordFromFile();
    void ExtractMemoryOperation(unsigned long* addr, unsigned int* size);
    void ExtractNonStdHeader(unsigned short* readOps, unsigned short* writeOps);

    inline int GetMemoryRecordType() {
        return this->record.recordType;
    }
    inline int GetMemoryOperationType() {
        return this->record.data.operation.type;
    }
};

}  // namespace sinucaTracer

#endif
