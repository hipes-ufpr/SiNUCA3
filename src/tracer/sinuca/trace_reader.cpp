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
    this->traceFilesVersion = this->staticTrace->GetVersionInt();
    this->traceFilesTargetArch = this->staticTrace->GetTargetInt();

    SINUCA3_WARNING_PRINTF("Trace files:\n");
    SINUCA3_WARNING_PRINTF("\t Version: %d\n", this->traceFilesVersion);
    SINUCA3_WARNING_PRINTF("\t Target: %s\n", staticTrace->GetTargetString());

    for (int i = 0; i < this->totalThreads; ++i) {
        ThreadData *tData = new ThreadData;
        if (tData == NULL) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] failed to alloc ThreadData!\n");
            return 1;
        }
        
        this->threadDataVec.push_back(tData);
        
        if (tData->Allocate(sourceDir, imageName, i)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] tData Allocate method failed!\n");
            return 1;
        }
        if (tData->CheckVersion(this->traceFilesVersion)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] incompatible version!\n");
            return 1;
        }
        if (tData->CheckTargetArch(this->traceFilesTargetArch)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] incompatible target!\n");
            return 1;
        }
    }

    this->reachedAbruptEnd = false;

    if (this->GenerateInstructionDict()) {
        SINUCA3_ERROR_PRINTF("[OpenTrace] Failed to generate instruction "
            "dictionary\n");
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
    StaticTraceRecordType recordType;

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
            this->staticTrace->TranslateRawInstructionToSinucaInst(instInfoPtr);
        }
    }

    return 0;
}

bool SinucaTraceReader::HasExecutionEnded() {
    if (this->reachedAbruptEnd) return true;

    if (this->threadDataVec[0]->dynFile.HasReachedEnd()) {
        for (int i = 1; i < this->totalThreads; ++i) {
            if (!this->threadDataVec[i]->dynFile.HasReachedEnd()) {
                SINUCA3_ERROR_PRINTF("Thread [%d] file hasnt reached end!\n", i);
            }
        }
        return true;
    }
    return false;
}

