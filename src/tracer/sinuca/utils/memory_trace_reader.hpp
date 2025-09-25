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
 * @details The x86 based memory trace is a binary file having a list of memory
 * operations in sequential order. Each operation is a pair of address and size.
 * Since the final buffer size is not a fixed value, the pintool also stores
 * this value in the file before each buffer flush.
 */

#include <sinuca3.hpp>
#include <tracer/sinuca/file_handler.hpp>

namespace sinucaTracer {

class MemoryTraceFile {
  private:
    FILE* file;
    StandardMemoryAccess stdAccess;
    NonStandardMemoryAccess nonStdAccess;

  public:
    inline MemoryTraceFile() : file(NULL){};
    inline ~MemoryTraceFile() {
        if (file) fclose(this->file);
    }
    int OpenFile(const char* sourceDir, const char* imgName, THREADID tid);
    int ReadStandardMemoryAccess();
    int ReadNonStandardMemoryAccess();
    inline unsigned long GetAddressStdAccess() { return this->stdAccess.addr; }
    inline unsigned int GetSizeStdAccess() { return this->stdAccess.size; }
    inline int GetTypeStdAccess() { return this->stdAccess.type; }
    inline unsigned int GetReadOpsNonStdAcc() {
        return this->nonStdAccess.readOps;
    }
    inline unsigned int GetWriteOpsNonStdAcc() {
        return this->nonStdAccess.writeOps;
    }
    inline const unsigned long* GetReadAddrArrayNonStdAcc(unsigned long *s) {
        *s = sizeof(this->nonStdAccess.readAddrs);
        return this->nonStdAccess.readAddrs;
    }
    inline const unsigned long* GetWriteAddrArrayNonStdAcc(unsigned long *s) {
        *s = sizeof(this->nonStdAccess.writeAddrs);
        return this->nonStdAccess.writeAddrs;
    }
    inline const unsigned int* GetReadSizeArrayNonStdAcc(unsigned long *s) {
        *s = sizeof(this->nonStdAccess.readSize);
        return this->nonStdAccess.readSize;
    }
    inline const unsigned int* GetWriteSizeArrayNonStdAcc(unsigned long *s) {
        *s = sizeof(this->nonStdAccess.writeSize);
        return this->nonStdAccess.writeSize;
    }
};

}  // namespace sinucaTracer

#endif
