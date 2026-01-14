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
 * @details This file defines the StaticTraceReader class which encapsulates
 * the static trace file and read operations. It is allows one to open a
 * static trace, read its header and the subsequent records stored. No dedicated
 * buffering is done because the file is already mapped to virtual memory.
 *
 */

#include <cstdlib>
#include <tracer/sinuca/file_handler.hpp>

extern "C" {
#include <sys/mman.h>
#include <unistd.h>
}

/** @brief Check static_trace_reader.hpp documentation for details */
class StaticTraceReader {
  private:
    FileHeader header;
    StaticTraceRecord* record; /**<Aux pointer to current record. */
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
        if (this->mmapPtr) {
            munmap(this->mmapPtr, this->mmapSize);
        }
        if (this->fileDescriptor != -1) {
            close(this->fileDescriptor);
        }
    }

    /**
     * @return 1 on failure, 0 otherwise.
     */
    int OpenFile(const char *folderPath, const char *img);
    int ReadStaticRecordFromFile();
    void TranslateRawInstructionToSinucaInst(StaticInstructionInfo* instInfo);

    inline StaticTraceRecordType GetStaticRecordType() {
        return (StaticTraceRecordType)this->record->recordType;
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
    inline unsigned int GetVersionInt() {
        return this->header.traceVersion;
    }
    inline unsigned int GetTargetInt() {
        return this->header.targetArch;
    }
    inline const char* GetTargetString() {
        if (this->header.targetArch == TargetArchX86) {
            return TRACE_TARGET_X86;
        }
        if (this->header.targetArch == TargetArchARM) {
            return TRACE_TARGET_ARM;
        }
        if (this->header.targetArch == TargetArchRISCV) {
            return TRACE_TARGET_RISCV;
        }

        static const char UNKOWN[] = "UNKOWN TARGET ARCH!";
        return UNKOWN;
    }
};

#endif
