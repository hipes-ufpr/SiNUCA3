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

class MemoryTraceFile : public TraceFileReader {
  private:
    /**
     * @brief Reads number of operations from file.
     */
    unsigned short GetNumOps();
    /**
     * @brief Reads DataMEM array from file.
     * @param len Array size.
     */
    DataMEM *GetDataMemArr(unsigned short len);

  public:
    MemoryTraceFile(const char *folderPath, const char *img, THREADID tid);
    /**
     * @brief First reads buffer size, then the buffer itself.
     */
    void MemRetrieveBuffer();
    /**
     * @brief Reads vector of DataMEM struct from trace and number of operations
     * in case of a non standard memory access.
     * @note Read operations are always written before write ones.
     * @param insInfo Used to get static number of readings and writings.
     * @param dynInfo Where the information is copied to.
     */
    void ReadNextMemAccess(InstructionInfo *insInfo,
                           DynamicInstructionInfo *dynInfo);
};

}  // namespace sinucaTracer

#endif
