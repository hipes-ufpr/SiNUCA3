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
 * @file file_handler.cpp
 * @details Implementation of the file handler, a helper class for handling
 * trace files.
 */

#include "file_handler.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" {
#include <errno.h>
}

#include "../utils/logging.hpp"

inline void printFileErrorLog(const char *path, const char *mode) {
    SINUCA3_ERROR_PRINTF("Could not open [%s] in [%s] mode: %s\n", path, mode,
                         strerror(errno));
}

FILE *trace::TraceFileReader::UseFile(const char *path) {
    char mode[] = "rb";
    this->tf.file = fopen(path, mode);
    if (this->tf.file == NULL) {
        printFileErrorLog(path, mode);
        return NULL;
    }
    this->eofLocation = 0;
    this->eofFound = false;
    return this->tf.file;
}

unsigned long trace::TraceFileReader::RetrieveLenBytes(void *ptr,
                                                       unsigned long len) {
    unsigned long read = fread(ptr, 1, len, this->tf.file);
    return read;
}

int trace::TraceFileReader::SetBufActiveSize(unsigned long size) {
    if (size > BUFFER_SIZE) {
        return 1;
    }
    this->bufActiveSize = size;
    return 0;
}

void trace::TraceFileReader::RetrieveBuffer() {
    unsigned long read =
        this->RetrieveLenBytes(this->tf.buf, this->bufActiveSize);
    if (read < this->bufActiveSize) {
        this->eofLocation = read;
        this->eofFound = true;
    }
    this->tf.offset = 0;
}

void *trace::TraceFileReader::GetData(unsigned long len) {
    void *ptr = (void *)(this->tf.buf + this->tf.offset);
    this->tf.offset += len;
    return ptr;
}

FILE *trace::TraceFileWriter::UseFile(const char *path) {
    char mode[] = "wb";
    this->tf.file = fopen(path, mode);
    if (this->tf.file == NULL) {
        printFileErrorLog(path, mode);
        return NULL;
    }
    return this->tf.file;
}

/*
 * flush is not done here because derived class might flush buffer size to file
 * in addition to buffer
 */
int trace::TraceFileWriter::AppendToBuffer(void *ptr, unsigned long len) {
    if (BUFFER_SIZE - this->tf.offset < len) {
        return 1;
    }
    memcpy(this->tf.buf + this->tf.offset, ptr, len);
    this->tf.offset += len;
    return 0;
}

void trace::TraceFileWriter::FlushLenBytes(void *ptr, unsigned long len) {
    SINUCA3_DEBUG_PRINTF("len size [FlushLenBytes] [%lu]\n", len);
    unsigned long written = fwrite(ptr, 1, len, this->tf.file);
    SINUCA3_DEBUG_PRINTF("written size [FlushLenBytes] [%lu]\n", written);
    if (written != len) {
        SINUCA3_ERROR_PRINTF("fwrite returned something wrong: %s\n",
                             strerror(errno));
        assert(false && "fwrite error");
    }
}

void trace::TraceFileWriter::FlushBuffer() {
    this->FlushLenBytes(this->tf.buf, this->tf.offset);
    this->tf.offset = 0;
}

unsigned long trace::GetPathTidInSize(const char *sourceDir, const char *prefix,
                                      const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    // 7 == number of digits on MAX_INT.
    // 13 == number of characters on the format string.
    return 13 + 7 + sourceDirLen + prefixLen + imageNameLen;
}

void trace::FormatPathTidIn(char *dest, const char *sourceDir,
                            const char *prefix, const char *imageName,
                            THREADID tid, unsigned long bufferSize) {
    snprintf(dest, bufferSize, "%s/%s_%s_tid%u.trace", sourceDir, prefix,
             imageName, tid);
}

unsigned long trace::GetPathTidOutSize(const char *sourceDir,
                                       const char *prefix,
                                       const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    // 9 == number characters on the format string.
    return 9 + sourceDirLen + prefixLen + imageNameLen;
}

void trace::FormatPathTidOut(char *dest, const char *sourceDir,
                             const char *prefix, const char *imageName,
                             unsigned long bufferSize) {
    snprintf(dest, bufferSize, "%s/%s_%s.trace", sourceDir, prefix, imageName);
}
