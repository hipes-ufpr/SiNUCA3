#ifndef SINUCA3_SINUCA_TRACER_MEMORY_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_MEMORY_TRACE_READER_HPP_

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
 * @file memory_trace_reader.hpp
 * @brief Class implementation of the memory trace reader.
 * @details The x86 based memory trace is a binary file .
 */

#include <engine/default_packets.hpp>
#include <tracer/sinuca/file_handler.hpp>
#include <utils/logging.hpp>

/** @brief Check memory_trace_reader.hpp documentation for details */
class MemoryTraceReader {
  private:
    FILE* file;
    FileHeader header;
    MemoryTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int numberOfRecordsRead;
    int recordArrayIndex;
    bool reachedEnd;

    int LoadRecordArray();

  public:
    inline MemoryTraceReader()
        : file(0), numberOfRecordsRead(0), recordArrayIndex(0), reachedEnd(0) {
          };
    inline ~MemoryTraceReader() {
        if (file) {
            fclose(this->file);
        }
        if (!this->reachedEnd &&
            this->recordArrayIndex != numberOfRecordsRead) {
            SINUCA3_WARNING_PRINTF(
                "Memory operations may have been left unread!\n");
        }
    }

    int OpenFile(const char* sourceDir, const char* imgName, int tid);
    int ReadMemoryOperations(InstructionPacket* inst);

    inline bool HasReachedEnd() { return this->reachedEnd; }
    inline unsigned int GetVersionInt() { return this->header.traceVersion; }
    inline unsigned int GetTargetInt() { return this->header.targetArch; }
};

#endif
