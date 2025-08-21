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

const int MAX_INT_DIGITS = 7;

FILE *sinucaTracer::TraceFileReader::UseFile(const char *path) {
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

FILE *sinucaTracer::TraceFileWriter::UseFile(const char *path) {
    char mode[] = "wb";
    this->tf.file = fopen(path, mode);
    if (this->tf.file == NULL) {
        printFileErrorLog(path, mode);
        return NULL;
    }
    return this->tf.file;
}

unsigned long sinucaTracer::TraceFileReader::RetrieveLenBytes(
    void *ptr, unsigned long len) {
    unsigned long read = fread(ptr, 1, len, this->tf.file);
    return read;
}

int sinucaTracer::TraceFileReader::SetBufActiveSize(unsigned long size) {
    if (size > BUFFER_SIZE) return 1;
    this->bufActiveSize = size;
    return 0;
}

void sinucaTracer::TraceFileReader::RetrieveBuffer() {
    unsigned long read =
        this->RetrieveLenBytes(this->tf.buf, this->bufActiveSize);
    if (read < this->bufActiveSize) {
        this->eofLocation = read;
        this->eofFound = true;
    }
    this->tf.offsetInBytes = 0;
}

void *sinucaTracer::TraceFileReader::GetData(unsigned long len) {
    void *ptr = (void *)(this->tf.buf + this->tf.offsetInBytes);
    this->tf.offsetInBytes += len;
    return ptr;
}

int sinucaTracer::TraceFileWriter::AppendToBuffer(void *ptr,
                                                  unsigned long len) {
    if (BUFFER_SIZE - this->tf.offsetInBytes < len) {
        return 1;
    }
    memcpy(this->tf.buf + this->tf.offsetInBytes, ptr, len);
    this->tf.offsetInBytes += len;
    return 0;
}

void sinucaTracer::TraceFileWriter::FlushLenBytes(void *ptr,
                                                  unsigned long len) {
    __attribute__((unused)) unsigned long written;
    written = fwrite(ptr, 1, len, this->tf.file);
    assert(written == len && "fwrite error");
}

void sinucaTracer::TraceFileWriter::FlushBuffer() {
    this->FlushLenBytes(this->tf.buf, this->tf.offsetInBytes);
    this->tf.offsetInBytes = 0;
}

unsigned long sinucaTracer::GetPathTidInSize(const char *sourceDir,
                                             const char *prefix,
                                             const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    // 10 is the number of characters on the format string
    return MAX_INT_DIGITS + 10 + sourceDirLen + prefixLen + imageNameLen;
}

void sinucaTracer::FormatPathTidIn(char *dest, const char *sourceDir,
                                   const char *prefix, const char *imageName,
                                   THREADID tid, long destSize) {
    snprintf(dest, destSize, "%s/%s_%s_tid%u.trace", sourceDir, prefix,
             imageName, tid);
}

unsigned long sinucaTracer::GetPathTidOutSize(const char *sourceDir,
                                              const char *prefix,
                                              const char *imageName) {
    unsigned long sourceDirLen = strlen(sourceDir);
    unsigned long prefixLen = strlen(prefix);
    unsigned long imageNameLen = strlen(imageName);
    /* 9 is the number characters in the format string */
    return 9 + sourceDirLen + prefixLen + imageNameLen;
}

void sinucaTracer::FormatPathTidOut(char *dest, const char *sourceDir,
                                    const char *prefix, const char *imageName,
                                    long destSize) {
    snprintf(dest, destSize, "%s/%s_%s.trace", sourceDir, prefix, imageName);
}
