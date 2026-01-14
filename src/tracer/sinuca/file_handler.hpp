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
 * @brief Common trace file handling API.
 */

#include <cstdio>
#include <cstring>

#include "engine/default_packets.hpp"
#include "utils/logging.hpp"

extern "C" {
#include <alloca.h>
#include <errno.h>
#include <stdint.h>
}

#define _PACKED __attribute__((packed))

const int MAX_IMAGE_NAME_SIZE = 255;
const int RECORD_ARRAY_SIZE = 10000;
const int CURRENT_TRACE_VERSION = 1;
const unsigned char MAGIC_NUMBER = 187;

const char TRACE_TARGET_X86[] = "X86";
const char TRACE_TARGET_ARM[] = "ARM";
const char TRACE_TARGET_RISCV[] = "RISCV";
const char PREFIX_STATIC_FILE[] = "S3S";
const char PREFIX_DYNAMIC_FILE[] = "S3D";
const char PREFIX_MEMORY_FILE[] = "S3M";
const int PREFIX_SIZE = sizeof(PREFIX_STATIC_FILE);

enum FileType : uint8_t {
    FileTypeStaticTrace,
    FileTypeDynamicTrace,
    FileTypeMemoryTrace
};

enum TargetArch : uint8_t { TargetArchX86, TargetArchARM, TargetArchRISCV };

enum StaticTraceRecordType : uint8_t {
    StaticRecordInstruction,
    StaticRecordBasicBlockSize
};

enum DynamicTraceRecordType : uint8_t {
    DynamicRecordBasicBlockIdentifier,
    DynamicRecordThreadEvent
};

enum ThreadEventType : uint32_t {
    ThreadEventBarrierSync,
    ThreadEventCriticalStart,
    ThreadEventCriticalEnd,
    ThreadEventAbruptEnd
};

enum MemoryRecordType : uint8_t {
    MemoryRecordHeader,
    MemoryRecordLoad,
    MemoryRecordStore
};

/** @brief Instruction extracted informations */
struct Instruction {
    uint64_t instructionAddress;
    uint64_t instructionSize;
    uint16_t readRegsArray[MAX_REGISTERS];
    uint16_t writtenRegsArray[MAX_REGISTERS];
    uint8_t wRegsArrayOccupation;
    uint8_t rRegsArrayOccupation;
    uint8_t instHasFallthrough;
    uint8_t isBranchInstruction;
    uint8_t isSyscallInstruction;
    uint8_t isCallInstruction;
    uint8_t isRetInstruction;
    uint8_t isSysretInstruction;
    uint8_t isPrefetchHintInst;
    uint8_t isPredicatedInst;
    uint8_t isIndirectCtrlFlowInst;
    uint8_t instCausesCacheLineFlush;
    uint8_t instPerformsAtomicUpdate;
    uint8_t instReadsMemory;
    uint8_t instWritesMemory;
    char instructionMnemonic[INST_MNEMONIC_LEN];
} _PACKED;

/** @brief Written to static trace file. */
struct StaticTraceRecord {
    union _PACKED {
        uint16_t basicBlockSize;
        Instruction instruction;
    } data;
    uint8_t recordType;

    inline StaticTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief Written to dynamic trace file. */
struct DynamicTraceRecord {
    union _PACKED {
        uint32_t basicBlockId;
        uint32_t threadEvent;
    } data;
    uint8_t recordType;

    inline DynamicTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief Written to memory trace file. */
struct MemoryTraceRecord {
    union _PACKED {
        struct _PACKED {
            uint64_t address; /**<Virtual address accessed. */
            uint16_t size;    /**<Size in bytes of memory read or written. */
        } operation;
        int32_t numberOfMemoryOps;
    } data;
    uint8_t recordType;

    inline MemoryTraceRecord() { memset(this, 0, sizeof(*this)); }
} _PACKED;

/** @brief File header for general usage. */
struct FileHeader {
    uint8_t magicNumber;
    int8_t prefix[PREFIX_SIZE];
    uint8_t fileType;
    uint8_t traceVersion;
    uint8_t targetArch;

    union _PACKED {
        struct _PACKED {
            uint32_t instCount;
            uint32_t bblCount;
            uint16_t threadCount;
        } staticHeader;
        struct {
            uint64_t totalExecutedInstructions;
        } dynamicHeader;
    } data;

    inline FileHeader() {
        memset(this, 0, sizeof(*this));
        magicNumber = MAGIC_NUMBER;
        traceVersion = CURRENT_TRACE_VERSION;
    }

    /** @brief Write header to file. */
    int FlushHeader(FILE *file);
    /** @brief Read header from file. */
    int LoadHeader(FILE *file);
    /** @brief Read header from file if it is mapped to virtual memory. */
    int LoadHeader(char *file, unsigned long *fileOffset);
    /** @brief Adjust file pointer. The header is generally written at file
     * clousure, so the file ptr must be moved to leave enough space for it. */
    void ReserveHeaderSpace(FILE *file);
    /** @brief Set header type and prefix. */
    void SetHeaderType(uint8_t fileType);
} _PACKED;

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

#endif
