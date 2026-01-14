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
 * @details A static trace contains all the basic blocks 'touched' within
 * execution with its instructions. This way the dynamic trace has to store the
 * indexes of the basic blocks executed so that the execution may be simulated.
 * Remember that a basic block is a piece of code with a single entrance point
 * and a single exit. As the name suggests, all information regarding the
 * instruction that is not dynamic (e.g. the number of registers accessed) is
 * stored in the static file. The reader expects the number of instructions of
 * the basic block being read before the instructions per se, hence a method to
 * add a StaticTraceRecord with the size is implemented. The implementation
 * does not force the 'AddBasicBlockSize' and 'AddInstruction' to be called in a
 * certain order for things to work.
 */

#include <cstdlib>
#include <tracer/sinuca/file_handler.hpp>

/** @brief Check static_trace_writer.hpp documentation for details */
class StaticTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    StaticTraceRecord* basicBlock; /**<Current basic block. */
    int basicBlockArraySize;       /**<Current size of the buffer. */
    int basicBlockOccupation;
    int currentBasicBlockSize;     /**<Number of instructions in the bbl. */

    inline void ResetBasicBlock() {
        this->basicBlockOccupation = 1;
        this->currentBasicBlockSize = -1;
    }
    inline int IsBasicBlockReadyToBeFlushed() {
        return (this->currentBasicBlockSize == this->basicBlockOccupation - 1);
    }
    inline int WasBasicBlockReset() {
        return (this->currentBasicBlockSize == -1);
    }
    inline int IsBasicBlockArrayFull() {
        return (this->basicBlockOccupation >= this->basicBlockArraySize);
    }

    int FlushBasicBlock();
    int ReallocBasicBlock();
    int AddStaticRecord(StaticTraceRecord record, int pos);

  public:
    inline StaticTraceWriter()
        : file(0), basicBlock(0), basicBlockArraySize(128) {
        this->header.SetHeaderType(FileTypeStaticTrace);
        this->ResetBasicBlock();
        this->basicBlock = (StaticTraceRecord*)malloc(
            sizeof(StaticTraceRecord) * this->basicBlockArraySize);
    };
    inline ~StaticTraceWriter() {
        if (this->header.FlushHeader(this->file)) {
            SINUCA3_ERROR_PRINTF("Failed to write static header!\n")
        }
        if (!this->WasBasicBlockReset()) {
            SINUCA3_ERROR_PRINTF("Last basic block is incomplete!\n");
        }
        if (this->file) {
            fclose(this->file);
        }
        if (this->basicBlock) {
            free(this->basicBlock);
        }
    }

    /** @brief Create the static file in the [sourceDir] directory. */
    int OpenFile(const char* sourceDir, const char* imageName);
    /** @brief Add a formated instruction to the current basic block. */
    int AddInstruction(const Instruction* inst);
    /** @brief Add the number of instructions of the current basic block. The
     * last bbl is expected to be flushed when this method is called. */
    int AddBasicBlockSize(unsigned int basicBlockSize);

    inline void IncStaticInstructionCount() {
        this->header.data.staticHeader.instCount++;
    }
    inline void IncThreadCount() {
        this->header.data.staticHeader.threadCount++;
    }
    inline void IncBasicBlockCount() {
        this->header.data.staticHeader.bblCount++;
    }
    inline unsigned int GetBasicBlockCount() {
        return this->header.data.staticHeader.bblCount;
    }
    inline void SetTargetArch(unsigned char target) {
        this->header.targetArch = target;
    }
};

#endif
