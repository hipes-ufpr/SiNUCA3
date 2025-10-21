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

    /* thread 0 or main thread */
    ThreadData* tData = new ThreadData;
    if (tData == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create thread data array\n");
        return 1;
    }
    if (tData->Allocate(sourceDir, imageName, 0)) {
        SINUCA3_ERROR_PRINTF("OpenTrace failed to allocate tData!\n");
        return 1;
    }

    this->threadDataVec.push_back(tData);

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

    /* thread not yet created */
    if ((unsigned long)tid >= this->threadDataVec.size()) {
        return FetchResultNop;
    }

    for (int i = 0; i < this->totalThreads; i++) {
        if (this->threadDataVec[tid]->dynFile.ReachedDynamicTraceEnd()) {
            return FetchResultEnd;
        }
    }

    if (this->threadDataVec[tid]->dynFile.ReachedDynamicTraceEnd()) {
        return FetchResultNop;
    }

    if (!this->threadDataVec[tid]->isInsideBasicBlock) {
        this->threadDataVec[tid]->currentInst = 0;

        /* Loop
         * - if new thread is created: create and continue
         * - if tid trace ended: return FetchResultNop
         * - if new inst is fetched, break
         */
        do {
            if (this->threadDataVec[tid]->dynFile.ReadDynamicRecord()) {
                return FetchResultError;
            }

        } while ();
    }

    ret->staticInfo =
        &this->instructionDict[this->threadDataVec[tid]->currentBasicBlock]
                              [this->threadDataVec[tid]->currentInst];

    unsigned long arraySize;

    if (ret->staticInfo->instReadsMemory || ret->staticInfo->instWritesMemory) {
        if (this->threadDataVec[tid]->memFile.ReadMemoryOperations()) {
            SINUCA3_ERROR_PRINTF("Failed to read memory operations\n");
            return FetchResultError;
        }

        ret->dynamicInfo.numReadings =
            this->threadDataVec[tid]->memFile.GetNumberOfLoads();
        ret->dynamicInfo.numWritings =
            this->threadDataVec[tid]->memFile.GetNumberOfStores();

        arraySize = sizeof(ret->dynamicInfo.readsAddr) / sizeof(unsigned long);

        if (this->threadDataVec[tid]->memFile.CopyLoadOperations(
                ret->dynamicInfo.readsAddr, arraySize,
                ret->dynamicInfo.readsSize, arraySize)) {
            return FetchResultError;
        }
        if (this->threadDataVec[tid]->memFile.CopyStoreOperations(
                ret->dynamicInfo.writesAddr, arraySize,
                ret->dynamicInfo.writesSize, arraySize)) {
            return FetchResultError;
        }
    }

    ++this->threadDataVec[tid]->currentInst;
    if (this->threadDataVec[tid]->currentInst >=
        this->basicBlockSizeArr[this->threadDataVec[tid]->currentBasicBlock]) {
        this->threadDataVec[tid]->isInsideBasicBlock = false;
    }

    this->threadDataVec[tid]->fetchedInst++;

    return FetchResultOk;
}

void SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("###########################\n");
}

int ThreadData::Allocate(const char *sourceDir, const char *imageName,
                         int tid) {
    if (this->dynFile.OpenFile(sourceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF("Failed to open dynamic trace\n");
        return 1;
    }
    if (this->memFile.OpenFile(sourceDir, imageName, tid)) {
        SINUCA3_ERROR_PRINTF("Failed to open memory trace\n");
        return 1;
    }
    return 0;
}

#ifndef NDEBUG
int TestTraceReader() {
    TraceReader *reader = new SinucaTraceReader;

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
        printf("Instruction address [%p]\n",
               (void *)instPkt.staticInfo->instAddress);
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
