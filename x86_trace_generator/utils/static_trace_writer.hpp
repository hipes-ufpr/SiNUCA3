#ifndef SINUCA3_GENERATOR_STATIC_FILE_HPP_
#define SINUCA3_GENERATOR_STATIC_FILE_HPP_

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
 * @file static_trace_writer.hpp
 * @details .
 */

#include <tracer/sinuca/file_handler.hpp>

#include "pin.H"

namespace sinucaTracer {

/** @brief Check static_trace_writer.hpp documentation for details */
class StaticTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    StaticTraceRecord* recordArray;
    int recordArraySize;
    int recordArrayOccupation;

    inline int WriteHeaderToFile() {
        if (this->file == NULL) return 1;
        rewind(this->file);
        return (fwrite(&this->header, 1, sizeof(this->header), this->file) !=
                sizeof(this->header));
    }
    inline int FlushStaticRecords() {
        if (this->file == NULL) return 1;
        unsigned long occupationInBytes =
            sizeof(*this->recordArray) * this->recordArrayOccupation;
        return (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
                occupationInBytes);
    }

    int DoubleRecordArraySize();
    unsigned char ToUnsignedChar(bool val);

  public:
    inline StaticTraceWriter()
        : file(0),
          recordArray(0),
          recordArraySize(0),
          recordArrayOccupation(0) {
        this->header.fileType = FileTypeStaticTrace;
        this->header.data.staticHeader.instCount = 0;
        this->header.data.staticHeader.bblCount = 0;
        this->header.data.staticHeader.threadCount = 0;
    };
    inline ~StaticTraceWriter() {
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write static header!\n")
        }
        if (this->FlushStaticRecords()) {
            SINUCA3_ERROR_PRINTF("Failed to flush static records!\n");
        }
        if (this->file) {
            fclose(this->file);
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName);
    int AddInstruction(const INS* pinInst);
    int AddBasicBlockSize(unsigned int basicBlockSize);

    inline int LastInstIsMemoryRead() {
        return this->recordArray[this->recordArrayOccupation - 1]
            .data.instruction.instReadsMemory;
    }
    inline int LastInstIsMemoryWrite() {
        return this->recordArray[this->recordArrayOccupation - 1]
            .data.instruction.instWritesMemory;
    }
    inline void IncStaticInstructionCount() {
        this->header.data.staticHeader.instCount++;
    }
    inline void IncThreadCount() {
        this->header.data.staticHeader.threadCount++;
    }
    inline void IncBasicBlockCount() {
        this->header.data.staticHeader.bblCount++;
    }
    inline unsigned int GetBasicBlockCount() {
        return this->header.data.staticHeader.bblCount;
    }
};

}  // namespace sinucaTracer

#endif
