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
#include "utils/circular_buffer.hpp"
#include "utils/logging.hpp"

struct Lock {
    unsigned long addr;
    bool isBusy;
    int owner;
    int recCont;

    CircularBuffer waitingThreadsQueue;
};

struct Barrier {
    int thrCont;
};

struct ThreadData {
    DynamicTraceReader dynFile;
    MemoryTraceReader memFile;
    unsigned long currentBasicBlock; /**<Index of basic block. */
    unsigned long fetchedInst;       /**<Number of instructions fetched */
    int currentInst; /**<Index of instruction inside basic block. */
    int parentThreadId;
    bool isInsideBasicBlock;
    bool isThreadAwake;

    int Allocate(const char* sourceDir, const char* imageName, int tid);

    inline ThreadData()
        : currentBasicBlock(0),
          currentInst(0),
          isInsideBasicBlock(0),
          isThreadAwake(false) {}
};

/** @brief Check trace_reader.hpp documentation for details */
class SinucaTraceReader : public TraceReader {
  private:
    ThreadData** threadDataArr;
    StaticTraceReader* staticTrace;
    StaticInstructionInfo** instructionDict;
    StaticInstructionInfo* instructionPool;
    int* basicBlockSizeArr; /**<Number of instructions per basic block. */
    unsigned long totalBasicBlocks; /**<Number of basic blocks in static file.*/
    int totalStaticInst;
    int totalThreads;

    Lock globalLock;
    Barrier globalBarrier;
    std::vector<Lock> privateLockVec;
    int numberOfActiveThreads;

    /**
     * @brief Fill instructions dictionary.
     * @return 1 on failure, 0 otherwise.
     */
    int GenerateInstructionDict();
    int FetchBasicBlock(int tid);
    int FetchMemoryData(InstructionPacket* ret, int tid);
    bool HasExecutionEnded();

    inline void SetNewLock(Lock* lock) {
        lock->waitingThreadsQueue.Allocate(0, sizeof(int));
        lock->isBusy = false;
        lock->recCont = 1;
        lock->addr = 0;
    }
    inline void ResetLock(Lock* lock) {
        lock->isBusy = false;
        lock->recCont = 1;
    }
    inline void SetNewBarrier(Barrier* barrier) { barrier->thrCont = 0; }
    inline void ResetBarrier(Barrier* barrier) { barrier->thrCont = 0; }
    inline bool IsThreadSleeping(int tid) {
        return (this->threadDataArr[tid]->isThreadAwake == false);
    }

  public:
    inline SinucaTraceReader()
        : staticTrace(0),
          instructionDict(0),
          instructionPool(0),
          basicBlockSizeArr(0),
          totalBasicBlocks(0),
          totalThreads(0),
          numberOfActiveThreads(1) {}
    virtual inline ~SinucaTraceReader() {
        for (int i = 0; i < this->totalThreads; ++i) {
            if (this->threadDataArr[i]) {
                delete this->threadDataArr[i];
            }
        }
        delete[] this->instructionDict;
        delete[] this->instructionPool;
        delete[] this->basicBlockSizeArr;
        delete this->staticTrace;
    }

    virtual FetchResult Fetch(InstructionPacket* ret, int tid);
    virtual int OpenTrace(const char* imageName, const char* sourceDir);
    virtual void PrintStatistics();

    virtual unsigned long GetNumberOfFetchedInst(int tid) {
        return this->threadDataArr[tid]->fetchedInst;
    }
    virtual unsigned long GetTotalInstToBeFetched(int tid) {
        return this->threadDataArr[tid]->dynFile.GetTotalExecutedInstructions();
    }

    virtual inline int GetTotalThreads() { return this->totalThreads; }
    virtual inline int GetTotalBasicBlocks() { return this->totalBasicBlocks; }
};

#ifndef NDEBUG
int TestTraceReader();
#endif

#endif  // SINUCA3_SINUCA_TRACER_TRACE_READER_HPP_
