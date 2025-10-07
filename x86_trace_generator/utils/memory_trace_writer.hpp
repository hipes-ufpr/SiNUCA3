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
 * @details .
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

namespace sinucaTracer {

/** @brief Check memory_trace_writer.hpp documentation for details */
class MemoryTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    MemoryTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int recordArrayOccupation;

    inline void ResetRecordArray() {
        this->recordArrayOccupation = 0;
    }

    int FlushRecordArray();

  public:
    inline MemoryTraceWriter()
        : file(0),
          recordArrayOccupation(0)
          {
            this->header.fileType = FileTypeMemoryTrace;
          };
    inline ~MemoryTraceWriter() {
        if (this->header.FlushHeader(this->file)) {
            SINUCA3_ERROR_PRINTF("Failed to write memory file header!\n");
        }
        if (!this->IsRecordArrayEmpty()) {
            if (this->FlushRecordArray()) {
                SINUCA3_ERROR_PRINTF("[1] Failed to flush memory records!\n");
            }
        }
        if (file) {
            fclose(file);
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    int AddNumberOfMemOperations(unsigned int memoryOperations);
    int AddMemoryOperation(unsigned long address, unsigned int size,
                           unsigned char type);

    inline int IsRecordArrayEmpty() {
        return (this->recordArrayOccupation <= 0);
    }
    inline int IsRecordArrayFull() {
        return (this->recordArrayOccupation == RECORD_ARRAY_SIZE);
    }
};

}  // namespace sinucaTracer

#endif
