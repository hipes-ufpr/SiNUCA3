#ifndef SINUCA3_STATIC_TRACE_READER_HPP_
#define SINUCA3_STATIC_TRACE_READER_HPP_

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
 * @file static_trace_reader.hpp
 * @brief Class implementation of the static trace reader.
 * @details The x86 based static trace is a binary file having the number
 * of instructions and the instructions themselves of each basic block.
 *
 * "In compiler construction, a basic block is a straight-line code sequence
 * with no branches in except to the entry and no branches out except at the
 * exit" - Wikipedia "Basic block".
 *
 * The definition above guarantees that when entering a basic block,
 * the program should execute all of its instructions, except in case of
 * interruption. Therefore, the flow of execution can be recorded only by
 * taking note of the indices of the basis blocks executed and storing each
 * basic block once in the static trace. This approach allows for a smaller
 * dynamic trace.
 *
 * The StaticTraceFile does not use a buffer, unlike the classes that manipulate
 * the Dynamic and Memory files, and uses mmap for file manipulation instead.
 * This is possible because the static trace tends to be small and mmap may be
 * slightly faster for the matter (it's just the dev trying to justify why he
 * did it this way).
 */

#include "../x86_file_handler.hpp"
#include "sinuca3.hpp"

namespace tracer {

class StaticTraceFile {
  private:
    bool isValid; /**<False if constructor fails to build object. */

    unsigned int totalThreads;
    unsigned int totalBBLs;     /**<Number of basic blocks in static trace. */
    unsigned int totalIns;      /**<Number of instructions in static trace. */
    unsigned int instLeftInBBL; /**<Used to detect when GetNewBBlSize was called
                                    earlier than expected. */

    char *mmapPtr;
    unsigned long mmapOffset;
    unsigned long mmapSize;
    int fd; /**<File descriptor .*/

    /**
     * @brief Returns pointer to generic data and updates mmapOffset value.
     * @param len Number of bytes read.
     */
    void *GetData(unsigned long len);

    void GetBooleanValues(StaticInstructionInfo *instInfo, DataINS *data);
    void GetBranchType(StaticInstructionInfo *instInfo, DataINS *data);
    void GetRegisters(StaticInstructionInfo *instInfo, DataINS *data);

  public:
    StaticTraceFile(const char *folderPath, const char *img);
    /**
     * @brief Fills struct with static info about the instruction.
     */
    void ReadNextInstruction(InstructionInfo *instInfo);
    /**
     * @brief Retrieves number of instructions of new basic block.
     * @return 1 on failure, 0 otherwise.
     */
    int GetNewBBLSize(unsigned int *size);

    inline unsigned int GetTotalBBLs() { return this->totalBBLs; }
    inline unsigned int GetTotalIns() { return this->totalIns; }
    inline unsigned int GetNumThreads() { return this->totalThreads; }
    inline bool Valid() { return this->isValid; }

    ~StaticTraceFile();
};

}  // namespace tracer

#endif