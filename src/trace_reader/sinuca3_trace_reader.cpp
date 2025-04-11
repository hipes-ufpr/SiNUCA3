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
 * @file sinuca3_trace_reader.cpp
 */

#include "sinuca3_trace_reader.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>  // FILE*

#include "../../trace_generator/sinuca3_pintool.hpp"
#include "../utils/logging.hpp"
#include "trace_reader.hpp"
#include "buffer.h"

extern "C" {
#include <sys/mman.h>
#include <unistd.h>
}

inline void IncreaseOffset(size_t *offset, size_t size) { *offset += size; }

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *traceFileName) {
    //------------------------//
    char fileName[256];
    int fd;

    /* Open static trace */
    fileName[0] = '\0';
    snprintf(fileName, sizeof(fileName), "../../trace/static_%s.trace",
             traceFileName);
    this->StaticTraceFile = fopen(fileName, "rb");
    if (this->StaticTraceFile == NULL) {
        SINUCA3_ERROR_PRINTF("Could not open the static file.\n%s\n", fileName);
        return 1;
    }
    SINUCA3_DEBUG_PRINTF("Static File = %s => READY !\n", fileName);

    /* Map static trace file to process virtual memory */
    fd = fileno(this->StaticTraceFile);
    this->mmapSize = lseek(fd, 0, SEEK_END);
    SINUCA3_DEBUG_PRINTF("MMAP SIZE => %lu\n", this->mmapSize);
    this->mmapPtr = (char*)mmap(NULL, mmapSize, PROT_READ, MAP_PRIVATE, fd, 0);
    SINUCA3_DEBUG_PRINTF("MMAP PTR => %p\n", mmapPtr);
    /* Get total number of threads */
    // ==> TODO <==
    this->mmapPtr += sizeof(unsigned int);

    /* Open dynamic traces */
    for (int i = 0; i < 1; i++) {
        fileName[0] = '\0';
        snprintf(fileName, sizeof(fileName), "../../trace/dynamic_%s_tid%d.trace",
                 traceFileName, i);
        this->ThreadsDynFiles.push_back(fopen(fileName, "rb"));
        if (this->ThreadsDynFiles.back() == NULL) {
            SINUCA3_ERROR_PRINTF("Could not open the dynamic file.\n%s\n",
                                 fileName);
            return 1;
        }
        SINUCA3_DEBUG_PRINTF("Dynamic File = %s => READY !\n", fileName);
    }

    /* Open memory traces */
    for (int i = 0; i < 1; i++) {
        fileName[0] = '\0';
        snprintf(fileName, sizeof(fileName), "../../trace/memory_%s_tid%d.trace",
                 traceFileName, i);
        this->ThreadsMemFiles.push_back(fopen(fileName, "rb"));
        if (this->ThreadsMemFiles.back() == NULL) {
            SINUCA3_ERROR_PRINTF("Could not open the memory file.\n%s\n", fileName);
            return 1;
        }
        SINUCA3_DEBUG_PRINTF("Memory File = %s => READY !\n", fileName);
    }
    

    this->isInsideBBL = false;
    this->currentBBL = 0;
    this->binaryTotalBBLs = 0;

    if (this->GetTotalBBLs()) return 1;
    this->binaryBBLsSize = new unsigned short[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];
    if (this->GenerateBinaryDict()) return 1;

    return 0;
}

unsigned long
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTraceSize() {
    return this->binaryTotalBBLs;
}

