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

#include <vector>

namespace sinucaTracer {

/** @brief Check memory_trace_reader.hpp documentation for details */
class MemoryTraceReader {
  private:
    FILE* file;
    FileHeader header;
    MemoryTraceRecord record;
    std::vector<unsigned long> loadOpsAddressVec;
    std::vector<unsigned long> storeOpsAddressVec;
    std::vector<unsigned short> loadOpsSizeVec;
    std::vector<unsigned short> storeOpsSizeVec;
    int numberOfMemLoadOps;
    int numberOfMemStoreOps;

  public:
    inline MemoryTraceReader() : file(NULL) {};
    inline ~MemoryTraceReader() {
        if (file) {
            fclose(this->file);
        }
    }

    int OpenFile(const char* sourceDir, const char* imgName, int tid);
    int ReadMemoryOperations();
    int CopyLoadOpsAddresses(unsigned long* array, unsigned long size);
    int CopyLoadOpsSizes(unsigned int* array, unsigned long size);
    int CopyStoreOpsAddresses(unsigned long* array, unsigned long size);
    int CopyStoreOpsSizes(unsigned int* array, unsigned long size);

    inline int GetNumberOfMemLoadOps() {
        int numOps = this->numberOfMemLoadOps;
        this->numberOfMemLoadOps = 0;
        return numOps;
    }
    inline int GetNumberOfMemStoreOps() {
        int numOps = this->numberOfMemStoreOps;
        this->numberOfMemStoreOps = 0;
        return numOps;
    }
};

}  // namespace sinucaTracer

#endif
