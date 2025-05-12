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
#include <cstddef>

#include "../utils/logging.hpp"
#include "x86_reader_file_handler.hpp"
#include "trace_reader.hpp"

int sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::OpenTrace(
    const char *executableName, const char *traceFolderPath) {
    std::string folderPath = std::string(traceFolderPath);

    StaticTraceFile staticFile(folderPath, executableName);

    this->binaryTotalBBLs = staticFile.GetTotalBBLs();
    this->numThreads = staticFile.GetNumThreads();

    this->thrInfo = new struct ContextInfo[this->numThreads];
    this->threadsDynFiles = new DynamicTraceFile *[this->numThreads];
    this->threadsMemFiles = new MemoryTraceFile *[this->numThreads];

    for (int i = 0; i < this->numThreads; i++) {
        this->thrInfo[i].isInsideBBL = false;
        this->threadsDynFiles[i] =
            new DynamicTraceFile(folderPath, executableName, i);
        this->threadsMemFiles[i] =
            new MemoryTraceFile(folderPath, executableName, i);
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
    delete[] this->thrInfo;
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
    InstructionInfo *package;
    size_t poolOffset;
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

    return 0;
}

sinuca::traceReader::FetchResult
sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader::Fetch(
    InstructionPacket *ret, unsigned int tid) {
    InstructionInfo *packageInfo;

    if (!this->thrInfo[tid].isInsideBBL) {
        if (this->threadsDynFiles[tid]->ReadNextBBl(&this->thrInfo[tid].currentBBL)) {
            return FetchResultEnd;
        }
        this->thrInfo[tid].isInsideBBL = true;
        this->thrInfo[tid].currentOpcode = 0;
    }

    unsigned int currentBbl = this->thrInfo[tid].currentBBL;
    unsigned int currentIns = this->thrInfo[tid].currentOpcode;
    packageInfo = &this->binaryDict[currentBbl][currentIns];
    ret->staticInfo = &(packageInfo->staticInfo);
    this->threadsMemFiles[tid]->ReadNextMemAccess(packageInfo,
                                                  &ret->dynamicInfo);

    this->thrInfo[tid].currentOpcode++;
    if (this->thrInfo[tid].currentOpcode >=
        this->binaryBBLsSize[this->thrInfo[tid].currentBBL]) {
        this->thrInfo[tid].isInsideBBL = false;
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
    sinuca::InstructionPacket package;
    sinuca::traceReader::FetchResult ret;
    sinuca::traceReader::TraceReader *tracer =
        new sinuca::traceReader::sinuca3TraceReader::SinucaTraceReader();
    tracer->OpenTrace("sinuca_teste", "/home/fbc04/Programas/SiNUCA3/trace");

    do {
        ret = tracer->Fetch(&package, 0);
        SINUCA3_DEBUG_PRINTF("INS NAME [%s]\n", package.staticInfo->opcodeAssembly);
        SINUCA3_DEBUG_PRINTF("INS SIZE [%d]\n", package.staticInfo->opcodeSize);
        SINUCA3_DEBUG_PRINTF("NUM READ REGS [%d]\n", package.staticInfo->numReadRegs);
    } while (ret == sinuca::traceReader::FetchResultOk);

    delete tracer;
}
#endif
