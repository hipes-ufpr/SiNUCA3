#ifndef SINUCA3_SINUCA_TRACER_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_TRACE_READER_HPP_

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
 * @file trace_reader.hpp
 * @brief Implementation of SinucaTraceReader for x86 based traces.
 */

#include <tracer/sinuca/utils/dynamic_trace_reader.hpp>
#include <tracer/sinuca/utils/memory_trace_reader.hpp>
#include <tracer/sinuca/utils/static_trace_reader.hpp>
#include <tracer/trace_reader.hpp>

#include "engine/default_packets.hpp"

namespace sinucaTracer {

struct ThreadData {
    DynamicTraceReader* dynFile;
    MemoryTraceReader* memFile;
    unsigned long currentBasicBlock; /**<Index of basic block. */
    unsigned long fetchedInst;       /**<Number of instructions fetched */
    int currentInst; /**<Index of instruction inside basic block. */
    bool isInsideBasicBlock;

    int Allocate(const char* sourceDir, const char* imageName, int tid);

    inline ThreadData()
        : dynFile(0),
          memFile(0),
          currentBasicBlock(0),
          currentInst(0),
          isInsideBasicBlock(0) {}
    inline ~ThreadData() {
        if (this->memFile) delete this->memFile;
        if (this->dynFile) delete this->dynFile;
    }
};

/** @brief Check trace_reader.hpp documentation for details */
class SinucaTraceReader : public TraceReader {
  private:
    ThreadData* threadDataArray;
    StaticTraceReader* staticTrace;
    StaticInstructionInfo** instructionDict;
    StaticInstructionInfo* instructionPool;
    unsigned long totalBasicBlocks; /**<Number of basic blocks in static file.*/
    int* basicBlockSizeArr; /**<Number of instructions per basic block. */
    int totalStaticInst;
    int totalThreads;

    /**
     * @brief Fill instructions dictionary.
     * @return 1 on failure, 0 otherwise.
     */
    int GenerateInstructionDict();

  public:
    inline SinucaTraceReader()
        : threadDataArray(0),
          staticTrace(0),
          instructionDict(0),
          instructionPool(0),
          totalBasicBlocks(0),
          basicBlockSizeArr(0),
          totalThreads(0) {}
    virtual inline ~SinucaTraceReader() {
        delete[] this->threadDataArray;
        delete[] this->instructionDict;
        delete[] this->instructionPool;
        delete[] this->basicBlockSizeArr;
        delete this->staticTrace;
    }

    virtual FetchResult Fetch(InstructionPacket* ret, int tid);
    virtual int OpenTrace(const char* imageName, const char* sourceDir);
    virtual void PrintStatistics();

    virtual unsigned long GetNumberOfFetchedInst(int tid) {
        return this->threadDataArray[tid].fetchedInst;
    }
    virtual unsigned long GetTotalInstToBeFetched(int tid) {
        return this->threadDataArray[tid]
            .dynFile->GetTotalExecutedInstructions();
    }

    virtual inline int GetTotalThreads() { return this->totalThreads; }
    virtual inline int GetTotalBasicBlocks() { return this->totalBasicBlocks; }
};

}  // namespace sinucaTracer

#ifndef NDEBUG
int TestTraceReader();
#endif

#endif  // SINUCA3_SINUCA_TRACER_TRACE_READER_HPP_
