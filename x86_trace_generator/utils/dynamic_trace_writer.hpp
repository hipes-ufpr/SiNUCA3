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
 * @details .
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>
#include "utils/logging.hpp"

/** @brief Check dynamic_trace_writer.hpp documentation for details */
class DynamicTraceWriter {
  private:
    FILE *file;
    FileHeader header;
    DynamicTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int recordArrayOccupation;

    inline int FlushDynamicRecords() {
        if (this->file == NULL) return 1;
        unsigned long occupationInBytes =
            sizeof(*this->recordArray) * this->recordArrayOccupation;
        return (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
                occupationInBytes);
    }
    inline void ResetRecordArray() {
        this->recordArrayOccupation = 0;
    }

    int FlushRecordArray();

  public:
    inline DynamicTraceWriter()
        : file(0),
          recordArrayOccupation(0) {
        this->header.fileType = FileTypeDynamicTrace;
    };
    inline ~DynamicTraceWriter() {
        if (this->header.FlushHeader(this->file)) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic file header!\n");
        }
        if (!this->IsRecordArrayEmpty()) {
            if (this->FlushRecordArray()) {
                SINUCA3_ERROR_PRINTF("Failed to flush dynamic records!\n");
            }
        }
        if (file) {
            fclose(file);
        }
    }

    int OpenFile(const char *source, const char *img, int tid);
    int AddThreadEvent(unsigned char event, int eid);
    int AddBasicBlockId(int basicBlockId);

    inline void IncExecutedInstructions(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
    inline int IsRecordArrayEmpty() {
        return (this->recordArrayOccupation <= 0);
    }
    inline int IsRecordArrayFull() {
        return (this->recordArrayOccupation == RECORD_ARRAY_SIZE);
    }
};

#endif
