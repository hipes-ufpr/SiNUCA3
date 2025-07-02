#ifndef SINUCA3_FILE_HANDLER_HPP_
#define SINUCA3_FILE_HANDLER_HPP_

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
 * @file file_handler.hpp
 * @details Public API of the file handler, a helper class for handling trace
 * files.
 */

#include <cstdio>  // FILE*

const int MAX_INSTRUCTION_NAME_LENGTH = 32;
// 1MiB
const unsigned long BUFFER_SIZE = 1 << 20;
// Used in alignas to avoid false sharing
const unsigned long CACHE_LINE_SIZE = 64;
// Adjust if needed
const unsigned long MAX_IMAGE_NAME_SIZE = 255;
// Used to standardize reading and writing
const unsigned long SIZE_NUM_MEM_R_W = sizeof(unsigned short);
// Used to standardize reading and writing
const unsigned long SIZE_NUM_BBL_INS = sizeof(unsigned int);
// Adjust if needed
const unsigned long MAX_REG_OPERANDS = 8;

// Not using enum cause it may vary in size depending on machine
const unsigned char BRANCH_CALL = 1;
const unsigned char BRANCH_COND = 2;
const unsigned char BRANCH_UNCOND = 3;
const unsigned char BRANCH_SYSCALL = 4;
const unsigned char BRANCH_RETURN = 5;

namespace trace {

typedef unsigned int BBLID;
typedef unsigned int THREADID;

struct DataINS {
    char name[MAX_INSTRUCTION_NAME_LENGTH];
    unsigned short int readRegs[MAX_REG_OPERANDS];
    unsigned short int writeRegs[MAX_REG_OPERANDS];
    unsigned long addr;
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
    unsigned char branchType;
    unsigned char isPredicated : 1;
    unsigned char isPrefetch : 1;
    unsigned char isControlFlow : 1;
    unsigned char isIndirectControlFlow : 1;
    unsigned char isNonStandardMemOp : 1;
    unsigned char isRead : 1;
    unsigned char isRead2 : 1;
    unsigned char isWrite : 1;
} __attribute__((packed));  // no padding

struct DataMEM {
    unsigned long addr;
    unsigned int size;
} __attribute__((packed));  // no padding

struct TraceFile {
    unsigned char *buf;
    FILE *file;
    unsigned long offset;  // in bytes

    inline TraceFile() : offset(0) {
        this->buf = new unsigned char[BUFFER_SIZE];
    }
    inline ~TraceFile() {
        delete[] this->buf;
        fclose(file);
    }
};

class TraceFileReader {
  protected:
    bool eofFound;
    unsigned long eofLocation;
    unsigned long bufActiveSize;
    TraceFile tf;

    FILE *UseFile(const char *path);
    unsigned long RetrieveLenBytes(void *, unsigned long);
    int SetBufActiveSize(unsigned long);
    void RetrieveBuffer();
    void *GetData(unsigned long);
};

class TraceFileWriter {
  protected:
    TraceFile tf;

    FILE *UseFile(const char *);
    int AppendToBuffer(void *, unsigned long);
    void FlushLenBytes(void *, unsigned long);
    void FlushBuffer();
};

unsigned long GetPathTidInSize(const char *sourceDir, const char *prefix,
                               const char *imageName);
void FormatPathTidIn(char *dest, const char *sourceDir, const char *prefix,
                     const char *imageName, THREADID tid,
                     unsigned long bufferSize);

unsigned long GetPathTidOutSize(const char *sourceDir, const char *prefix,
                                const char *imageName);
void FormatPathTidOut(char *dest, const char *sourceDir, const char *prefix,
                      const char *imageName, unsigned long bufferSize);

}  // namespace trace

#endif
