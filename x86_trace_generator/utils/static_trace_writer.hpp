#ifndef SINUCA3_GENERATOR_STATIC_FILE_HPP_
#define SINUCA3_GENERATOR_STATIC_FILE_HPP_

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
 * @file static_trace_writer.hpp
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

}  // namespace traceGenerator
}  // namespace trace

#endif
