#ifndef SINUCA3_SINUCA_TRACER_STATIC_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_STATIC_TRACE_READER_HPP_

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
 * slightly faster for the matter.
 */

#include <sinuca3.hpp>
#include <tracer/sinuca/file_handler.hpp>

extern "C" {
#include <sys/mman.h>
#include <unistd.h>
}

/** @brief Check static_trace_reader.hpp documentation for details */
class StaticTraceReader {
  private:
    FileHeader header;
    StaticTraceRecord* record;
    unsigned long mmapOffset;
    unsigned long mmapSize;
    int fileDescriptor;
    char *mmapPtr;

    /**
     * @brief Returns pointer to generic data and updates mmapOffset value.
     * @param len Number of bytes read.
     */
    void *ReadData(unsigned long len);

  public:
    inline StaticTraceReader()
        : mmapOffset(0), mmapSize(0), fileDescriptor(0), mmapPtr(0) {};
    inline ~StaticTraceReader() {
        if (this->mmapPtr == NULL) return;
        munmap(this->mmapPtr, this->mmapSize);
        if (this->fileDescriptor == -1) return;
        close(this->fileDescriptor);
    }

    /**
     * @return 1 on failure, 0 otherwise.
     */
    int OpenFile(const char *folderPath, const char *img);
    int ReadStaticRecordFromFile();
    void GetInstruction(StaticInstructionInfo* instInfo);

    inline unsigned char GetStaticRecordType() {
        return this->record->recordType;
    }
    inline int GetBasicBlockSize() {
        return this->record->data.basicBlockSize;
    }
    inline unsigned long GetTotalBasicBlocks() {
        return this->header.data.staticHeader.bblCount;
    }
    inline unsigned long GetTotalInstInStaticTrace() {
        return this->header.data.staticHeader.instCount;
    }
    inline unsigned int GetNumThreads() {
        return this->header.data.staticHeader.threadCount;
    }
};

#endif
