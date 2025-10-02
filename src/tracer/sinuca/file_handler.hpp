#ifndef SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_
#define SINUCA3_SINUCA_TRACER_FILE_HANDLER_HPP_

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

#include <cstdint>
#include <cstring>
#include <sinuca3.hpp>
#include "engine/default_packets.hpp"

extern "C" {
#include <errno.h>
#include <stdint.h>
}

namespace sinucaTracer {

const int MAX_INSTRUCTION_NAME_LENGTH = 32;
const int MAX_IMAGE_NAME_SIZE = 255;
const int STDE_PAD = 7;

enum FileType : uint8_t {
    StaticTrace,
    DynamicTrace,
    MemoryTrace
};

enum StaticTraceDictionaryRecordType : uint8_t {
    StaticTraceDictionaryNewInstruction,
    StaticTraceDictionaryEndOfDictionary
};

enum StaticTraceBasicBlockRecordType : uint8_t {
    StaticTraceBasicBlockInstruction,
    StaticTraceBasicBlockSizeBlock
};

enum DynamicTraceRecordType : uint8_t {
    DynamicRecordBasicBlockIdentifier,
    DynamicRecordThreadEvent
};

enum ThreadEvent : uint8_t {
    ThreadEventCreateThread,
    ThreadEventDestroyThread,
    ThreadEventLockRequest,
    ThreadEventBarrier
};

enum MemoryRecordType : uint8_t {
    MemoryRecordNonStdHeader,
    MemoryRecordOperation
};

enum MemoryOperationType : uint8_t {
    MemoryOperationLoad,
    MemoryOperationStore
};

enum BranchType : uint8_t {
    BranchCall,
    BranchSyscall,
    BranchConditional,
    BranchUnconditional,
    BranchReturn,
    None
};

/** @brief Written to static trace file. */
struct StaticTraceDictionaryEntry {
    uint64_t instructionAddress;
    uint16_t loadRegisters[MAX_REGISTERS];
    uint16_t storeRegisters[MAX_REGISTERS];
    uint16_t baseRegister;
    uint16_t indexRegister;
    uint16_t instructionIdentifier;
    uint8_t numberLoadRegisters;
    uint8_t numberStoreRegisters;
    uint8_t instructionSize;
    uint8_t branchType;
    uint8_t isPredicated;
    uint8_t isPrefetch;
    uint8_t isControlFlow;
    uint8_t isIndirectControlFlow;
    uint8_t isNonStandardMemOp;
    uint8_t numStdMemLoadOps;  /**<Field ignored if non std.>*/
    uint8_t numStdMemStoreOps; /**<Field ignored if non std.>*/
    uint8_t padding[STDE_PAD]; /**<Manual padding. Reserved for future use.> */
    char instructionMnemonic[MAX_INSTRUCTION_NAME_LENGTH];
} __attribute__((packed));

/** @brief Written to static trace file. */
struct StaticTraceDictionaryRecord {
    uint8_t recordType;
    union {
        struct StaticTraceDictionaryEntry newInstruction;
    } data;
} __attribute__((packed));

/** @brief Written to static trace file. */
struct StaticTraceBasicBlockRecord {
    uint8_t recordType;
    union {
        uint16_t basicBlockSize;
        uint16_t instructionIdentifier;
    } data;
} __attribute__((packed));

/** @brief Written to dynamic trace file. */
struct DynamicTraceRecord {
    uint8_t recordType;
    union {
        struct {
            uint32_t basicBlockIdentifier;
        } bbl;
        struct {
            uint8_t threadId;
            uint8_t event;
        } thr;
    } data;
} __attribute__((packed));

/** @brief Written to memory trace file. */
struct MemoryTraceRecord {
    uint8_t recordType;
    union {
        struct {
            uint64_t addr; /**<Virtual address accessed. */
            uint16_t size;  /**<Size in bytes of memory read or written. */
            uint8_t type;
        } operation;
        struct {
            uint16_t nonStdReadOps;
            uint16_t nonStdWriteOps;
        } nonStdHeader;
    } data;
} __attribute__((packed));

/** @brief General usage. */
struct FileHeader {
    uint8_t fileType;
    union {
        struct {
            uint16_t threadCount;
            uint64_t bblCount;
            uint64_t instCount;
        } staticHeader;
        struct {
            uint64_t totalExecutedInstructions;
        } dynamicHeader;
    } data;
} __attribute__((packed));

/**
 * @details This struct is only used internally by the x86 reader. It turns out
 * that more than one instance of the same instruction might be in the processor
 * pipeline at once. Since the number of memory loads and stores might change
 * between them, these values are not kept in the StaticInstructionInfo. When
 * the number of memory operations is indeed fixed, they are written to the
 * numStdMemLoadOps and numStdMemStoreOps variables.
 */
struct TraceReaderInstruction {
    unsigned short numStdMemLoadOps;
    unsigned short numStdMemStoreOps;
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
                     const char *imageName, int tid, long destSize);

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
