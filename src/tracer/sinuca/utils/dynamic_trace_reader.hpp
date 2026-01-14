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
 * @details .
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

#include "utils/logging.hpp"

/** @brief Check dynamic_trace_reader.hpp documentation for details */
class DynamicTraceReader {
  private:
    FILE* file;
    FileHeader header;
    DynamicTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int numberOfRecordsRead;
    int recordArrayIndex;
    bool reachedEnd;

    int LoadRecordArray();

  public:
    inline DynamicTraceReader()
        : file(0), numberOfRecordsRead(0), reachedEnd(0) {}
    inline ~DynamicTraceReader() {
        if (file) {
            fclose(this->file);
        }
        if (!this->reachedEnd && this->recordArrayIndex != numberOfRecordsRead) {
            SINUCA3_WARNING_PRINTF(
                "Basic block ids may have been left unread\n");
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    int ReadDynamicRecord();

    inline unsigned long GetTotalExecutedInstructions() {
        return this->header.data.dynamicHeader.totalExecutedInstructions;
    }
    inline DynamicTraceRecordType GetRecordType() {
        return (DynamicTraceRecordType)this->recordArray[this->recordArrayIndex]
        .recordType;
    }
    inline unsigned int GetBasicBlockIdentifier() {
        return this->recordArray[this->recordArrayIndex]
        .data.basicBlockId;
    }
    inline ThreadEventType GetThreadEvent() {
        return (ThreadEventType)this->recordArray[this->recordArrayIndex].data
        .threadEvent;
    }
    
    inline bool HasReachedEnd() { return this->reachedEnd; }
    inline unsigned int GetVersionInt() { return this->header.traceVersion; }
    inline unsigned int GetTargetInt() { return this->header.targetArch; }
};

#endif
