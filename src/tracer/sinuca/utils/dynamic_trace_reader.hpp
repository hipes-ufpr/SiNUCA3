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

namespace sinucaTracer {

/** @brief Check dynamic_trace_reader.hpp documentation for details */
class DynamicTraceReader {
  private:
    FILE* file;
    FileHeader header;
    DynamicTraceRecord record;
    bool reachedEnd;

  public:
    inline DynamicTraceReader() : file(0), reachedEnd(0) {}
    inline ~DynamicTraceReader() {
        if (file) {
            fclose(this->file);
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    int ReadDynamicRecord();

    inline bool ReachedDynamicTraceEnd() {
        return this->reachedEnd;
    }
    inline int GetRecordType() {
        return this->record.recordType;
    }
    inline unsigned long GetBasicBlockIdentifier() {
        return this->record.data.bbl.basicBlockIdentifier;
    }
    inline unsigned long GetTotalExecutedInstructions() {
        return this->header.data.dynamicHeader.totalExecutedInstructions;
    }
    inline unsigned char GetThreadEvent() {
        return this->record.data.thr.event;
    }
    inline unsigned int GetThreadIdentifier() {
        return this->record.data.thr.threadId;
    }
};

}  // namespace sinucaTracer

#endif
