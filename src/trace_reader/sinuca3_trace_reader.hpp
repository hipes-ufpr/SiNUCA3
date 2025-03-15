#ifndef SINUCA3_ORCS_TRACE_READER_HPP
#define SINUCA3_ORCS_TRACE_READER_HPP

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
 * @file orcs_trace_reader.hpp
 * @brief Port of the OrCS trace reader. https://github.com/mazalves/OrCS
 */

#include <cstdio>
#include <cstring>
#include "trace_reader.hpp"

inline void increaseOffset(size_t *offset, size_t size) {
    *offset += size;
}

namespace sinuca {
namespace traceReader {
namespace sinuca3TraceReader {

/** @brief  */
class SinucaTraceReader : public TraceReader {
  private:
    FILE *StaticTraceFile;
    FILE *DynamicTraceFile;
    FILE *MemoryTraceFile;

    // Controls the trace reading.
    bool isInsideBBL;
    unsigned int currentBBL;
    unsigned int currentOpcode;

    // Controls the static dictionary.
    // Total of BBLs for the static file.
    // Total of instructions for each BBL.
    unsigned int binaryTotalBBLs;
    unsigned short *binaryBBLsSize;
    // Complete dictionary of BBLs and instructions.
    InstructionPacket **binaryDict;

    unsigned long fetchInstructions;

    // Generate the static dictionary.
    int GetTotalBBLs();
    int GenerateBinaryDict();

    int TraceNextDynamic(unsigned int*);
    int TraceNextMemory(InstructionPacket*);

    FetchResult TraceFetch(InstructionPacket**);

  public:
    virtual int OpenTrace(const char* traceFileName);
    virtual unsigned long GetTraceSize();
    virtual unsigned long GetNumberOfFetchedInstructions();
    virtual void PrintStatistics();
    virtual FetchResult Fetch(const InstructionPacket** ret);

    inline ~SinucaTraceReader() {
        fclose(this->StaticTraceFile);
        fclose(this->DynamicTraceFile);
        fclose(this->MemoryTraceFile);
    }
};

}  // namespace orcsTraceReader
}  // namespace traceReader
}  // namespace sinuca

#endif  // SINUCA3_ORCS_TRACE_READER_HPP
