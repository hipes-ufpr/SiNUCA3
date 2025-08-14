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

#include "x86_trace_reader.hpp"

#include <cassert>

#include "sinuca3.hpp"
#include "tracer/trace_reader.hpp"

int tracer::SinucaTraceReader::OpenTrace(const char *imageName,
                                         const char *sourceDir) {
    StaticTraceFile staticFile(sourceDir, imageName);
    if (!staticFile.Valid()) return 1;

    this->totalThreads = staticFile.GetNumThreads();
    this->binaryTotalBBLs = staticFile.GetTotalBBLs();
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

void tracer::SinucaTraceReader::CloseTrace() {
    delete[] this->thrsInfo;
    delete[] this->binaryBBLsSize;
    delete[] this->pool;
    delete[] this->binaryDict;
}

int tracer::SinucaTraceReader::GenerateBinaryDict(StaticTraceFile *stFile) {
    InstructionInfo *instInfoPtr;
    unsigned long poolOffset;
    unsigned int bblSize;
    unsigned int instCounter;
    unsigned int bblCounter;

    this->binaryBBLsSize = new unsigned int[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];
    this->pool = new InstructionInfo[stFile->GetTotalIns()];
    poolOffset = 0;

    for (bblCounter = 0; bblCounter < this->binaryTotalBBLs; bblCounter++) {
        if (stFile->GetNewBBLSize(&bblSize)) {
            return 1;
        }
        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = &this->pool[poolOffset];
        poolOffset += bblSize;

        for (instCounter = 0; instCounter < bblSize; instCounter++) {
            instInfoPtr = &this->binaryDict[bblCounter][instCounter];
            stFile->ReadNextInstruction(instInfoPtr);
        }

        SINUCA3_DEBUG_PRINTF("bbl [%u] size [%u]\n", bblCounter + 1, bblSize);
    }

    return 0;
}

FetchResult tracer::SinucaTraceReader::Fetch(InstructionPacket *ret, int tid) {
    InstructionInfo *packageInfo;

    if (!this->thrsInfo[tid].isInsideBBL) {
        if (this->thrsInfo[tid].dynFile->ReadNextBBl(
                &this->thrsInfo[tid].currentBBL)) {
            return FetchResultEnd;
        }
        this->thrsInfo[tid].isInsideBBL = true;
        this->thrsInfo[tid].currentOpcode = 0;
    }

    unsigned int currentBbl = this->thrsInfo[tid].currentBBL;
    unsigned int currentIns = this->thrsInfo[tid].currentOpcode;
    packageInfo = &this->binaryDict[currentBbl][currentIns];
    ret->staticInfo = &(packageInfo->staticInfo);
    this->thrsInfo[tid].memFile->ReadNextMemAccess(packageInfo,
                                                   &ret->dynamicInfo);

    this->thrsInfo[tid].currentOpcode++;
    if (this->thrsInfo[tid].currentOpcode >=
        this->binaryBBLsSize[this->thrsInfo[tid].currentBBL]) {
        this->thrsInfo[tid].isInsideBBL = false;
    }

    this->thrsInfo[tid].fetchedInst++;

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret).staticInfo->opcodeAssembly);

    return FetchResultOk;  // Maybe this is not suitable for multiple threads
}

int tracer::SinucaTraceReader::GetTotalThreads() { return this->totalThreads; }

unsigned long tracer::SinucaTraceReader::GetTotalBBLs() {
    return this->binaryTotalBBLs;
}

unsigned long tracer::SinucaTraceReader::GetNumberOfFetchedInst(int tid) {
    return this->thrsInfo[tid].fetchedInst;
}

unsigned long tracer::SinucaTraceReader::GetTotalInstToBeFetched(int tid) {
    return this->thrsInfo[tid].dynFile->GetTotalExecInst();
}

void tracer::SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    // SINUCA3_LOG_PRINTF("Fetch Instructions:%lu\n", this->fetchInstructions);
    SINUCA3_LOG_PRINTF("###########################\n");
}

tracer::ThrInfo::ThrInfo() {
    this->isInsideBBL = false;
    this->fetchedInst = 0;
}

int tracer::ThrInfo::Allocate(const char *sourceDir, const char *imageName,
                              int tid) {
    this->dynFile = new DynamicTraceFile(sourceDir, imageName, tid);
    if (!this->dynFile->Valid()) return 1;
    this->memFile = new MemoryTraceFile(sourceDir, imageName, tid);
    if (!this->memFile->Valid()) return 1;
    return 0;
}

tracer::ThrInfo::~ThrInfo() {
    delete this->memFile;
    delete this->dynFile;
}

#ifdef TEST_MAIN
int main() {
    InstructionPacket instInfo;
    FetchResult ret;
    TraceReader *tracer = new tracer::SinucaTraceReader();
    tracer->OpenTrace("factorials", "");

    do {
        ret = tracer->Fetch(&instInfo, 0);
    } while (ret == FetchResultOk);

    delete tracer;
}
#endif
