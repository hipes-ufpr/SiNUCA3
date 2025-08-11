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
 * @file x86_file_handler.cpp
 * @details Implementation of the file handler, a helper class for handling
 * trace files.
 */

#include "x86_file_handler.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

extern "C" {
#include <errno.h>
}

inline void printFileErrorLog(const char *path, const char *mode) {
    SINUCA3_ERROR_PRINTF("Could not open [%s] in [%s] mode: %s\n", path, mode,
                         strerror(errno));
}

FILE *tracer::TraceFileReader::UseFile(const char *path) {
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

unsigned long tracer::TraceFileReader::RetrieveLenBytes(void *ptr,
                                                       unsigned long len) {
    unsigned long read = fread(ptr, 1, len, this->tf.file);
    return read;
}

int tracer::TraceFileReader::SetBufActiveSize(unsigned long size) {
    if (size > BUFFER_SIZE) {
        return 1;
    }
    this->bufActiveSize = size;
    return 0;
}

void tracer::TraceFileReader::RetrieveBuffer() {
    unsigned long read =
        this->RetrieveLenBytes(this->tf.buf, this->bufActiveSize);
    if (read < this->bufActiveSize) {
        this->eofLocation = read;
        this->eofFound = true;
    }
    this->tf.offsetInBytes = 0;
}

void *tracer::TraceFileReader::GetData(unsigned long len) {
    void *ptr = (void *)(this->tf.buf + this->tf.offsetInBytes);
    this->tf.offsetInBytes += len;
    return ptr;
}

FILE *tracer::TraceFileWriter::UseFile(const char *path) {
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
int tracer::TraceFileWriter::AppendToBuffer(void *ptr, unsigned long len) {
    if (BUFFER_SIZE - this->tf.offsetInBytes < len) {
        return 1;
    }
    memcpy(this->tf.buf + this->tf.offsetInBytes, ptr, len);
    this->tf.offsetInBytes += len;
    return 0;
}

void tracer::TraceFileWriter::FlushLenBytes(void *ptr, unsigned long len) {
    SINUCA3_DEBUG_PRINTF("len size [FlushLenBytes] [%lu]\n", len);
    unsigned long written = fwrite(ptr, 1, len, this->tf.file);
    SINUCA3_DEBUG_PRINTF("written size [FlushLenBytes] [%lu]\n", written);
    if (written != len) {
        SINUCA3_ERROR_PRINTF("fwrite returned something wrong: %s\n",
                             strerror(errno));
        assert(false && "fwrite error");
    }
}

void tracer::TraceFileWriter::FlushBuffer() {
    this->FlushLenBytes(this->tf.buf, this->tf.offsetInBytes);
    this->tf.offsetInBytes = 0;
}

unsigned long tracer::GetPathTidInSize(const char *sourceDir, const char *prefix,
                                      const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    /*
     * 7 is the number of digits on MAX_INT
     * 13 is the number of characters on the format string
     */
    return 13 + 7 + sourceDirLen + prefixLen + imageNameLen;
}

void tracer::FormatPathTidIn(char *dest, const char *sourceDir,
                            const char *prefix, const char *imageName,
                            THREADID tid, unsigned long bufferSize) {
    snprintf(dest, bufferSize, "%s/%s_%s_tid%u.trace", sourceDir, prefix,
             imageName, tid);
}

unsigned long tracer::GetPathTidOutSize(const char *sourceDir,
                                       const char *prefix,
                                       const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    // 9 == number characters on the format string.
    return 9 + sourceDirLen + prefixLen + imageNameLen;
}

void tracer::FormatPathTidOut(char *dest, const char *sourceDir,
                             const char *prefix, const char *imageName,
                             unsigned long bufferSize) {
    snprintf(dest, bufferSize, "%s/%s_%s.trace", sourceDir, prefix, imageName);
}
