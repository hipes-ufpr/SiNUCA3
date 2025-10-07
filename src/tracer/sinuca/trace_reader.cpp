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
 * @file x86_trace_reader.cpp
 */

#include "trace_reader.hpp"

#include <sinuca3.hpp>

#include "engine/default_packets.hpp"
#include "tracer/sinuca/file_handler.hpp"
#include "tracer/trace_reader.hpp"
#include "utils/logging.hpp"

namespace sinucaTracer {

int SinucaTraceReader::OpenTrace(const char *imageName, const char *sourceDir) {
    this->staticTrace = new StaticTraceReader;
    if (this->staticTrace == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create static trace reader\n");
        return 1;
    }
    if (this->staticTrace->OpenFile(sourceDir, imageName)) {
        SINUCA3_ERROR_PRINTF("Failed to open static trace\n");
        return 1;
    }

    this->totalThreads = this->staticTrace->GetNumThreads();
    this->totalBasicBlocks = this->staticTrace->GetTotalBasicBlocks();
    this->totalStaticInst = this->staticTrace->GetTotalInstInStaticTrace();

    this->threadDataArray = new ThreadData[this->totalThreads];
    if (this->threadDataArray == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create thread data array\n");
        return 1;
    }

    for (int tid = 0; tid < this->totalThreads; ++tid) {
        if (this->threadDataArray[tid].Allocate(sourceDir, imageName, tid)) {
            return 1;
        }
    }

    if (this->GenerateInstructionDict()) {
        SINUCA3_ERROR_PRINTF("Failed to generate instruction dictionary\n");
        return 1;
    }

    return 0;
}

int SinucaTraceReader::GenerateInstructionDict() {
    StaticInstructionInfo *instInfoPtr;
    unsigned long poolOffset;
    unsigned long bblCounter;
    unsigned int bblSize;
    unsigned int instCounter;
    unsigned char recordType;

    this->basicBlockSizeArr = new int[this->totalBasicBlocks];
    if (this->basicBlockSizeArr == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc basicBlockSizeArr\n");
        return 1;
    }

    this->instructionDict = new StaticInstructionInfo *[this->totalBasicBlocks];
    if (this->instructionDict == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc instructionDict\n");
        return 1;
    }

    this->instructionPool = new StaticInstructionInfo[this->totalStaticInst];
    if (this->instructionPool == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to alloc instructionPool\n");
        return 1;
    }

    poolOffset = 0;

    for (bblCounter = 0; bblCounter < this->totalBasicBlocks; bblCounter++) {
        if (this->staticTrace->ReadStaticRecordFromFile()) {
            return 1;
        }

        recordType = this->staticTrace->GetStaticRecordType();
        if (recordType != StaticRecordBasicBlockSize) {
            SINUCA3_ERROR_PRINTF("Expected basic block size record type\n");
            return 1;
        }

        bblSize = this->staticTrace->GetBasicBlockSize();
        this->basicBlockSizeArr[bblCounter] = bblSize;
        this->instructionDict[bblCounter] = &this->instructionPool[poolOffset];
        poolOffset += bblSize;

        for (instCounter = 0; instCounter < bblSize; instCounter++) {
            if (this->staticTrace->ReadStaticRecordFromFile()) {
                return 1;
            }

            recordType = this->staticTrace->GetStaticRecordType();
            if (recordType != StaticRecordInstruction) {
                SINUCA3_ERROR_PRINTF("Expected instruction record type\n");
                return 1;
            }

            instInfoPtr = &this->instructionDict[bblCounter][instCounter];
            this->staticTrace->GetInstruction(instInfoPtr);
        }
    }

    return 0;
}

FetchResult SinucaTraceReader::Fetch(InstructionPacket *ret, int tid) {
    unsigned char recordType;

    for (int i = 0; i < this->totalThreads; i++) {
        if (this->threadDataArray[tid].dynFile->ReachedDynamicTraceEnd()) {
            return FetchResultEnd;
        }
    }

    if (this->threadDataArray[tid].dynFile->ReachedDynamicTraceEnd()) {
        return FetchResultNop;
    }

    if (!this->threadDataArray[tid].isInsideBasicBlock) {
        this->threadDataArray[tid].currentInst = 0;

        do {
            if (this->threadDataArray[tid].dynFile->ReadDynamicRecord()) {
                return FetchResultError;
            }
            recordType = this->threadDataArray[tid].dynFile->GetRecordType();
        } while (recordType != DynamicRecordBasicBlockIdentifier);

        this->threadDataArray[tid].currentBasicBlock =
            this->threadDataArray[tid].dynFile->GetBasicBlockIdentifier();
    }

    ret->staticInfo =
        &this->instructionDict[this->threadDataArray[tid].currentBasicBlock]
                              [this->threadDataArray[tid].currentInst];

    unsigned long arraySize;

    if (ret->staticInfo->instReadsMemory || ret->staticInfo->instWritesMemory) {
        if (this->threadDataArray[tid].memFile->ReadMemoryOperations()) {
            SINUCA3_ERROR_PRINTF("Failed to read memory operations\n");
            return FetchResultError;
        }

        ret->dynamicInfo.numReadings =
            this->threadDataArray[tid].memFile->GetNumberOfMemLoadOps();
        ret->dynamicInfo.numWritings =
            this->threadDataArray[tid].memFile->GetNumberOfMemStoreOps();

        arraySize = sizeof(ret->dynamicInfo.readsAddr) / sizeof(unsigned long);
        if (this->threadDataArray[tid].memFile->CopyLoadOpsAddresses(
                ret->dynamicInfo.readsAddr, arraySize)) {
            return FetchResultError;
        }
        arraySize = sizeof(ret->dynamicInfo.readsSize) / sizeof(unsigned int);
        if (this->threadDataArray[tid].memFile->CopyLoadOpsSizes(
                ret->dynamicInfo.readsSize, arraySize)) {
            return FetchResultError;
        }
        arraySize = sizeof(ret->dynamicInfo.writesAddr) / sizeof(unsigned long);
        if (this->threadDataArray[tid].memFile->CopyStoreOpsAddresses(
                ret->dynamicInfo.writesAddr, arraySize)) {
            return FetchResultError;
        }
        arraySize = sizeof(ret->dynamicInfo.writesSize) / sizeof(unsigned int);
        if (this->threadDataArray[tid].memFile->CopyStoreOpsSizes(
                ret->dynamicInfo.writesSize, arraySize)) {
            return FetchResultError;
        }
    }


    ++this->threadDataArray[tid].currentInst;
    if (this->threadDataArray[tid].currentInst >=
        this->basicBlockSizeArr[this->threadDataArray[tid].currentBasicBlock]) {
        this->threadDataArray[tid].isInsideBasicBlock = false;
    }

    this->threadDataArray[tid].fetchedInst++;

    return FetchResultOk;
}

void SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("###########################\n");
}

