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

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>  // FILE*

#include "../utils/logging.hpp"
#include "reader_file_handler.hpp"
#include "trace_reader.hpp"

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *executableName, const char *traceFolderPath) {
    std::string folderPath = std::string(traceFolderPath);
    std::string staticFilePath;

    staticFilePath = folderPath + "static_" + executableName + ".trace";
    StaticTraceFile staticFile(staticFilePath.c_str());

    this->binaryTotalBBLs = staticFile.GetTotalBBLs();
    this->numThreads = staticFile.GetNumThreads();

    this->isInsideBBL = new bool[this->numThreads];
    this->currentBBL = new unsigned int[this->numThreads];
    this->currentOpcode = new unsigned int[this->numThreads];
    this->threadsDynFiles = new DynamicTraceFile *[this->numThreads];
    this->threadsMemFiles = new MemoryTraceFile *[this->numThreads];

    for (int i = 0; i < this->numThreads; i++) {
        this->isInsideBBL[i] = false;
        this->threadsDynFiles[i] =
            new DynamicTraceFile(executableName, i, traceFolderPath);
        this->threadsMemFiles[i] =
            new MemoryTraceFile(executableName, i, traceFolderPath);
    }

    if (this->GenerateBinaryDict(&staticFile)) return 1;

    return 0;
}

void sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::CloseTrace() {
    for (int i = 0; i < this->numThreads; i++) {
        delete (this->threadsDynFiles[i]);
        delete (this->threadsMemFiles[i]);
    }

    delete[] this->threadsDynFiles;
    delete[] this->threadsMemFiles;
    delete[] this->currentBBL;
    delete[] this->currentOpcode;
    delete[] this->isInsideBBL;
    delete[] this->binaryBBLsSize;
    delete[] this->pool;
    delete[] this->binaryDict;
}

unsigned long
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::GetTraceSize() {
    return this->binaryTotalBBLs;
}

unsigned long sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GetNumberOfFetchedInstructions() {
    return this->fetchInstructions;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    GenerateBinaryDict(StaticTraceFile *stFile) {
    unsigned int bblSize;
    unsigned int instCounter;
    unsigned int bblCounter;
    InstructionInfo *package;
    size_t pool_offset;

    this->binaryBBLsSize = new unsigned int[this->binaryTotalBBLs];
    this->binaryDict = new InstructionInfo *[this->binaryTotalBBLs];
    this->pool = new InstructionInfo[stFile->GetTotalIns()];
    pool_offset = 0;

    for (bblCounter = 0; bblCounter < this->binaryTotalBBLs; bblCounter++) {
        bblSize = stFile->GetNewBBlSize();
        SINUCA3_DEBUG_PRINTF("Bbl %u Size => %u\n", bblCounter + 1, bblSize);

        this->binaryBBLsSize[bblCounter] = bblSize;
        this->binaryDict[bblCounter] = &this->pool[pool_offset];
        pool_offset += bblSize;

        for (instCounter = 0; instCounter < bblSize; instCounter++) {
            package = &this->binaryDict[bblCounter][instCounter];
            stFile->ReadNextPackage(package);
        }
    }

    return 0;
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::
    TraceNextDynamic(unsigned int *nextBbl, unsigned int tid) {
    return this->threadsDynFiles[tid]->ReadNextBBl(nextBbl);
}

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::TraceNextMemory(
    InstructionPacket *ret, InstructionInfo *packageInfo, unsigned int tid) {
    return this->threadsMemFiles[tid]->ReadNextMemAccess(packageInfo, ret);
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    InstructionPacket *ret, unsigned int tid) {
    if (!this->isInsideBBL[tid]) {
        if (this->TraceNextDynamic(&this->currentBBL[tid], tid)) {
            SINUCA3_DEBUG_PRINTF("Fetch ended!\n");
            return FetchResultEnd;  // Maybe this is not suitable for multiple
                                    // threads
        }
        this->isInsideBBL[tid] = true;
        this->currentOpcode[tid] = 0;
    }

    InstructionInfo *packageInfo =
        &this->binaryDict[this->currentBBL[tid]][this->currentOpcode[tid]];
    (*ret).staticInfo = &(packageInfo->staticInfo);
    this->TraceNextMemory(ret, packageInfo, tid);

    this->currentOpcode[tid]++;
    if (this->currentOpcode[tid] >=
        this->binaryBBLsSize[this->currentBBL[tid]]) {
        this->isInsideBBL[tid] = false;
    }

    this->fetchInstructions++;

    SINUCA3_DEBUG_PRINTF("Fetched: %s\n", (*ret).staticInfo->opcodeAssembly);

    return FetchResultOk;  // Maybe this is not suitable for multiple threads
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
    tracer->OpenTrace("test", "./");
    delete tracer;
}
#endif
