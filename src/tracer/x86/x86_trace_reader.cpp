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
#include "tracer/x86/utils/memory_trace_reader.hpp"
#include "utils/logging.hpp"

int tracer::SinucaTraceReader::OpenTrace(
    const char *imageName, const char *sourceDir) {

    StaticTraceFile staticFile(sourceDir, imageName);
    if (!staticFile.Valid()) return 1;

    this->binaryTotalBBLs = staticFile.GetTotalBBLs();
    this->numThreads = staticFile.GetNumThreads();

    this->thrsInfo = new ThrInfo[this->numThreads];

    for (int i = 0; i < this->numThreads; i++) {
        this->thrsInfo[i].isInsideBBL = false;
        if (this->thrsInfo[i].Allocate(sourceDir, imageName, i)) {
            return 1;
        }
    }

    this->GenerateBinaryDict(&staticFile);

    return 0;
}

void tracer::SinucaTraceReader::CloseTrace() {
    delete[] this->thrsInfo;
    delete[] this->binaryBBLsSize;
    delete[] this->pool;
    delete[] this->binaryDict;
}

unsigned long tracer::SinucaTraceReader::GetTraceSize() {
    return this->binaryTotalBBLs;
}

unsigned long tracer::SinucaTraceReader::GetNumberOfFetchedInstructions() {
    return this->fetchInstructions;
}

void tracer::SinucaTraceReader::GenerateBinaryDict(
    StaticTraceFile *stFile) {
    InstructionInfo *package;
    unsigned long poolOffset;
    unsigned int bblSize;
    unsigned int instCounter;
    unsigned int bblCounter;

    this->binaryBBLsSize = new unsigned int[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];
    this->pool = new InstructionInfo[stFile->GetTotalIns()];
    poolOffset = 0;

    for (bblCounter = 0; bblCounter < this->binaryTotalBBLs; bblCounter++) {
        bblSize = stFile->GetNewBBlSize();

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = &this->pool[poolOffset];
        poolOffset += bblSize;

        SINUCA3_DEBUG_PRINTF("Bbl [%u] Size [%u]\n", bblCounter + 1, bblSize);
        for (instCounter = 0; instCounter < bblSize; instCounter++) {
            package = &this->binaryDict[bblCounter][instCounter];
            stFile->ReadNextPackage(package);
        }
    }
}

FetchResult tracer::SinucaTraceReader::Fetch(InstructionPacket *ret,
                                                         unsigned int tid) {
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

    this->fetchInstructions++;

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret).staticInfo->opcodeAssembly);

    return FetchResultOk;  // Maybe this is not suitable for multiple threads
}

void tracer::SinucaTraceReader::PrintStatistics() {
    SINUCA3_LOG_PRINTF("###########################\n");
    SINUCA3_LOG_PRINTF("Sinuca3 Trace Reader\n");
    SINUCA3_LOG_PRINTF("Fetch Instructions:%lu\n", this->fetchInstructions);
    SINUCA3_LOG_PRINTF("###########################\n");
}

int tracer::ThrInfo::Allocate(const char *sourceDir, const char *imageName, int tid) {
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
    InstructionPacket package;
    traceReader::FetchResult ret;
    traceReader::TraceReader *tracer =
        new traceReader::tracer::SinucaTraceReader();
    tracer->OpenTrace("sinuca_teste", "");

    do {
        ret = tracer->Fetch(&package, 0);
        SINUCA3_DEBUG_PRINTF("");
    } while (ret == traceReader::FetchResultOk);

    delete tracer;
}
#endif
