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

#include <cstring>
#include <sinuca3.hpp>
#include "engine/default_packets.hpp"

extern "C" {
#include <errno.h>
#include <stdint.h>
}

#define _PACKED __attribute__((packed))

const char TRACE_VERSION[] = "0.0.1"; /**<Used to detect incompatibility.> */
const int MAX_IMAGE_NAME_SIZE = 255;
const int RECORD_ARRAY_SIZE = 10000;

enum FileType : uint8_t {
    FileTypeStaticTrace,
    FileTypeDynamicTrace,
    FileTypeMemoryTrace
};

enum StaticTraceRecordType : uint8_t {
    StaticRecordInstruction,
    StaticRecordBasicBlockSize
};

enum DynamicTraceRecordType : uint8_t {
    DynamicRecordBasicBlockIdentifier,
    DynamicRecordThreadEvent
};

enum ThreadEventType : uint8_t {
    ThreadEventCreateThread,
    ThreadEventDestroyThread,
    ThreadEventHaltThread,
    ThreadEventLockRequest,
    ThreadEventUnlockRequest,
    ThreadEventBarrier
};

enum MemoryRecordType : uint8_t {
    MemoryRecordOperationHeader,
    MemoryRecordOperation
};

enum MemoryOperationType : uint8_t {
    MemoryOperationLoad,
    MemoryOperationStore
};

/** @brief Instruction extracted informations */
struct Instruction {
    uint64_t instructionAddress;
    uint64_t instructionSize;
    uint32_t effectiveAddressWidth;
    uint16_t readRegsArray[MAX_REGISTERS];    /**<Enum defined in reg_ia32.PH */
    uint16_t writtenRegsArray[MAX_REGISTERS]; /**<Enum defined in reg_ia32.PH */
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
    char instructionMnemonic[INST_MNEMONIC_LEN]; /**<Used for debug. */
} _PACKED;

/** @brief Written to static trace file. */
struct StaticTraceRecord {
    union _PACKED {
        uint16_t basicBlockSize;
        Instruction instruction;
    } data;
    uint8_t recordType;

    inline StaticTraceRecord() {
        memset(this, 0, sizeof(*this));
    }
} _PACKED;

/** @brief Written to dynamic trace file. */
struct DynamicTraceRecord {
    union _PACKED {
        struct {
            uint32_t basicBlockIdentifier;
        } bbl;
        struct _PACKED {
            uint8_t eventType;
            union _PACKED {
                struct _PACKED {
                    uint64_t lockAddress;
                    uint8_t isDefaultLock;
                    uint8_t isTestLock;
                    uint8_t isNestedLock;
                } lockInfo;
                struct {
                    uint32_t tid;
                } thrCreate;
            } eventData;
        } thrEvent;
    } data;
    uint8_t recordType;

    inline DynamicTraceRecord() {
        memset(this, 0, sizeof(*this));
    }
} _PACKED;

/** @brief Written to memory trace file. */
struct MemoryTraceRecord {
    union _PACKED {
        struct _PACKED {
            uint64_t address; /**<Virtual address accessed. */
            uint16_t size;    /**<Size in bytes of memory read or written. */
            uint8_t type;     /**<Load or Store. */
        } operation;
        struct {
            int32_t numberOfMemoryOps;
        } opHeader;
    } data;
    uint8_t recordType;

    inline MemoryTraceRecord() {
        memset(this, 0, sizeof(*this));
    }
} _PACKED;

/** @brief General usage. */
struct FileHeader {
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
    uint8_t fileType;
    char traceVersion[sizeof(TRACE_VERSION)];

    inline FileHeader() {
        memset(this, 0, sizeof(*this));
        memcpy(this->traceVersion, TRACE_VERSION, sizeof(this->traceVersion));
    }
    inline int FlushHeader(FILE* file) {
        if (!file) return 1;
        rewind(file);
        return (fwrite(this, 1, sizeof(*this), file) != sizeof(*this));
    }
    inline int LoadHeader(FILE* file) {
        if (!file) return 1;
        return (fread(this, 1, sizeof(*this), file) != sizeof(*this));
    }
    inline void ReserveHeaderSpace(FILE* file) {
        fseek(file, sizeof(*this), SEEK_SET);
    }
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
