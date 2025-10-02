#ifndef SINUCA3_GENERATOR_DYNAMIC_FILE_HPP_
#define SINUCA3_GENERATOR_DYNAMIC_FILE_HPP_

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
 * @file dynamic_trace_writer.hpp
 * @details All classes defined here inherit from TraceFileWriter and implement
 * the preparation and buffering/flush of data to each file making up a trace
 * (static, dynamic and memory files). All of them implement a PrepareData**
 * method and an AppendToBuffer** one. They should be called in the order:
 * PrepareData**, it fills data structures, and then AppendToBuffer** which
 * deals with buffering/flushing the data.
 */

#include <pin.H>

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

const int DYN_PAD = 24;

namespace sinucaTracer {

class DynamicTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    DynamicTraceRecord record;
    char padding[DYN_PAD]; /**<Manual padding to avoid false sharing.> */

  public:
    DynamicTraceFile() : file(NULL) {};
    inline ~DynamicTraceFile() { if (file) fclose(file); }
    int OpenFile(const char *source, const char *img, THREADID tid);
    int WriteHeaderToFile();
    int WriteDynamicRecordToFile();

    inline void InitializeFileHeader() {
        this->header.data.dynamicHeader.totalExecutedInstructions = 0;
    }
    inline void SetDynamicRecordType(short type) {
        this->record.recordType = type;
    }
    inline void SetDynamicRecordRoutineName(const char *name) {
        strncpy(this->record.data.routineName, name,
                sizeof(this->record.data.routineName) - 1);
    }
    inline void SetDynamicRecordBasicBlockId(unsigned long identifier) {
        this->record.data.basicBlockIdentifier = identifier;
    }
    inline void IncTotalExecInst(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
};

}  // namespace sinucaTracer

#endif
