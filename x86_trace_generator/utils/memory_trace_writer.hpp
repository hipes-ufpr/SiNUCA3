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
    MemoryTraceRecord* recordArray;
    int recordArrayOccupation;
    int recordArraySize;

    inline int WriteHeaderToFile() {
        if (this->file == NULL) return 1;
        rewind(this->file);
        return (fwrite(&this->header, 1, sizeof(this->header), this->file) !=
                sizeof(this->header));
    }
    inline int FlushMemoryRecords() {
        if (this->file == NULL) return 1;
        unsigned long occupationInBytes =
            sizeof(*this->recordArray) * this->recordArrayOccupation;
        return (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
                occupationInBytes);
    }

    int DoubleRecordArraySize();

  public:
    inline MemoryTraceWriter()
        : file(0),
          recordArray(0),
          recordArrayOccupation(0),
          recordArraySize(0) {
            this->header.fileType = FileTypeMemoryTrace;
          };
    inline ~MemoryTraceWriter() {
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write memory file header!\n");
        }
        if (this->FlushMemoryRecords()) {
            SINUCA3_ERROR_PRINTF("Failed to flush memory records!\n");
        }
        if (file) {
            fclose(file);
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    int AddNumberOfMemOperations(unsigned int memoryOperations);
    int AddMemoryOperation(unsigned long address, unsigned int size,
                           unsigned char type);
};

}  // namespace sinucaTracer

#endif
