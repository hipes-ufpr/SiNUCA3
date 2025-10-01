#ifndef SINUCA3_SINUCA_TRACER_DYNAMIC_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_DYNAMIC_TRACE_READER_HPP_

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
 * @file dynamic_trace_reader.hpp
 * @brief Class implementation of the dynamic trace reader.
 * @details The x86 based dynamic trace is a binary file having a sequential
 * list of indices of basic blocks indicating the flow of execution. Each index
 * is stored as a BBLID, a type defined in x86_file_handler header.
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

namespace sinucaTracer {

class DynamicTraceReader {
  private:
    FILE* file;
    FileHeader header;
    ExecutionRecord record;

  public:
    inline DynamicTraceFile() : file(NULL) {};
    inline ~DynamicTraceFile() {
        if (file) fclose(this->file);
    }
    int OpenFile(const char* sourceDir, const char* imgName, THREADID tid);
    int ReadHeaderFromFile();
    int ReadDynamicRecordFromFile();

    inline int GetDynamicRecordType() {
        return this->record.recordType;
    }
    inline unsigned long GetBasicBlockIdentifier() {
        return this->record.data.basicBlockIdentifier;
    }
    inline const char* GetRoutineName() {
        return this->record.data.routineName;
    }
    inline unsigned long GetTotalExecutedInstructions() {
        return this->header.data.dynamicHeader.totalExecutedInstructions;
    }
};

}  // namespace sinucaTracer

#endif
