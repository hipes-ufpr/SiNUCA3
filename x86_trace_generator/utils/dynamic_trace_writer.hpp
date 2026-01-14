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
 * @details A dynamic trace stores the basic blocks that were executed and
 * thread events (e.g. when a thread reaches a barrier). The trace reader uses
 * this info to simulate an execution. This file defines the DynamicTraceWriter
 * class which encapsulates the dynamic trace file and the methods that may
 * modify it.
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

#include "utils/logging.hpp"

/** @brief Check dynamic_trace_writer.hpp documentation for details */
class DynamicTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    DynamicTraceRecord recordArray[RECORD_ARRAY_SIZE]; /**<Buffer of records. */
    int recordArrayOccupation; /**<The number of records currently stored. */

    inline void ResetRecordArray() { this->recordArrayOccupation = 0; }
    inline int IsRecordArrayEmpty() {
        return (this->recordArrayOccupation <= 0);
    }
    inline int IsRecordArrayFull() {
        return (this->recordArrayOccupation == RECORD_ARRAY_SIZE);
    }

    int FlushRecordArray();
    int CheckRecordArray();
    int AddDynamicRecord(DynamicTraceRecord record);

  public:
    inline DynamicTraceWriter() : file(0), recordArrayOccupation(0) {
        this->header.SetHeaderType(FileTypeDynamicTrace);
    };
    inline ~DynamicTraceWriter() {
        if (!this->IsRecordArrayEmpty()) {
            if (this->FlushRecordArray()) {
                SINUCA3_ERROR_PRINTF("Failed to flush dynamic records!\n");
            }
        }
        if (this->header.FlushHeader(this->file)) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic file header!\n");
        }
        if (file) {
            fclose(file);
        }
    }

    /** @brief Create the [tid] dynamic file in the [sourceDir] directory. */
    int OpenFile(const char* sourceDir, const char* img, int tid);
    /**
     * @brief Add thread event record to the trace file.
     * @param type Event type
     * @param mutexAddr Unique identifier to mutex. Field ignored if the event
     * does not request it.
    */
    int AddThreadEvent(ThreadEventType evType);
    /** @brief Add the identifier of basic block executed. */
    int AddBasicBlockId(unsigned int basicBlockId);

    inline void IncExecutedInstructions(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
    inline void SetTargetArch(unsigned char target) {
        this->header.targetArch = target;
    }
};

#endif
