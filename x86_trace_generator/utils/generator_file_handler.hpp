#ifndef SINUCA3_GENERATOR_FILE_HANDLER_HPP_
#define SINUCA3_GENERATOR_FILE_HANDLER_HPP_

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
 * @file generator_file_handler.hpp
 * @details All classes defined here inherit from TraceFileWriter and implement
 * the preparation and buffering/flush of data to each file making up a trace
 * (static, dynamic and memory files). All of them implement a PrepareData**
 * method and an AppendToBuffer** one. They should be called in the order:
 * PrepareData**, it fills data structures, and then AppendToBuffer** which
 * deals with buffering/flushing the data.
 */

#include "../../src/utils/file_handler.hpp"
#include "pin.H"

namespace trace {
namespace traceGenerator {

// Set to be equal to same constant declared in default_packets.hpp
const unsigned int MAX_MEM_OPERATIONS = 16;

class StaticTraceFile : public TraceFileWriter {
  private:
    struct DataINS data;
    unsigned int threadCount;
    unsigned int bblCount;
    unsigned int instCount;

    void ResetFlags();
    void SetFlags(const INS *pinInstruction);
    void SetBranchFields(const INS *pinInstruction);
    void FillRegs(const INS *pinInstruction);
    void StaticAppendToBuffer(void *ptr, unsigned long len);

  public:
    StaticTraceFile(const char *source, const char *img);
    ~StaticTraceFile();
    void PrepareDataINS(const INS *pinInstruction);
    void AppendToBufferDataINS();
    void AppendToBufferNumIns(unsigned int numIns);
    inline void IncBBlCount() { this->bblCount++; }
    inline void IncInstCount() { this->instCount++; }
    inline void IncThreadCount() { this->threadCount++; }
    inline unsigned int GetBBlCount() { return this->bblCount; }
};

class DynamicTraceFile : public TraceFileWriter {
  private:
    BBLID bblId;

    void DynamicAppendToBuffer(void *ptr, unsigned long len);

  public:
    DynamicTraceFile(const char *source, const char *img, THREADID tid);
    ~DynamicTraceFile();
    void PrepareId(BBLID id);
    void AppendToBufferId();
};

class MemoryTraceFile : public TraceFileWriter {
  private:
    struct DataMEM readOps[MAX_MEM_OPERATIONS];
    struct DataMEM writeOps[MAX_MEM_OPERATIONS];
    struct DataMEM stdAccessOp;
    unsigned int numReadOps;
    unsigned int numWriteOps;
    bool wasLastOperationStd;

    void MemoryAppendToBuffer(void *ptr, unsigned long len);

  public:
    MemoryTraceFile(const char *source, const char *img, THREADID tid);
    ~MemoryTraceFile();
    void PrepareDataNonStdAccess(PIN_MULTI_MEM_ACCESS_INFO *pinNonStdInfo);
    void PrepareDataStdMemAccess(unsigned long addr, unsigned int opSize);
    void AppendToBufferLastMemoryAccess();
};

}  // namespace traceGenerator
}  // namespace trace

#endif
