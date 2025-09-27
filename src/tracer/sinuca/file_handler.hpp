#ifndef SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_
#define SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_

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

#include <cstring>
#include <sinuca3.hpp>
#include "engine/default_packets.hpp"

extern "C" {
#include <errno.h>
}

namespace sinucaTracer {

typedef unsigned int THREADID;

const int CACHE_LINE_SIZE = 64;  // Used in alignas to avoid false sharing
const int MAX_INSTRUCTION_NAME_LENGTH = 32;
const int MAX_ROUTINE_NAME_LENGTH = 16;
const unsigned long MAX_IMAGE_NAME_SIZE = 255;
const unsigned long MAX_REG_OPERANDS = 8;

const short NON_STD_HEADER_TYPE = 1;
const short MEM_OPERATION_TYPE = 2;
const short MEM_READ_TYPE = 3;
const short MEM_WRITE_TYPE = 4;
const short BBL_IDENTIFIER_TYPE = 5;
const short RTN_NAME_TYPE = 6;
const short BBL_SIZE_TYPE = 7;
const short INSTRUCTION_TYPE = 8;

const unsigned char BRANCH_CALL = 1;
const unsigned char BRANCH_COND = 2;
const unsigned char BRANCH_UNCOND = 3;
const unsigned char BRANCH_SYSCALL = 4;
const unsigned char BRANCH_RETURN = 5;

/** @brief Written to static trace file. Condensed instruction info. */
struct Instruction {
    char name[MAX_INSTRUCTION_NAME_LENGTH];
    unsigned short int readRegs[MAX_REG_OPERANDS];
    unsigned short int writeRegs[MAX_REG_OPERANDS];
    unsigned long addr;
    unsigned short int baseReg;
    unsigned short int indexReg;
    unsigned char size;
    unsigned char numReadRegs;
    unsigned char numWriteRegs;
    unsigned char numStdMemReadOps;
    unsigned char numStdMemWriteOps;
    unsigned char branchType;
    unsigned char isPredicated : 1;
    unsigned char isPrefetch : 1;
    unsigned char isControlFlow : 1;
    unsigned char isIndirectControlFlow : 1;
    unsigned char isNonStandardMemOp : 1;
} __attribute__((packed));

/** @brief Written to static trace file. */
struct StaticRecord {
    short recordType;
    union {
        unsigned int basicBlockSize;
        struct Instruction instruction;
    } data;
} __attribute__((packed));

/** @brief Written to dynamic trace file. */
struct ExecutionRecord {
    short recordType;
    union {
        unsigned long basicBlockIdentifier;
        char routineName[MAX_ROUTINE_NAME_LENGTH];
    } data;
} __attribute__((packed));

/** @brief Written to memory trace file. */
struct MemoryRecord {
    short recordType;
    union {
        struct {
            unsigned long addr; /**<Virtual address accessed. */
            unsigned int size;  /**<Size in bytes of memory read or written. */
            short type;
        } operation;
        struct {
            unsigned short nonStdReadOps;
            unsigned short nonStdWriteOps;
        } nonStdHeader;
    } data;
} __attribute__((packed));

/** @brief General usage. */
struct FileHeader {
    union {
        struct {
            unsigned int threadCount;
            unsigned long bblCount;
            unsigned long instCount;
        } staticHeader;
        struct {
            unsigned long totalExecutedInstructions;
        } dynamicHeader;
    } data;
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

}  // namespace sinucaTracer

#endif
