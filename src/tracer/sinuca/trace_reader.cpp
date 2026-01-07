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

    this->threadDataArr = new ThreadData *[this->totalThreads];
    for (int i = 0; i < this->totalThreads; ++i) {
        ThreadData *tData = new ThreadData;
        if (tData == NULL) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] failed to alloc tData!\n");
            return 1;
        }
        if (tData->Allocate(sourceDir, imageName, i)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] tData Allocate method failed!\n");
            return 1;
        }
        this->threadDataArr[i] = tData;

        if (tData->CheckVersion(this->traceFilesVersion)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] incompatible version!\n");
            return 1;
        }
        if (tData->CheckTargetArch(this->traceFilesTargetArch)) {
            SINUCA3_ERROR_PRINTF("[OpenTrace] incompatible target!\n");
            return 1;
        }
    }

    this->threadDataArr[0]->isThreadAwake = true;
    this->reachedAbruptEnd = false;

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
            this->staticTrace->TranslateRawInstructionToSinucaInst(instInfoPtr);
        }
    }

    return 0;
}

bool SinucaTraceReader::HasExecutionEnded() {
    if (this->reachedAbruptEnd) return true;

    if (this->threadDataArr[0]->dynFile.HasReachedEnd()) {
        for (int i = 1; i < this->totalThreads; ++i) {
            if (this->threadDataArr[i]->isThreadAwake) {
                SINUCA3_ERROR_PRINTF("Thread [%d] should be sleeping!\n", i);
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
    if (this->IsThreadSleeping(tid)) {
        return FetchResultNop;
    }

    /* Detect need to fetch new basic block */
    if (!this->threadDataArr[tid]->isInsideBasicBlock) {
        this->threadDataArr[tid]->currentInst = 0;
        if (this->FetchBasicBlock(tid)) {
            if (this->fetchFailed) {
                return FetchResultError;
            } else {
                return FetchResultNop;
            }
        }
        this->threadDataArr[tid]->isInsideBasicBlock = true;
    }

    this->ResetInstructionPacket(ret);

    ret->staticInfo =
        &this->instructionDict[this->threadDataArr[tid]->currentBasicBlock]
                              [this->threadDataArr[tid]->currentInst];
    if (ret->staticInfo->instReadsMemory || ret->staticInfo->instWritesMemory) {
        if (this->FetchMemoryData(ret, tid)) {
            return FetchResultError;
        }
    }

    ++this->threadDataArr[tid]->currentInst;
    if (this->threadDataArr[tid]->currentInst >=
        this->basicBlockSizeArr[this->threadDataArr[tid]->currentBasicBlock]) {
        this->threadDataArr[tid]->isInsideBasicBlock = false;
    }

    this->threadDataArr[tid]->fetchedInst++;

    return FetchResultOk;
}

int SinucaTraceReader::FetchMemoryData(InstructionPacket *ret, int tid) {
    if (this->threadDataArr[tid]->memFile.HasReachedEnd()) {
        SINUCA3_ERROR_PRINTF(
            "[FetchMemoryData] should have reached end in dynamic trace file "
            "first!\n");
        return 1;
    }

    if (this->threadDataArr[tid]->memFile.ReadMemoryOperations(ret)) {
        SINUCA3_ERROR_PRINTF("[FetchMemoryData] failed to read mem ops!\n");
        return 1;
    }

    return 0;
}

int SinucaTraceReader::FetchBasicBlock(int tid) {
    if (this->threadDataArr[tid]->dynFile.ReadDynamicRecord()) return 1;

    int recordType = this->threadDataArr[tid]->dynFile.GetRecordType();

    while (recordType != DynamicRecordBasicBlockIdentifier) {
        if (recordType == DynamicRecordAbruptEnd) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordAbruptEnd!\n");

            this->reachedAbruptEnd = true;
            SINUCA3_WARNING_PRINTF(
                "Trace reader fetched abrupt end event in thread [%d]!\n", tid);
            return 1;
        } else if (recordType == DynamicRecordCreateThread) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordCreateThread!\n");

            /* Currently there is no support for nested parallelism */
            if (tid != 0) {
                SINUCA3_ERROR_PRINTF(
                    "Thr [%d] is not expected to create new threads!\n", tid);
                this->fetchFailed = true;
                return 1;
            }

            /* Loop activates all threads. */
            for (int i = 1; i < this->totalThreads; i++) {
                this->threadDataArr[i]->parentThreadId = 0;
                this->threadDataArr[i]->isThreadAwake = true;
                this->numberOfActiveThreads++;
            }
        } else if (recordType == DynamicRecordDestroyThread) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordDestroyThread!\n");

            if (tid != 0) {
                SINUCA3_ERROR_PRINTF(
                    "Thr [%d] is not expected to destroy threads!\n", tid);
                this->fetchFailed = true;
                return 1;
            }

            /* Loop destroys all threads. */
            for (int i = 1; i < this->totalThreads; i++) {
                this->threadDataArr[i]->isThreadAwake = false;
                this->numberOfActiveThreads--;
            }
        } else if (recordType == DynamicRecordLockRequest) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordLockRequest!\n");

            unsigned long lockAddr =
                this->threadDataArr[tid]->dynFile.GetLockAddress();

            /* Check if lock with corresponding lockAddr was created. If
             * not, create a new lock and append to vector. */
            Lock *lock = NULL;
            for (unsigned long i = 0; i < mutexVec.size(); ++i) {
                if (this->mutexVec[i].addr == lockAddr) {
                    lock = &this->mutexVec[i];
                }
            }

            if (lock == NULL) {
                Lock newLock;
                newLock.addr = lockAddr;
                this->mutexVec.push_back(newLock);
                lock = &mutexVec.back();
            }

            if (lock->isBusy) {
                this->threadDataArr[tid]->isThreadAwake = false;
                lock->waitingThreadsQueue->Enqueue(&tid);
                return 1; /* no basic block to be fetched */
            } else {
                lock->isBusy = true;
                lock->owner = tid;
            }
        } else if (recordType == DynamicRecordUnlockRequest) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordUnlockRequest!\n");

            unsigned long lockAddr =
                this->threadDataArr[tid]->dynFile.GetLockAddress();

            /* Look for lock with corresponding lockAddr. */
            Lock *lock = NULL;
            for (unsigned long i = 0; i < mutexVec.size(); ++i) {
                if (this->mutexVec[i].addr == lockAddr) {
                    lock = &this->mutexVec[i];
                }
            }

            if (lock == NULL) {
                SINUCA3_ERROR_PRINTF("Lock with addr [%ld] wasnt created\n",
                                        lockAddr);
                this->fetchFailed = true;
                return 1;
            }

            if (!lock->isBusy) {
                SINUCA3_ERROR_PRINTF("Lock with addr [%ld] isnt busy!\n",
                                        lockAddr);
                this->fetchFailed = true;
                return 1;
            }

            if (lock->owner != tid) {
                SINUCA3_ERROR_PRINTF(
                    "Thr [%d] isnt the current owner of lock with addr "
                    "[%ld]!\n",
                    tid, lockAddr);
                this->fetchFailed = true;
                return 1;
            }

            if (!lock->waitingThreadsQueue->IsEmpty()) {
                int sleepingThr;
                lock->waitingThreadsQueue->Dequeue(&sleepingThr);
                this->threadDataArr[sleepingThr]->isThreadAwake = true;
                /* Change to new lock owner. */
                lock->owner = sleepingThr;
            } else {
                /* Lock is free. */
                lock->ResetLock();
            }
        } else if (recordType == DynamicRecordBarrier) {
            SINUCA3_DEBUG_PRINTF("Fetched DynamicRecordBarrier!\n");

            this->globalBarrier.thrCont++;

            if (this->globalBarrier.thrCont < this->totalThreads) {
                this->threadDataArr[tid]->isThreadAwake = false;
                return 1; /* no basic block to be fetched. */
            } else if (this->globalBarrier.thrCont == this->totalThreads) {
                this->globalBarrier.ResetBarrier();
                /* Wake all sleeping threads. */
                for (int i = 0; i < this->totalThreads; i++) {
                    this->threadDataArr[i]->isThreadAwake = true;
                }
            } else {
                SINUCA3_ERROR_PRINTF(
                    "Barrier thread counter should not be greater than the "
                    "total of threads!\n");
                this->fetchFailed = true;
                return 1;
            }
        } else {
            SINUCA3_ERROR_PRINTF("Unkown dynamic record [%d]!\n", recordType);
            return 1;
        }

        if (this->threadDataArr[tid]->dynFile.ReadDynamicRecord()) {
            this->fetchFailed = true;
            return 1;
        }
        recordType = this->threadDataArr[tid]->dynFile.GetRecordType();
    }

    unsigned int bblIndex =
        this->threadDataArr[tid]->dynFile.GetBasicBlockIdentifier();
    this->threadDataArr[tid]->currentBasicBlock = bblIndex;

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
    while (1) {
        for (int i = 0; i < reader->GetTotalThreads(); i++) {
            SINUCA3_DEBUG_PRINTF("\n");
            SINUCA3_DEBUG_PRINTF("Fetching for thread [%d]: \n", i);

            FetchResult res = reader->Fetch(&instPkt, i);

            if (res == FetchResultNop) {
                SINUCA3_DEBUG_PRINTF("\t Thread [%d] returned NOP!\n", i);
                continue;
            }
            if (res == FetchResultError) {
                SINUCA3_DEBUG_PRINTF("\t Thread [%d] fetch failed!\n", i);
                return 1;
            }
            if (res == FetchResultEnd) {
                SINUCA3_DEBUG_PRINTF("\t FetchResultEnd got in thr [%d]!\n", i);
                return 0;
            }

            SINUCA3_DEBUG_PRINTF("\t Instruction mnemonic is [%s]\n",
                                 instPkt.staticInfo->instMnemonic);
            SINUCA3_DEBUG_PRINTF("\t Instruction size is [%ld]\n",
                                 instPkt.staticInfo->instSize);
            SINUCA3_DEBUG_PRINTF("\t Instruction address is [%p]\n",
                                 (void *)instPkt.staticInfo->instAddress);
            SINUCA3_DEBUG_PRINTF("\t Effective addr width [%d]\n",
                                 instPkt.staticInfo->effectiveAddressWidth);
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

            SINUCA3_DEBUG_PRINTF("\n");
        }
    }

    delete reader;

    return 0;
}
#endif
