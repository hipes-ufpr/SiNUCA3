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

namespace sinucaTracer {

class DynamicTraceWriter {
  private:
    FILE *file;
    FileHeader header;
    DynamicTraceRecord *recordArray;
    int recordArrayOccupation;
    int recordArraySize;

    inline int WriteHeaderToFile() {
        if (this->file == NULL) return 1;
        rewind(this->file);
        return (fwrite(&this->header, 1, sizeof(this->header), this->file) !=
                sizeof(this->header));
    }
    inline int FlushDynamicRecords() {
        if (this->file == NULL) return 1;
        unsigned long occupationInBytes =
            sizeof(*this->recordArray) * this->recordArrayOccupation;
        return (fwrite(this->recordArray, 1, occupationInBytes, this->file) !=
                occupationInBytes);
    }

    int DoubleRecordArraySize();

  public:
    inline DynamicTraceWriter()
        : file(0),
          recordArray(0),
          recordArrayOccupation(0),
          recordArraySize(0) {
        this->header.fileType = FileTypeDynamicTrace;
        this->header.data.dynamicHeader.totalExecutedInstructions = 0;
    };
    inline ~DynamicTraceWriter() {
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic file header!\n");
        }
        if (this->FlushDynamicRecords()) {
            SINUCA3_ERROR_PRINTF("Failed to flush dynamic records!\n");
        }
        if (file) {
            fclose(file);
        }
    }

    int OpenFile(const char *source, const char *img, int tid);
    int AddThreadEvent(unsigned char event, int tid);
    int AddBasicBlockId(int basicBlockId);

    inline void IncExecutedInstructions(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
};

}  // namespace sinucaTracer

#endif
