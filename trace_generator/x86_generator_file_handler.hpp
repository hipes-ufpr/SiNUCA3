#ifndef SINUCA3_X86_GENERATOR_FILE_HANDLER_HPP_
#define SINUCA3_X86_GENERATOR_FILE_HANDLER_HPP_

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
 * @file x86_generator_file_handler.hpp
 * @details Public API of the SiNUCA3 x86_64 tracer.
 */

#include "../src/utils/file_handler.hpp"
#include "pin.H"

namespace trace {
namespace traceGenerator {

class StaticTraceFile : public TraceFileWriter {
  private:
    unsigned int threadCount;
    unsigned int bblCount;
    unsigned int instCount;

    void ResetFlags(struct DataINS *);
    void SetFlags(struct DataINS *, const INS *);
    void SetBranchFields(struct DataINS *, const INS *);
    void FillRegs(struct DataINS *, const INS *);

  public:
    StaticTraceFile(const char *, const char *);
    ~StaticTraceFile();
    void PrepareData(struct DataINS *, const INS *);
    void StAppendToBuffer(void *, size_t);
    inline void IncBBlCount() { this->bblCount++; }
    inline void IncInstCount() { this->instCount++; }
    inline void IncThreadCount() { this->threadCount++; }
    inline unsigned int GetBBlCount() { return this->bblCount; }
};

class DynamicTraceFile : public TraceFileWriter {
  public:
    DynamicTraceFile(const char *, const char *, THREADID);
    ~DynamicTraceFile();
    void DynAppendToBuffer(void *, size_t);
};

class MemoryTraceFile : public TraceFileWriter {
  public:
    MemoryTraceFile(const char *, const char *, THREADID);
    ~MemoryTraceFile();
    void PrepareDataNonStdAccess(unsigned short *, struct DataMEM[],
                                 unsigned short *, struct DataMEM[],
                                 PIN_MULTI_MEM_ACCESS_INFO *);
    void MemAppendToBuffer(void *, size_t);
};

}  // namespace traceGenerator
}  // namespace trace

#endif
