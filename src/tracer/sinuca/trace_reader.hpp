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
 * @file x86_trace_reader.hpp
 * @brief Implementation of SinucaTraceReader for x86 based traces.
 */

#include <tracer/sinuca/utils/dynamic_trace_reader.hpp>
#include <tracer/sinuca/utils/memory_trace_reader.hpp>
#include <tracer/sinuca/utils/static_trace_reader.hpp>
#include <tracer/trace_reader.hpp>

namespace sinucaTracer {

/**
 * @details This struct is only used internally by the x86 reader. It turns out
 * that more than one instance of the same instruction might be in the processor
 * pipeline at once. Since the number of memory loads and stores might change
 * between them, these values are not kept in the StaticInstructionInfo. When
 * the number of memory operations is indeed fixed, they are written to the
 * numStdMemLoadOps and numStdMemStoreOps variables.
 */
struct TraceReaderInstruction {
    unsigned short numStdMemLoadOps;
    unsigned short numStdMemStoreOps;
    StaticInstructionInfo staticInfo;
};

struct ThrInfo {
    DynamicTraceFile* dynFile;
    MemoryTraceFile* memFile;
    unsigned long currentBBL;    /**<Index of basic block. */
    unsigned long currentOpcode; /**<Index of instruction inside basic block. */
    unsigned long fetchedInst;   /**<Number of instructions fetched */
    bool isInsideBBL;
    int Allocate(const char *sourceDir, const char *imageName, int tid);

    inline ThrInfo() : dynFile(NULL), memFile(NULL) {
        this->isInsideBBL = false;
        this->fetchedInst = 0;
    }
    inline void ReadDynamicFileHeader() {
        this->dynFile->ReadHeaderFromFile();
    }
    inline ~ThrInfo() {
        if (this->memFile != NULL) delete this->memFile;
        if (this->dynFile != NULL) delete this->dynFile;
    }
};

class SinucaTraceReader : public TraceReader {
  private:
    ThrInfo* thrsInfo; /**<Information specific to each thread. */

    int totalThreads;

    unsigned long binaryTotalBBLs; /**<Number of basic blocks in static file. */
    unsigned int* binaryBBLsSize; /**<Number of instructions per basic block. */
    InstructionInfo** binaryDict; /**<Array containing all instructions. */
    InstructionInfo* pool;        /**<Pool used for more efficient allocation.*/

    /**
     * @brief Fill instructions dictionary
     * @details Info per instruction:
     * Address        | Ins. Size   | Base Reg    | Index Reg       |
     * Is predicated  | Is prefetch | Is indirect | Is !std mem op  |
     * Is read        | Has read2   | Is write    | Num. Write Regs |
     * Num. Read Regs | Read Regs   | Write Regs  | Ins. Mnemonic   |
     * Branch Type
     * @return 1 on failure, 0 otherwise.
     */
    int GenerateBinaryDict(StaticTraceFile *stFile);
    int CopyMemoryOperations(InstructionPacket *instPkt,
                             InstructionInfo *instInfo, ThrInfo *thrInfo);

  public:
    virtual int OpenTrace(const char *imageName, const char *sourceDir);
    virtual void PrintStatistics();
    virtual int GetTotalThreads();
    virtual unsigned long GetTotalBBLs();
    virtual unsigned long GetTotalInstToBeFetched(int tid);
    virtual unsigned long GetNumberOfFetchedInst(int tid);
    virtual FetchResult Fetch(InstructionPacket *ret, int tid);
    virtual void CloseTrace();

    inline ~SinucaTraceReader() { this->CloseTrace(); }
};

}  // namespace sinucaTracer

#ifndef NDEBUG
int TestTraceReader();
#endif

#endif  // SINUCA3_SINUCA_TRACER_TRACE_READER_HPP_
