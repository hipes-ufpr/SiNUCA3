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

#include <pin.H>

#include <tracer/sinuca/file_handler.hpp>

extern "C" {
#include <stdlib.h>
}

namespace sinucaTracer {

class StaticTraceFile {
  private:
    FILE* file;
    FileHeader header;
    StaticRecord record;

    void ConvertPinInstToRawInstFormat(const INS* pinInstruction, Instruction* rawInst);
  public:
    inline StaticTraceFile() : file(NULL) {};
    inline ~StaticTraceFile() {
        if (file == NULL) fclose(this->file);
    }
    int OpenFile(const char* sourceDir, const char* imgName);
    int WriteHeaderToFile();
    int WriteStaticRecordToFile();

    inline void InitializeFileHeader() {
        this->header.data.staticHeader.bblCount = 0;
        this->header.data.staticHeader.instCount = 0;
        this->header.data.staticHeader.threadCount = 0;
    }
    inline void SetStaticRecordInstruction(const INS* pinInstruction) {
        this->ConvertPinInstToRawInstFormat(pinInstruction, &this->record.data.instruction);
    }
    inline void SetStaticRecordType(short type) {
        this->record.recordType = type;
    }
    inline void SetStaticRecordBasicBlockSize(unsigned int size) {
        this->record.data.basicBlockSize = size;
    }
    inline void IncBasicBlockCount() {
        this->header.data.staticHeader.bblCount++;
    }
    inline void IncStaticInstructionCount() {
        this->header.data.staticHeader.instCount++;
    }
    inline void IncThreadCount() {
        this->header.data.staticHeader.threadCount++;
    }
    inline unsigned int GetBBlCount() {
        return this->header.data.staticHeader.bblCount;
    }
};

}  // namespace sinucaTracer

#endif
