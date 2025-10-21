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
 * @details .
 */

#include <cstdlib>
#include <tracer/sinuca/file_handler.hpp>

#include "pin.H"

/** @brief Check static_trace_writer.hpp documentation for details */
class StaticTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    StaticTraceRecord* basicBlock;
    int basicBlockArraySize;
    int basicBlockOccupation;
    int currentBasicBlockSize;

    inline void ResetBasicBlock() {
        this->basicBlockOccupation = 1;
        this->currentBasicBlockSize = -1;
    }

    int FlushBasicBlock();
    int ReallocBasicBlock();
    int TranslatePinInst(Instruction* inst, const INS* pinInst);

  public:
    inline StaticTraceWriter()
        : file(0),
          basicBlock(0),
          basicBlockArraySize(0) {
        this->header.fileType = FileTypeStaticTrace;
        this->ResetBasicBlock();
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

    int OpenFile(const char* sourceDir, const char* imageName);
    int AddInstruction(const INS* pinInst);
    int AddBasicBlockSize(unsigned int basicBlockSize);

    inline int IsBasicBlockReadyToFlush() {
        return (this->currentBasicBlockSize == this->basicBlockOccupation - 1);
    }
    inline int WasBasicBlockReset() {
        return (this->currentBasicBlockSize == -1);
    }
    inline int IsBasicBlockArrayFull() {
        return (this->basicBlockOccupation >= this->basicBlockArraySize);
    }
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
};

#endif
