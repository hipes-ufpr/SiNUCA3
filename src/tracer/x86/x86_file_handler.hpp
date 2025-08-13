#ifndef SINUCA3_X86_FILE_HANDLER_HPP_
#define SINUCA3_X86_FILE_HANDLER_HPP_

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
 * @file x86_file_handler.hpp
 * @brief Common file handling API for x86 traces.
 * @details It gathers constants, classes and functions used by the trace
 * generator based on intel pin for x86 architecture and the corresponding
 * trace reader. It is appropriate to have such file, as the traces for x86
 * architecture are binary files which implies a deep dependency on how the
 * the reading is done in relation to how the information is stored in the
 * traces. Therefore, maintaining the TraceFileWriter and TraceFileReader
 * implementations together allows for a better undestanding of how they
 * coexist.
 */

#include <cstdio>
#include <cstring>

#include "sinuca3.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <errno.h>
}

namespace tracer {

typedef unsigned int BBLID;
typedef unsigned int THREADID;

const int CACHE_LINE_SIZE = 64;  // Used in alignas to avoid false sharing
const int MAX_INSTRUCTION_NAME_LENGTH = 32;
const unsigned long BUFFER_SIZE = 1 << 20;
const unsigned long MAX_IMAGE_NAME_SIZE = 255;
const unsigned long MAX_REG_OPERANDS = 8;
/*
 * In case of a non standard memory access, the numbers of read and write
 * operations are stored first, hence the following constant exist, and then
 * memory accesses itself. It is done that way to save storage space. It
 * represents the size in bytes designated in the file for the purpose of
 * storing these numbers.
 */
const unsigned long SIZE_NUM_MEM_R_W = sizeof(unsigned short);
/*
 * Before each new BBL (basic block) is stored in the static trace, the number
 * of instructions that make up the block is stored. Thus, the following
 * constant is the size in bytes designated to store it in the file.
 */
const unsigned long SIZE_NUM_BBL_INS = sizeof(unsigned int);

/*
 * One may propably think that an enumerator would be more appropriate here.
 * The issue is that the files are binary and there is no guarantee that an
 * enum will occupy the same size accross different machines, or it is less
 * probable at the very least, which may cause bugs while reading traces
 * generated elsewhere.
 */
const unsigned char BRANCH_CALL = 1;
const unsigned char BRANCH_COND = 2;
const unsigned char BRANCH_UNCOND = 3;
const unsigned char BRANCH_SYSCALL = 4;
const unsigned char BRANCH_RETURN = 5;

/**
 * @details DataINS is a packed struct since it is written directly to the
 * static trace (binary file). It is designed to take as little space as
 * possible while storing instruction data.
 */
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
} __attribute__((packed));

/**
 * @details DataMEM is a packed struct since it is written directly to the
 * memory trace (binary file). It is designed to take as little space as
 * possible while storing memory access data.
 */
struct DataMEM {
    unsigned long addr; /**<Virtual address accessed. */
    unsigned int size;  /**<Size in bytes of memory read or written. */
} __attribute__((packed));

/**
 * @details This struct is only used internally by the x86 reader. It turns out
 * that more than one instance of the same instruction might be in the processor
 * pipeline at once. Since the number of memory read and write accesses
 * might change between them if the instruction performs non standard memory
 * operations, these values are not kept in the staticInfo struct. As a
 * consequence, when the instruction is standard and the number of operations
 * is not dynamic, they are written to the staticNumReadings/Writings variables.
 */
struct InstructionInfo {
    unsigned short staticNumReadings;
    unsigned short staticNumWritings;
    StaticInstructionInfo staticInfo;
};

struct TraceFile {
    FILE *file;
    unsigned char *buf;
    unsigned long offsetInBytes;

    inline TraceFile() : offsetInBytes(0) {
        this->buf = new unsigned char[BUFFER_SIZE];
    }
    inline ~TraceFile() {
        delete[] this->buf;
        fclose(file);
    }
};

class TraceFileReader {
  protected:
    TraceFile tf;
    bool isValid; /**<False if UseFile fails>.*/
    bool eofFound;
    unsigned long eofLocation;   /**<Stores bytes left to end of file.*/
    unsigned long bufActiveSize; /**<Closest value to BUFFER_SIZE that is a
                                    multiple of the struct size (e.g. struct
                                    DataINS). The memory trace stores this value
                                    after every buffer write, an exception.*/

    /**
     * @brief Opens trace file and initializes attributes.
     * @param path Indicates the complete path to trace file.
     */
    FILE *UseFile(const char *path);
    /**
     * @brief Wrapper to fread.
     * @param ptr Where to write bytes read from file.
     * @param len Number of bytes to read from file.
     */
    unsigned long RetrieveLenBytes(void *ptr, unsigned long len);
    /**
     * @brief Used to modify bufActiveSize
     */
    int SetBufActiveSize(unsigned long size);
    /**
     * @brief Wrapper to RetrieveLenBytes having the buffer as pointer. It also
     * detects end of file and resets the offsetInBytes variable.
     */
    void RetrieveBuffer();
    /**
     * @brief Obtains data from the buffer
     * @param len Number of bytes to add to the buffer offset
     * @return Pointer to data (needs to be cast)
     */
    void *GetData(unsigned long len);

  public:
    inline bool Valid() { return this->isValid; }
};

class TraceFileWriter {
  protected:
    TraceFile tf;

    /**
     * @brief Opens trace file and initializes attributes.
     * @param path Indicates the complete path to trace file.
     */
    FILE *UseFile(const char *path);
    /**
     * @brief Appends data to the buffer.
     * @param ptr Pointer to data to be written.
     * @param len Number of bytes to copy into buffer.
     * @return 1 if there is not enough space in buffer, 0 otherwise.
     */
    int AppendToBuffer(void *ptr, unsigned long len);
    /**
     * @brief Wrapper to fwrite.
     * @param ptr Where to read bytes from to be written to file.
     * @param len Number of bytes to be written to file.
     */
    void FlushLenBytes(void *ptr, unsigned long len);
    /**
     * @brief Wrapper to FlushLenBytes having the buffer as pointer. It also
     * resets the offsetInBytes variable.
     */
    void FlushBuffer();
};

inline void printFileErrorLog(const char *path, const char *mode) {
    SINUCA3_ERROR_PRINTF("Could not open [%s] in [%s] mode: ", path, mode);
    SINUCA3_ERROR_PRINTF("%s\n", strerror(errno));
}

/**
 * @brief Get max size of the formatted path string that includes the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 */
unsigned long GetPathTidInSize(const char *sourceDir, const char *prefix,
                               const char *imageName);

/**
 * @brief Format the path in dest string including the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 * @param tid Thread identier
 * @param destSize Max capacity of dest string.
 */
void FormatPathTidIn(char *dest, const char *sourceDir, const char *prefix,
                     const char *imageName, THREADID tid, long destSize);

/**
 * @brief Get size of the formatted path string without the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'
 * @param imageName Name of the executable used to generate the traces.
 */
unsigned long GetPathTidOutSize(const char *sourceDir, const char *prefix,
                                const char *imageName);

/**
 * @brief Format the path in dest string without the thread id.
 * @param sourceDir Complete path to the directory that stores the traces.
 * @param prefix 'dynamic', 'memory' or 'static'.
 * @param imageName Name of the executable used to generate the traces.
 * @param destSize Max capacity of dest string.
 */
void FormatPathTidOut(char *dest, const char *sourceDir, const char *prefix,
                      const char *imageName, long destSize);

}  // namespace tracer

#endif