FetchResult SinucaTraceReader::Fetch(InstructionPacket *ret, int tid) {
    if (this->HasExecutionEnded()) {
        return FetchResultEnd;
    }
    if (this->threadDataVec[tid]->dynFile.HasReachedEnd()) {
        return FetchResultNop;           
    }
    if (this->IsThreadSleeping(tid)) {
        return FetchResultNop;
    }

    /* Detect need to fetch new basic block */
    if (!this->threadDataVec[tid]->isInsideBasicBlock) {
        this->threadDataVec[tid]->currentInst = 0;
        if (this->FetchBasicBlock(tid)) {
            if (this->fetchFailed) {
                return FetchResultError;
            } else {
                return FetchResultNop;
            }
        }
        this->threadDataVec[tid]->isInsideBasicBlock = true;
    }

    this->ResetInstructionPacket(ret);

    ret->staticInfo =
        &this->instructionDict[this->threadDataVec[tid]->currentBasicBlock]
                              [this->threadDataVec[tid]->currentInst];
    if (ret->staticInfo->instReadsMemory || ret->staticInfo->instWritesMemory) {
        if (this->FetchMemoryData(ret, tid)) {
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

int SinucaTraceReader::FetchMemoryData(InstructionPacket *ret, int tid) {
    if (this->threadDataVec[tid]->memFile.HasReachedEnd()) {
        SINUCA3_ERROR_PRINTF(
            "[FetchMemoryData] should have reached end in dynamic trace file "
            "first!\n");
        return 1;
    }

    if (this->threadDataVec[tid]->memFile.ReadMemoryOperations(ret)) {
        SINUCA3_ERROR_PRINTF("[FetchMemoryData] failed to read mem ops!\n");
        return 1;
    }

    return 0;
}

int SinucaTraceReader::FetchBasicBlock(int tid) {
    if (this->threadDataVec[tid]->dynFile.ReadDynamicRecord()) {
        if (this->threadDataVec[tid]->dynFile.HasReachedEnd()) {
            SINUCA3_DEBUG_PRINTF("[FetchBasicBlock] thread [%u] file reached "
                                    "end!\n", tid);
        } else {
            this->fetchFailed = true;
        }
        return 1;
    }

    static int criticalCont = 0;
    static int barrierCont = 0;

    DynamicTraceRecordType recType = this->threadDataVec[tid]->dynFile
        .GetRecordType();

    while (recType == DynamicRecordThreadEvent) {
        ThreadEventType evType = this->threadDataVec[tid]->dynFile
            .GetThreadEvent();

        SINUCA3_DEBUG_PRINTF("[FetchBasicBlock] Fetched thread event [%u] in "
            "thread [%d]\n", evType, tid);

        if (evType == ThreadEventAbruptEnd) {
            this->reachedAbruptEnd = true;
            SINUCA3_WARNING_PRINTF(
                "Trace reader fetched abrupt end event in thread [%d]!\n", tid);
            return 1; // no basic block to fetch
        } else if (evType == ThreadEventCriticalStart) {
            criticalCont++;
            SINUCA3_DEBUG_PRINTF("Critical region found in thread [%u] and "
                "criticalCont is [%d]\n", tid, criticalCont);
            for (int i = 0; i < this->totalThreads; i++) {
                if (i == tid) continue;
                this->threadDataVec[i]->isThreadAwake = false;
            }
            
        } else if (evType == ThreadEventCriticalEnd) {
            criticalCont--;
            if (criticalCont == 0) {
                SINUCA3_DEBUG_PRINTF("End of critical region. Waking up all "
                    "threads!\n");
                for (int i = 0; i < this->totalThreads; i++) {
                    this->threadDataVec[i]->isThreadAwake = true;
                }
            } else if (criticalCont < 0) {
                SINUCA3_ERROR_PRINTF("[FetchBasicBlock] criticalCont is "
                    "negative!\n");
                this->fetchFailed = true;
            }
        } else if (evType == ThreadEventBarrierSync) {
            barrierCont++;
            if (barrierCont == this->totalThreads) {
                SINUCA3_DEBUG_PRINTF("[FetchBasicBlock] Threads reached barrier"
                    " sync. Waking up all threads!\n");
                for (int i = 0; i < this->totalThreads; i++) {
                    this->threadDataVec[i]->isThreadAwake = true;
                }
                barrierCont = 0;
            } else {
                this->threadDataVec[tid]->isThreadAwake = false;
                return 1; // no basic block to fetch
            }
        } else {
            SINUCA3_ERROR_PRINTF("[FetchBasicBlock] Unkown thread event [%d]!\n", 
                evType);
            return 1;
        }

        if (this->threadDataVec[tid]->dynFile.ReadDynamicRecord()) {
            if (this->threadDataVec[tid]->dynFile.HasReachedEnd()) {
                SINUCA3_DEBUG_PRINTF("[FetchBasicBlock] thread [%u] file "
                                        "reached end!\n", tid);
            } else {
                this->fetchFailed = true;
            }
            return 1;
        }
        recType = this->threadDataVec[tid]->dynFile.GetRecordType();
    }

    if (recType != DynamicRecordBasicBlockIdentifier) {
        SINUCA3_ERROR_PRINTF("[FetchBasicBlock] not expected rec type [%u]\n",
            recType);
        this->fetchFailed = true;
        return 1;
    }

    unsigned int bblIndex =
        this->threadDataVec[tid]->dynFile.GetBasicBlockIdentifier();
    this->threadDataVec[tid]->currentBasicBlock = bblIndex;

    SINUCA3_DEBUG_PRINTF("Bbl fetched is [%d] and it has [%d] inst\n", bblIndex,
                         this->basicBlockSizeArr[bblIndex]);

    return 0;
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
    FetchResult res;

    while (1) {
        for (int i = 0; i < reader->GetTotalThreads(); i++) {
            SINUCA3_DEBUG_PRINTF("\n");
            SINUCA3_DEBUG_PRINTF("Fetching for thread [%d]: \n", i);

            res = reader->Fetch(&instPkt, i);

            if (res == FetchResultNop) {
                SINUCA3_DEBUG_PRINTF("\t Thread [%d] returned NOP!\n", i);
                continue;
            }
            if (res == FetchResultError) {
                SINUCA3_DEBUG_PRINTF("\t Thread [%d] fetch failed!\n", i);
                break;
            }
            if (res == FetchResultEnd) {
                SINUCA3_DEBUG_PRINTF("\t FetchResultEnd got in thr [%d]!\n", i);
                break;
            }

            SINUCA3_DEBUG_PRINTF("\t Instruction mnemonic is [%s]\n",
                                 instPkt.staticInfo->instMnemonic);
            SINUCA3_DEBUG_PRINTF("\t Instruction size is [%ld]\n",
                                 instPkt.staticInfo->instSize);
            SINUCA3_DEBUG_PRINTF("\t Instruction address is [%p]\n",
                                 (void *)instPkt.staticInfo->instAddress);
            SINUCA3_DEBUG_PRINTF("\t Store regs total [%d]\n",
                                 instPkt.staticInfo->numberOfWriteRegs);
            SINUCA3_DEBUG_PRINTF("\t Load regs total [%d]\n",
                                 instPkt.staticInfo->numberOfReadRegs);
            SINUCA3_DEBUG_PRINTF("\t Store mem total ops [%d]\n",
                                 instPkt.dynamicInfo.numWritings);
            SINUCA3_DEBUG_PRINTF("\t Load mem total ops [%d]\n",
                                 instPkt.dynamicInfo.numReadings);
            SINUCA3_DEBUG_PRINTF("\t Instruction is [%s]\n",
                                 instPkt.staticInfo->instMnemonic);

            if (instPkt.staticInfo->branchType == BranchCall) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchCall\n");
            } else if (instPkt.staticInfo->branchType == BranchSyscall) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchSyscall\n");
            } else if (instPkt.staticInfo->branchType == BranchCond) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchCond\n");
            } else if (instPkt.staticInfo->branchType == BranchUncond) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchUncond\n");
            } else if (instPkt.staticInfo->branchType == BranchRet) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchRet\n");
            } else if (instPkt.staticInfo->branchType == BranchSysret) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchSysret\n");
            } else if (instPkt.staticInfo->branchType == BranchNone) {
                SINUCA3_DEBUG_PRINTF("\t Branch type is BranchNone\n");
            } else {
                SINUCA3_DEBUG_PRINTF("\t Unkown branch type!\n");
                return 1;
            }
        }

        if (res == FetchResultError || res == FetchResultEnd) {
            break;
        }
    }

    delete reader;

    return (res != FetchResultEnd);
}
#endif
