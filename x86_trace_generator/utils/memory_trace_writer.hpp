#ifndef SINUCA3_GENERATOR_MEMORY_FILE_HPP_
#define SINUCA3_GENERATOR_MEMORY_FILE_HPP_

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
 * @file memory_trace_writer.hpp
 * @details A memory trace file stacks the memory operations. Each instruction
 * may perform a variable number of accesses to the main memory, therefore
 * this information must be stored somewhere. Since it may change dynamically,
 * it is not suitable to be in the static trace, which means that the trace
 * reader expects a MemoryTraceRecord with the number of operations performed by
 * the instruction being fetched before the operations per se. Each access is
 * stored in an individual MemoryTraceRecord. Note that the trace reader knowns
 * if an instruction accesses the main memory, either writing or reading from
 * it, because this is saved in the static trace.
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

/** @brief Check memory_trace_writer.hpp documentation for details */
class MemoryTraceWriter {
  private:
    FILE* file;
    FileHeader header;
    MemoryTraceRecord recordArray[RECORD_ARRAY_SIZE]; /**<Record buffer. */
    int recordArrayOccupation; /**<Number of records currently stored. */

    inline void ResetRecordArray() { this->recordArrayOccupation = 0; }
    inline int IsRecordArrayEmpty() {
        return (this->recordArrayOccupation <= 0);
    }
    inline int IsRecordArrayFull() {
        return (this->recordArrayOccupation == RECORD_ARRAY_SIZE);
    }

    int FlushRecordArray();
    int CheckRecordArray();
    int AddMemoryRecord(MemoryTraceRecord record);

  public:
    inline MemoryTraceWriter() : file(0), recordArrayOccupation(0) {
        this->header.SetHeaderType(FileTypeMemoryTrace);
    };
    inline ~MemoryTraceWriter() {
        if (!this->IsRecordArrayEmpty()) {
            if (this->FlushRecordArray()) {
                SINUCA3_ERROR_PRINTF("Failed to flush memory records!\n");
            }
        }
        if (this->header.FlushHeader(this->file)) {
            SINUCA3_ERROR_PRINTF("Failed to write memory file header!\n");
        }
        if (file) {
            fclose(file);
        }
    }

    /** @brief Create the [tid] memory file in the [sourceDir] directory. */
    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    /** @brief Add the number of memory operations. */
    int AddNumberOfMemOperations(unsigned int memoryOperations);
    /**
     * @brief Add a memory operation.
     * @param address Is the virtual address accessed.
     * @param size Number of bytes accessed.
     * @param isLoad True if it is a load op, false if it is a store op.
     */
    int AddMemOp(unsigned long address, unsigned int size, bool isLoadOp);

    inline void SetTargetArch(unsigned char target) {
        this->header.targetArch = target;
    }
};

#endif
