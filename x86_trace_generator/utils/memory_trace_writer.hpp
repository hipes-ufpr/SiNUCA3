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
#include <cstring>
#include <tracer/sinuca/file_handler.hpp>

#include "utils/logging.hpp"

namespace sinucaTracer {

class MemoryTraceWriter {
  private:
    FILE* file;
    FileHeader header; /**<header currently unused here.> */
    MemoryTraceRecord* recordArray;
    int recordArrayOccupation;
    int recordArraySize;

    inline int WriteArrayToFile(void* array, unsigned long size) {
        if (this->file == NULL) return 1;
        return (fwrite(array, size, 1, this->file) != size);
    }

    int DuplicateRecordArraySize();

  public:
    inline MemoryTraceWriter()
        : file(0),
          recordArray(0),
          recordArrayOccupation(0),
          recordArraySize(0) {};
    inline ~MemoryTraceWriter() {
        unsigned long size = 0;
        if (file) {
            fclose(file);
        }
        size = sizeof(*this->recordArray) * this->recordArrayOccupation;
        if (this->WriteArrayToFile(this->recordArray, size)) {
            SINUCA3_ERROR_PRINTF("Failed to write mem record entries!\n");
        }
    }

    int OpenFile(const char* sourceDir, const char* imgName, int tid);
    int AddNonStdOpHeader(unsigned int memoryOperations);
    int AddMemoryOperation(unsigned long address, unsigned int size,
                           unsigned char type);
};

}  // namespace sinucaTracer

#endif
