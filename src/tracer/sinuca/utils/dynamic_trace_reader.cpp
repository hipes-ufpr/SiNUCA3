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
 * @file x86_reader_file_handler.cpp
 * @brief Implementation of the x86_64 trace reader.
 */

#include "dynamic_trace_reader.hpp"

#include <cstdio>
#include <cstring>
#include <sinuca3.hpp>

extern "C" {
#include <alloca.h>
}

sinucaTracer::DynamicTraceFile::DynamicTraceFile(const char *folderPath,
                                                 const char *imageName,
                                                 THREADID tid) {
    unsigned long bufferSize =
        GetPathTidInSize(folderPath, "dynamic", imageName);
    char *path = (char *)alloca(bufferSize);

    FormatPathTidIn(path, folderPath, "dynamic", imageName, tid, bufferSize);
    if (this->UseFile(path) == NULL) {
        this->isValid = false;
        return;
    }

    /*
     * The number of executed instructions is placed at the top of the dynamic
     * file.
     */
    fread(&this->totalExecInst, sizeof(this->totalExecInst), 1, this->tf.file);
    SINUCA3_DEBUG_PRINTF("totalExecInst [%lu]\n", this->totalExecInst);

    this->bufActiveSize =
        (unsigned int)(BUFFER_SIZE / sizeof(BBLID)) * sizeof(BBLID);
    this->RetrieveBuffer(); /* First buffer read */
    this->isValid = true;
}

int sinucaTracer::DynamicTraceFile::ReadNextBBl(BBLID *bbl) {
    if (this->eofFound && this->tf.offsetInBytes == this->eofLocation) {
        return 1;
    }
    if (this->tf.offsetInBytes >= this->bufActiveSize) {
        this->RetrieveBuffer();
    }
    *bbl = *(BBLID *)(this->GetData(sizeof(BBLID)));

    return 0;
}
