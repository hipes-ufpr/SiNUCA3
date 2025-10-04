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
 * @details All classes defined here inherit from TraceFileWriter and implement
 * the preparation and buffering/flush of data to each file making up a trace
 * (static, dynamic and memory files). All of them implement a PrepareData**
 * method and an AppendToBuffer** one. They should be called in the order:
 * PrepareData**, it fills data structures, and then AppendToBuffer** which
 * deals with buffering/flushing the data.
 */

#include <tracer/sinuca/file_handler.hpp>

#include "utils/logging.hpp"

extern "C" {
#include <search.h>
#include <stdlib.h>
}

namespace sinucaTracer {

const int HASH_MAP_INIT_SIZE = 50000;

class StaticTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    StaticTraceBasicBlockRecord* basicBlocksArray;
    StaticTraceDictionaryRecord* registeredInstArray;
    StaticTraceDictionaryEntry newInstruction;
    int basicBlocksArraySize;
    int basicBlocksArrayOccupation;
    int registeredInstArraySize;
    int registeredInstArrayOccupation;
    int instHashMapSize;

    inline int WriteHeaderToFile() {
        if (this->file == NULL) return 1;
        return (fwrite(&this->header, sizeof(this->header), 1, this->file) !=
                sizeof(this->header));
    }
    inline int WriteArrayToFile(void* array, unsigned long size) {
        if (this->file == NULL) return 1;
        return (fwrite(array, size, 1, this->file) != size);
    }

    void ConvertPinInstToRawInstFormat(const INS* pinInstruction);
    int DuplicateBasicBlocksArraySize();
    int DuplicateRegisteredInstArraySize();

  public:
    inline StaticTraceWriter()
        : file(0),
          basicBlocksArray(0),
          registeredInstArray(0),
          basicBlocksArraySize(0),
          basicBlocksArrayOccupation(0),
          registeredInstArraySize(0),
          registeredInstArrayOccupation(0) {
        this->header.fileType = FileTypeStaticTrace;
        this->header.data.staticHeader.instCount = 0;
        this->header.data.staticHeader.bblCount = 0;
        this->header.data.staticHeader.threadCount = 0;
    };
    inline ~StaticTraceWriter() {
        unsigned long size = 0;
        if (file == NULL) {
            fclose(this->file);
        }
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write static header!\n")
        }
        size = sizeof(*this->registeredInstArray) *
               this->registeredInstArrayOccupation;
        if (this->WriteArrayToFile(this->registeredInstArray, size)) {
            SINUCA3_ERROR_PRINTF("Failed to write static instructions!\n")
        }
        size =
            sizeof(*this->basicBlocksArray) * this->basicBlocksArrayOccupation;
        if (this->WriteArrayToFile(this->basicBlocksArray, size)) {
            SINUCA3_ERROR_PRINTF("Failed to write static basic blocks!\n")
        }
        hdestroy();
    }

    int OpenFile(const char* sourceDir, const char* imgName);
    int AddInstruction(const INS* pinInst);
    int AddBasicBlockSize(unsigned int basicBlockSize);

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
        return this->basicBlocksArrayOccupation;
    }
};

}  // namespace sinucaTracer

#endif