unsigned long sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GetNumberOfFetchedInstructions() {
    //------------------------------//
    return this->fetchInstructions;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTotalBBLs() {
    /* The pintool writes to the beginning of the static
     * trace the total number of basic blocks */
    rewind(this->StaticTraceFile);
    unsigned int *num = &this->binaryTotalBBLs;
    size_t read = fread(num, 1, sizeof(*num), StaticTraceFile);
    if (read <= 0) {
        return 1;
    }

    SINUCA3_DEBUG_PRINTF("NUMBER OF BBLs => %u\n", this->binaryTotalBBLs);
    return 0;
}

/* Helper functions */
bool getBitBool(unsigned char byte, int pos) {
    return (byte & (1 << pos)) != 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict() {
    //------------------//
    traceGenerator::DataINS *data;
    size_t mmapOffset = 0, bytes;
    unsigned short bblSize, instCounter;
    unsigned int totalIns, bblCounter = 0;
    InstructionInfo *package, *pool, *poolPointer;
    StaticInstructionInfo *info;

    IncreaseOffset(&mmapOffset, sizeof(this->binaryTotalBBLs));
    totalIns = *(unsigned int*)(this->mmapPtr+mmapOffset);
    IncreaseOffset(&mmapOffset, sizeof(totalIns));
    pool = new InstructionInfo[totalIns];
    poolPointer = pool;

    while (bblCounter < this->binaryTotalBBLs) {
        /* Total of instructions of current basic block */
        bblSize = *(unsigned short *)(this->mmapPtr + mmapOffset);
        IncreaseOffset(&mmapOffset, sizeof(bblSize));
        SINUCA3_DEBUG_PRINTF("BBL SIZE => %d\n", bblSize);

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = poolPointer;
        poolPointer += bblSize;

        instCounter = 0;
        while (instCounter < bblSize) {
            package = &this->binaryDict[bblCounter][instCounter];
            info = &package->staticInfo;
            data = (traceGenerator::DataINS*)(mmapPtr + mmapOffset);
            
            info->opcodeAddress = data->addr;
            info->opcodeSize = data->size;
            info->baseReg = data->baseReg;
            info->indexReg = data->indexReg;
            info->branchType = data->branchType;
            info->numReadRegs = data->numReadRegs;
            info->numWriteRegs = data->numWriteRegs;

            info->isPredicated = getBitBool(data->booleanValues, 0);
            info->isPrefetch = getBitBool(data->booleanValues, 1);
            info->isControlFlow = getBitBool(data->booleanValues, 2);
            info->isIndirect = getBitBool(data->booleanValues, 3);
            info->isNonStdMemOp = getBitBool(data->booleanValues, 4);

            package->staticNumReadings = 0;
            package->staticNumWritings = 0;
            if (package->staticInfo.isNonStdMemOp == false) {
                if (getBitBool(data->booleanValues, 5))
                    package->staticNumReadings++;
                if (getBitBool(data->booleanValues, 6))
                    package->staticNumReadings++;
                if (getBitBool(data->booleanValues, 7))
                    package->staticNumWritings++;
            }

            IncreaseOffset(&mmapOffset, sizeof(*data));

            bytes = info->numReadRegs*sizeof(*info->readRegs);
            memcpy(info->readRegs, mmapPtr + mmapOffset, bytes);
            IncreaseOffset(&mmapOffset, bytes);
            bytes = info->numWriteRegs*sizeof(*info->writeRegs);
            memcpy(info->writeRegs, mmapPtr + mmapOffset, bytes);
            IncreaseOffset(&mmapOffset, bytes);
            bytes = strlen(mmapPtr + mmapOffset)+1;
            memcpy(info->opcodeAssembly, mmapPtr + mmapOffset, bytes);
            IncreaseOffset(&mmapOffset, bytes);

            SINUCA3_DEBUG_PRINTF("INS NAME => %s\n", info->opcodeAssembly);

            instCounter++;
        }

        bblCounter++;
    }

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    TraceNextDynamic(unsigned int *nextBbl) {
    //-------------------------------------//
    static Buffer buf;
    
    // ==> TODO <==
    if (buf.offset == buf.bufSize) {
        if (buf.readBufSizeFromFile(this->ThreadsDynFiles[0])) {
            return 1;
        }
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
    //---------------------------------------------------//
    static Buffer buf;
    traceGenerator::DataMEM *data;
    
    // ==> TODO <==
    if (buf.offset == buf.bufSize) {
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
        ret->dynamicInfo.numReadings = *(unsigned short *)(buf.store + buf.offset);
        IncreaseOffset(&buf.offset, sizeof(unsigned short));
        ret->dynamicInfo.numWritings = *(unsigned short *)(buf.store + buf.offset);
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
    IncreaseOffset(&buf.offset, (size_t)((char *)data - buf.store) - buf.offset);

    return 0;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    InstructionPacket *ret) {
    //---------------------//
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
    //---------------//
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("Fetch Instructions:%lu\n", this->fetchInstructions);
    SINUCA3_LOG_PRINTF("###########################\n");
}

#ifdef TEST_MAIN
int main() {
    sinuca::traceReader::TraceReader* tracer = new sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader();
    tracer->OpenTrace("test");
}
#endif