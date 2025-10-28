#ifndef SINUCA3_SINUCA_TRACER_DYNAMIC_TRACE_READER_HPP_
#define SINUCA3_SINUCA_TRACER_DYNAMIC_TRACE_READER_HPP_

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
 * @file dynamic_trace_reader.hpp
 * @brief Class implementation of the dynamic trace reader.
 * @details .
 */

#include <cstdio>
#include <tracer/sinuca/file_handler.hpp>

#include "utils/logging.hpp"

typedef int THREADID;

enum ThreadState {
    ThreadStateUndefined,
    ThreadStateSleeping,
    ThreadStateActive
};

struct ThreadLock {
    bool busy;
};

struct ThreadBarrier {
    int cont;
};

/** @brief Check dynamic_trace_reader.hpp documentation for details */
class DynamicTraceReader {
  private:
    FILE* file;
    FileHeader header;
    DynamicTraceRecord recordArray[RECORD_ARRAY_SIZE];
    int recordArraySize;
    int recordArrayIndex;
    bool reachedEnd;

    bool isThreadAwake;
    bool isThreadStateUndefined;
    bool createdThreads;
    bool destroyedThreads;
    int idOfChildThread;

    int LoadRecordArray();

  public:
    inline DynamicTraceReader()
        : file(0), recordArraySize(0), recordArrayIndex(0), reachedEnd(0) {}
    inline ~DynamicTraceReader() {
        if (file) {
            fclose(this->file);
        }
        if (this->recordArrayIndex != this->recordArraySize) {
            SINUCA3_WARNING_PRINTF(
                "Basic block ids may have been left unread\n");
        }
    }

    int OpenFile(const char* sourceDir, const char* imageName, int tid);
    ThreadState ReadDynamicRecord();

    inline bool ReachedDynamicTraceEnd() {
        return (this->reachedEnd &&
                this->recordArrayIndex == this->recordArraySize);
    }
    inline int GetRecordType() {
        return this->recordArray[this->recordArrayIndex].recordType;
    }
    inline unsigned long GetBasicBlockIdentifier() {
        return this->recordArray[this->recordArrayIndex]
            .data.bbl.basicBlockIdentifier;
    }
    inline unsigned long GetTotalExecutedInstructions() {
        return this->header.data.dynamicHeader.totalExecutedInstructions;
    }
    inline void SetThreadStateUndefined() {
        this->isThreadStateUndefined = true;
    }
    inline int GetIdOfChildThread() {
        return this->idOfChildThread;
    }
    inline bool CheckThreadCreation() {
        if (this->createdThreads) {
            this->createdThreads = false;
            return true;
        }
        return false;
    }
    inline bool CheckThreadDestruction() {
        if (this->destroyedThreads) {
            this->destroyedThreads = false;
            return true;
        }
        return false;
    }
};

#endif
