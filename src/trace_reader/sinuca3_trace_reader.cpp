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
 * @file sinuca3_trace_reader.cpp
 */

#include "sinuca3_trace_reader.hpp"

#include <alloca.h>

#include <cassert>
#include <cstddef>
#include <cstdio>  // FILE*

#include "../../trace_generator/sinuca3_pintool.hpp"
#include "../utils/logging.hpp"
#include "buffer.hpp"
#include "trace_reader.hpp"

extern "C" {
#include <fcntl.h>     // open
#include <sys/mman.h>  // mmap
#include <unistd.h>    // lseek
}

static inline void IncreaseOffset(size_t *offset, size_t size) {
    *offset += size;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTraceFile(
    TraceFileType ft, const char *name) {
    std::vector<FILE *> *vec;
    int numThreads;
    unsigned long fileNameSize = strlen(name) + 128;
    char *fileName = (char *)alloca(fileNameSize);
    static const char *fileTypes[] = {"dynamic", "memory"};
    const char *fileType;

    fileName[0] = '\0';
    numThreads = 1;
    switch (ft) {
        case DynamicFile:
            vec = &this->ThreadsDynFiles;
            fileType = fileTypes[0];
            break;
        case MemoryFile:
            vec = &this->ThreadsMemFiles;
            fileType = fileTypes[1];
            break;
    }

    for (int i = 0; i < numThreads; i++) {
        snprintf(fileName, fileNameSize, "./trace/%s_%s_tid%d.trace", fileType,
                 name, i);
        vec->push_back(fopen(fileName, "rb"));
        if (vec->back() == NULL) {
            SINUCA3_ERROR_PRINTF("Could not open => %s\n", fileName);
            return 1;
        }
        fileName[0] = '\0';
    }

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *executableName) {
    unsigned long staticFileNameSize = strlen(executableName) + 64;
    char *staticFileName = (char *)alloca(staticFileNameSize);

    /* Open Trace Files */
    if (OpenTraceFile(DynamicFile, executableName)) return 1;
    if (OpenTraceFile(MemoryFile, executableName)) return 1;

    this->isInsideBBL = false;
    this->currentBBL = 0;
    this->binaryTotalBBLs = 0;

    snprintf(staticFileName, staticFileNameSize, "./trace/static_%s.trace",
             executableName);
    if (this->GenerateBinaryDict(staticFileName)) return 1;

    return 0;
}

unsigned long
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTraceSize() {
    return this->binaryTotalBBLs;
}

unsigned long sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GetNumberOfFetchedInstructions() {
    return this->fetchInstructions;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTotalBBLs(
    char *mmapPtr, size_t *mmapOff) {
    /* The pintool writes to the beginning of the static
     * trace the total number of basic blocks */
    unsigned int *bbls = &this->binaryTotalBBLs;

    if (mmapPtr == NULL) {
        return 1;
    }
    *bbls = *(unsigned int *)(mmapPtr + *mmapOff);
    IncreaseOffset(mmapOff, sizeof(this->binaryTotalBBLs));
    SINUCA3_DEBUG_PRINTF("Number of BBLs => %u\n", this->binaryTotalBBLs);

    return 0;
}

/* Helper functions */
bool GetBitBool(unsigned char byte, int pos) {
    return (byte & (1 << pos)) != 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict(char *staticFileName) {
    traceGenerator::DataINS *data;
    size_t mmapOffset = 0;
    size_t mmapSize;
    unsigned int bblSize;
    unsigned short instCounter;
    unsigned int totalIns;
    unsigned int bblCounter = 0;
    InstructionInfo *package;
    InstructionInfo *poolPointer;
    StaticInstructionInfo *info;
    char *mmapPtr;

    int fd = open(staticFileName, O_RDONLY);
    if (fd == -1) {
        SINUCA3_ERROR_PRINTF("Could not open => %s\n", staticFileName);
        return 1;
    }

    /* Map static trace file to process virtual memory */
    mmapSize = lseek(fd, 0, SEEK_END);
    mmapPtr = (char *)mmap(NULL, mmapSize, PROT_READ, MAP_PRIVATE, fd, 0);
    /* Ignoring number of threads in static file for now <== */
    IncreaseOffset(&mmapOffset, sizeof(unsigned int));

    if (GetTotalBBLs(mmapPtr, &mmapOffset)) return 1;
    this->binaryBBLsSize = new unsigned short[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];

    totalIns = *(unsigned int *)(mmapPtr + mmapOffset);
    SINUCA3_DEBUG_PRINTF("Total Ins => %u\n", totalIns);
    IncreaseOffset(&mmapOffset, sizeof(totalIns));
    this->pool = new InstructionInfo[totalIns];
    poolPointer = this->pool;

    while (bblCounter < this->binaryTotalBBLs) {
        /* Total of instructions of current basic block */
        bblSize = *(unsigned int *)(mmapPtr + mmapOffset);
        IncreaseOffset(&mmapOffset, sizeof(bblSize));
        SINUCA3_DEBUG_PRINTF("Bbl (%u/%u) Size => %u\n", bblCounter + 1,
                             this->binaryTotalBBLs, bblSize);

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = poolPointer;
        poolPointer += bblSize;

        instCounter = 0;
        while (instCounter < bblSize) {
            package = &this->binaryDict[bblCounter][instCounter];
            info = &package->staticInfo;
            data = (traceGenerator::DataINS *)(mmapPtr + mmapOffset);

            strncpy(info->opcodeAssembly, data->name,
                    traceGenerator::MAX_INSTRUCTION_NAME_LENGTH);
            info->opcodeAddress = data->addr;
            info->opcodeSize = data->size;
            info->baseReg = data->baseReg;
            info->indexReg = data->indexReg;
            info->branchType = data->branchType;
            info->numReadRegs = data->numReadRegs;
            info->numWriteRegs = data->numWriteRegs;
            memcpy(info->readRegs, data->readRegs,
                   data->numReadRegs * sizeof(*data->readRegs));
            memcpy(info->writeRegs, data->writeRegs,
                   data->numWriteRegs * sizeof(*data->writeRegs));

            info->isPredicated = GetBitBool(data->booleanValues, 0);
            info->isPrefetch = GetBitBool(data->booleanValues, 1);
            info->isControlFlow = GetBitBool(data->booleanValues, 2);
            info->isIndirect = GetBitBool(data->booleanValues, 3);
            info->isNonStdMemOp = GetBitBool(data->booleanValues, 4);

            package->staticNumReadings = 0;
            package->staticNumWritings = 0;
            if (package->staticInfo.isNonStdMemOp == false) {
                if (GetBitBool(data->booleanValues, 5))
                    package->staticNumReadings++;
                if (GetBitBool(data->booleanValues, 6))
                    package->staticNumReadings++;
                if (GetBitBool(data->booleanValues, 7))
                    package->staticNumWritings++;
            }

            IncreaseOffset(&mmapOffset, sizeof(*data));

            SINUCA3_DEBUG_PRINTF("INS NAME => %s\n", info->opcodeAssembly);

            instCounter++;
        }

        bblCounter++;
    }

    munmap(mmapPtr, mmapSize);
    close(fd);

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    TraceNextDynamic(unsigned int *nextBbl) {
    static Buffer buf;
    buf.bufSize = (BUFFER_SIZE / sizeof(unsigned int)) * sizeof(unsigned int);  // sizeof(bllId);

    if (buf.eofLocation > 0 && buf.offset == buf.eofLocation) return 1;

    if (buf.offset >= buf.bufSize) {
        if (buf.readBuffer(this->ThreadsDynFiles[0])) {
            return 1;
        }
    }
    *nextBbl = *(unsigned int *)(buf.store + buf.offset);
    IncreaseOffset(&buf.offset, sizeof(*nextBbl));

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextMemory(
    InstructionPacket *ret, InstructionInfo *packageInfo) {
    static Buffer buf;
    traceGenerator::DataMEM *data;

    if (buf.eofLocation > 0 && buf.offset == buf.eofLocation) return 1;

    if (buf.offset >= buf.bufSize) {
        if (buf.readBufSizeFromFile(this->ThreadsMemFiles[0])) {
            return 1;
        }
        if (buf.readBuffer(this->ThreadsMemFiles[0])) {
            return 1;
        }
    }

    /* In case the instruction performs non standard memory operations
     * with variable number of operands, the number of readings/writings
     * is written directly to the memory trace file
     *
     * Otherwise, it was written in the static trace file.
     */
    if (ret->staticInfo->isNonStdMemOp) {
        ret->dynamicInfo.numReadings =
            *(unsigned short *)(buf.store + buf.offset);
        IncreaseOffset(&buf.offset, sizeof(unsigned short));
        ret->dynamicInfo.numWritings =
            *(unsigned short *)(buf.store + buf.offset);
        IncreaseOffset(&buf.offset, sizeof(unsigned short));
    } else {
        ret->dynamicInfo.numReadings = packageInfo->staticNumReadings;
        ret->dynamicInfo.numWritings = packageInfo->staticNumWritings;
    }

    data = (traceGenerator::DataMEM *)(buf.store + buf.offset);
    for (unsigned short readIt = 0; readIt < ret->dynamicInfo.numReadings;
         readIt++) {
        ret->dynamicInfo.readsAddr[readIt] = data->addr;
        ret->dynamicInfo.readsSize[readIt] = data->size;
        data++;
    }
    for (unsigned short writeIt = 0; writeIt < ret->dynamicInfo.numWritings;
         writeIt++) {
        ret->dynamicInfo.writesAddr[writeIt] = data->addr;
        ret->dynamicInfo.writesSize[writeIt] = data->size;
        data++;
    }
    IncreaseOffset(&buf.offset,
                   (size_t)((char *)data - buf.store) - buf.offset);

    return 0;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    InstructionPacket *ret) {
    if (!this->isInsideBBL) {
        if (this->TraceNextDynamic(&this->currentBBL)) {
            SINUCA3_DEBUG_PRINTF("Fetched ended!\n");
            return FetchResultEnd;
        }
        this->isInsideBBL = true;
        this->currentOpcode = 0;
    }

    InstructionInfo *packageInfo =
        &this->binaryDict[this->currentBBL][this->currentOpcode];
    (*ret).staticInfo = &(packageInfo->staticInfo);
    this->TraceNextMemory(ret, packageInfo);

    this->currentOpcode++;
    if (this->currentOpcode >= this->binaryBBLsSize[this->currentBBL]) {
        this->isInsideBBL = false;
    }

    this->fetchInstructions++;

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret).staticInfo->opcodeAssembly);

    return FetchResultOk;
}

void sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("Fetch Instructions:%lu\n", this->fetchInstructions);
    SINUCA3_LOG_PRINTF("###########################\n");
}

#ifdef TEST_MAIN
int main() {
    sinuca::traceReader::TraceReader *tracer =
        new sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader();
    tracer->OpenTrace("test");
    delete tracer;
}
#endif
