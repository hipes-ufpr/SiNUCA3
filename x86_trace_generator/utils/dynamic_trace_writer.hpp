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
        return (fwrite(&this->header, sizeof(this->header), 1, this->file) !=
                sizeof(this->header));
    }
    inline int WriteArrayToFile(void* array, unsigned long size) {
        if (this->file == NULL) return 1;
        return (fwrite(array, size, 1, this->file) != size);
    }

    int DuplicateRecordArraySize();

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
        unsigned long size = 0;
        if (file) {
            fclose(file);
        }
        if (this->WriteHeaderToFile()) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic file header!\n");
        }
        size = sizeof(*this->recordArray) * this->recordArrayOccupation;
        if (this->WriteArrayToFile(this->recordArray, size)) {
            SINUCA3_ERROR_PRINTF("Failed to write dynamic record entries!\n");
        }
    }

    int OpenFile(const char *source, const char *img, int tid);
    int AddThreadEvent(unsigned char event, int tid);
    int AddBasicBlockId(int id);

    inline void IncTotalExecInst(int ins) {
        this->header.data.dynamicHeader.totalExecutedInstructions += ins;
    }
};

}  // namespace sinucaTracer

#endif
