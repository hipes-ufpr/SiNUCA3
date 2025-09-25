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

#include <cassert>
#include <sinuca3.hpp>
#include <tracer/trace_reader.hpp>

#include "engine/default_packets.hpp"
#include "tracer/sinuca/file_handler.hpp"

int sinucaTracer::SinucaTraceReader::OpenTrace(const char *imageName,
                                               const char *sourceDir) {
    StaticTraceFile staticFile;

    if (staticFile.OpenFile(sourceDir, imageName)) return 1;
    staticFile.ReadFileHeader();
    this->totalThreads = staticFile.GetNumThreads();
    this->binaryTotalBBLs = staticFile.GetTotalBasicBlocks();

    this->thrsInfo = new ThrInfo[this->totalThreads];
    for (int i = 0; i < this->totalThreads; i++) {
        if (this->thrsInfo[i].Allocate(sourceDir, imageName, i)) {
            return 1;
        }
    }
    if (this->GenerateBinaryDict(&staticFile)) {
        SINUCA3_ERROR_PRINTF("Failed to generate instruction dictionary\n");
        return 1;
    }

    return 0;
}

void sinucaTracer::SinucaTraceReader::CloseTrace() {
    delete[] this->thrsInfo;
    delete[] this->binaryBBLsSize;
    delete[] this->pool;
    delete[] this->binaryDict;
}

int sinucaTracer::SinucaTraceReader::GenerateBinaryDict(
    StaticTraceFile *stFile) {
    InstructionInfo *instInfoPtr;
    unsigned long poolOffset;
    unsigned int bblSize;
    unsigned int instCounter;
    unsigned int bblCounter;
    unsigned long totalStaticInstructions;

    this->binaryBBLsSize = new unsigned int[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];
    totalStaticInstructions = stFile->GetTotalInstructionsInStaticTrace();
    this->pool = new InstructionInfo[totalStaticInstructions];
    poolOffset = 0;

    for (bblCounter = 0; bblCounter < this->binaryTotalBBLs; bblCounter++) {
        if (stFile->ReadBasicBlock()) return 1;
        bblSize = stFile->GetBasicBlockSize();
        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = &this->pool[poolOffset];
        poolOffset += bblSize;

        for (instCounter = 0; instCounter < bblSize; instCounter++) {
            instInfoPtr = &this->binaryDict[bblCounter][instCounter];
            stFile->GetInstruction(instInfoPtr);
        }

        SINUCA3_DEBUG_PRINTF("bbl [%u] size [%u]\n", bblCounter + 1, bblSize);
    }

    return 0;
}

FetchResult sinucaTracer::SinucaTraceReader::Fetch(InstructionPacket *ret,
                                                   int tid) {
    InstructionInfo *packageInfo;

    if (!this->thrsInfo[tid].isInsideBBL) {
        if (this->thrsInfo[tid].dynFile->ReadRecordFromFile()) {
            return FetchResultEnd;
        }
        /* deal with record types */
        this->thrsInfo[tid].isInsideBBL = true;
        this->thrsInfo[tid].currentOpcode = 0;
    }

    packageInfo = &this->binaryDict[this->thrsInfo[tid].currentBBL]
                                   [this->thrsInfo[tid].currentOpcode];
    this->CopyMemoryOperations(ret, packageInfo, &this->thrsInfo[tid]);

    this->thrsInfo[tid].currentOpcode++;
    if (this->thrsInfo[tid].currentOpcode >=
        this->binaryBBLsSize[this->thrsInfo[tid].currentBBL]) {
        this->thrsInfo[tid].isInsideBBL = false;
    }

    this->thrsInfo[tid].fetchedInst++;

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret).staticInfo->opcodeAssembly);

    return FetchResultOk;
}

int sinucaTracer::SinucaTraceReader::GetTotalThreads() {
    return this->totalThreads;
}

unsigned long sinucaTracer::SinucaTraceReader::GetTotalBBLs() {
    return this->binaryTotalBBLs;
}

unsigned long sinucaTracer::SinucaTraceReader::GetNumberOfFetchedInst(int tid) {
    return this->thrsInfo[tid].fetchedInst;
}

unsigned long sinucaTracer::SinucaTraceReader::GetTotalInstToBeFetched(
    int tid) {
    return this->thrsInfo[tid].dynFile->GetTotalExecutedInstructions();
}

void sinucaTracer::SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");

    SINUCA3_LOG_PRINTF("###########################\n");
}

int sinucaTracer::ThrInfo::Allocate(const char *sourceDir,
                                    const char *imageName, int tid) {
    this->dynFile = new DynamicTraceFile();
    if (this->dynFile->OpenFile(sourceDir, imageName, tid)) return 1;
    this->memFile = new MemoryTraceFile();
    if (this->memFile->OpenFile(sourceDir, imageName, tid)) return 1;
    return 0;
}

void sinucaTracer::SinucaTraceReader::CopyMemoryOperations(
    InstructionPacket *instPkt, InstructionInfo *instInfo, ThrInfo *thrInfo) {
    const unsigned long *addrs;
    const unsigned int *sizes;
    unsigned long arraySize;
    int accessType;
    short totalStdOps;

    if (instInfo->staticInfo.isNonStdMemOp) {
        thrInfo->memFile->ReadNonStandardMemoryAccess();

        instPkt->dynamicInfo.numReadings =
            thrInfo->memFile->GetReadOpsNonStdAcc();
        addrs = thrInfo->memFile->GetReadAddrArrayNonStdAcc(&arraySize);
        memcpy(instPkt->dynamicInfo.readsAddr, addrs, arraySize);
        sizes = thrInfo->memFile->GetReadSizeArrayNonStdAcc(&arraySize);
        memcpy(instPkt->dynamicInfo.readsSize, sizes, arraySize);

        instPkt->dynamicInfo.numWritings =
            thrInfo->memFile->GetWriteOpsNonStdAcc();
        addrs = thrInfo->memFile->GetWriteAddrArrayNonStdAcc(&arraySize);
        memcpy(instPkt->dynamicInfo.writesAddr, addrs, arraySize);
        sizes = thrInfo->memFile->GetWriteSizeArrayNonStdAcc(&arraySize);
        memcpy(instPkt->dynamicInfo.writesSize, sizes, arraySize);
    } else {
        thrInfo->memFile->ReadStandardMemoryAccess();

        instPkt->dynamicInfo.numReadings = instInfo->staticNumReadings;
        instPkt->dynamicInfo.numWritings = instInfo->staticNumWritings;
        totalStdOps =
            instPkt->dynamicInfo.numReadings + instPkt->dynamicInfo.numWritings;
        for (int i = 0, r = 0, w = 0; i < totalStdOps; i++) {
            accessType = thrInfo->memFile->GetTypeStdAccess();
            if (accessType == STD_ACCESS_READ) {
                instPkt->dynamicInfo.readsAddr[r] =
                    thrInfo->memFile->GetAddressStdAccess();
                instPkt->dynamicInfo.readsSize[r] =
                    thrInfo->memFile->GetSizeStdAccess();
                r++;
            } else if (accessType == STD_ACCESS_WRITE) {
                instPkt->dynamicInfo.writesAddr[w] =
                    thrInfo->memFile->GetAddressStdAccess();
                instPkt->dynamicInfo.writesSize[w] =
                    thrInfo->memFile->GetSizeStdAccess();
                w++;
            }
        }
    }
}
