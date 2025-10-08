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

#include <sinuca3.hpp>
#include <tracer/sinuca/file_handler.hpp>
#include <utils/circular_buffer.hpp>

namespace sinucaTracer {

/** @brief Check memory_trace_reader.hpp documentation for details */
class MemoryTraceReader {
  private:
    FILE* file;
    FileHeader header;
    MemoryTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int recordArraySize;
    int recordArrayIndex;
    bool reachedEnd;

    CircularBuffer loadOperations;
    CircularBuffer storeOperations;

    int totalLoadOps;
    int totalStoreOps;

    int LoadRecordArray();
    int CopyOperations(unsigned long* addrsArr, int addrsArrS,
                       unsigned int* sizesArr, int sizesArrS, bool isOpLoad);

  public:
    inline MemoryTraceReader()
        : file(0), recordArraySize(0), recordArrayIndex(0), reachedEnd(0) {
        this->loadOperations.Allocate(0, sizeof(*this->recordArray));
        this->storeOperations.Allocate(0, sizeof(*this->recordArray));
    };
    inline ~MemoryTraceReader() {
        if (file) {
            fclose(this->file);
        }
        if (this->loadOperations.GetOccupation() > 0) {
            SINUCA3_WARNING_PRINTF("Load operations left unread!\n");
        }
        this->loadOperations.Deallocate();
        if (this->storeOperations.GetOccupation() > 0) {
            SINUCA3_WARNING_PRINTF("Store operations left unread!\n");
        }
        this->storeOperations.Deallocate();
    }

    int OpenFile(const char* sourceDir, const char* imgName, int tid);
    int ReadMemoryOperations();

    int CopyLoadOperations(unsigned long* addrsArray, int addrsArraySize,
                           unsigned int* sizesArray, int sizesArraySize);
    int CopyStoreOperations(unsigned long* addrsArray, int addrsArraySize,
                            unsigned int* sizesArray, int sizesArraySize);

    inline bool ReachedMemoryTraceEnd() {
        return (this->reachedEnd &&
                this->recordArrayIndex == this->recordArraySize);
    }
    inline int GetNumberOfLoads() { return this->totalLoadOps; }
    inline int GetNumberOfStores() { return this->totalStoreOps; }
};

}  // namespace sinucaTracer

#endif