int ThreadData::Allocate(const char *sourceDir, const char *imageName,
    int tid) {
    if (!(this->dynFile = new DynamicTraceReader())) {
        SINUCA3_ERROR_PRINTF("Failed to alloc dynamic trace\n");
        return 1;
    }
    if (this->dynFile->OpenFile(sourceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF("Failed to open dynamic trace\n");
        return 1;
    }
    if (!(this->memFile = new MemoryTraceReader())) {
        SINUCA3_ERROR_PRINTF("Failed to alloc memory trace\n");
        return 1;
    }
    if (this->memFile->OpenFile(sourceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF("Failed to open memory trace\n");
        return 1;
    }
    return 0;
}

}  // namespace sinucaTracer

#ifndef NDEBUG
int TestTraceReader() {
    TraceReader* reader = new sinucaTracer::SinucaTraceReader;

    char traceDir[1024];
    char imageName[1024];

    printf("Trace directory: ");
    scanf("%s", traceDir);
    printf("Image name: ");
    scanf("%s", imageName);

    reader->OpenTrace(imageName, traceDir);

    InstructionPacket instPkt;
    FetchResult res = reader->Fetch(&instPkt, 0);
    while (res != FetchResultError && res != FetchResultEnd) {

        printf("Instruction name [%s]\n", instPkt.staticInfo->instMnemonic);
        printf("Instruction address [%p]\n", (void *)instPkt.staticInfo->instAddress);
        printf("Instruction size [%lu]\n", instPkt.staticInfo->instSize);
        printf("Instruction branch [%d]\n", instPkt.staticInfo->branchType);

        res = reader->Fetch(&instPkt, 0);
    }

    if (res == FetchResultError) {
        SINUCA3_ERROR_PRINTF("Fetch result error!\n");
    }

    delete reader;

    return 0;
}
#endif
