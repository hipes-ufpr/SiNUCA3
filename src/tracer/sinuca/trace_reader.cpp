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

    this->threadDataArr = new ThreadData *[this->totalThreads];
    for (int i = 0; i < this->totalThreads; ++i) {
        ThreadData *tData = new ThreadData;
        if (tData->Allocate(sourceDir, imageName, i)) {
            SINUCA3_ERROR_PRINTF("OpenTrace failed to allocate tData!\n");
            return 1;
        }
        this->threadDataArr[i] = tData;
    }

    this->threadDataArr[0]->isThreadAwake = true;
    this->SetNewLock(&this->globalLock);
    this->SetNewBarrier(&this->globalBarrier);

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

bool SinucaTraceReader::HasExecutionEnded() {
    if (this->threadDataArr[0]->dynFile.ReachedDynamicTraceEnd()) {
        for (int i = 1; i < this->totalThreads; ++i) {
            if (this->threadDataArr[i]->isThreadAwake) {
                SINUCA3_ERROR_PRINTF(
                    "Thread [%d] is awake while thread [0] execution already "
                    "ended!\n",
                    i);
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

    if (!this->threadDataArr[tid]->isInsideBasicBlock) {
        this->threadDataArr[tid]->currentInst = 0;
        if (this->FetchBasicBlock(tid)) {
            return FetchResultError;
        }
        if (!this->threadDataArr[tid]->isThreadAwake) {
            return FetchResultNop;
        }
    }

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

int SinucaTraceReader::FetchMemoryData(InstructionPacket* ret, int tid) {
    unsigned long arraySize;
    if (this->threadDataArr[tid]->memFile.ReadMemoryOperations()) {
        SINUCA3_ERROR_PRINTF("Failed to read memory operations\n");
        return 1;
    }

    ret->dynamicInfo.numReadings =
        this->threadDataArr[tid]->memFile.GetNumberOfLoads();
    ret->dynamicInfo.numWritings =
        this->threadDataArr[tid]->memFile.GetNumberOfStores();

    arraySize = sizeof(ret->dynamicInfo.readsAddr) / sizeof(unsigned long);

    if (this->threadDataArr[tid]->memFile.CopyLoadOperations(
            ret->dynamicInfo.readsAddr, arraySize,
            ret->dynamicInfo.readsSize, arraySize)) {
        SINUCA3_ERROR_PRINTF("Failed to copy load operations!\n");
        return 1;
    }
    if (this->threadDataArr[tid]->memFile.CopyStoreOperations(
            ret->dynamicInfo.writesAddr, arraySize,
            ret->dynamicInfo.writesSize, arraySize)) {
        SINUCA3_ERROR_PRINTF("Failed to copy store operations!\n");
        return 1;
    }

    return 0;
}

int SinucaTraceReader::FetchBasicBlock(int tid) {
    this->threadDataArr[tid]->dynFile.ReadDynamicRecord();
    int recordType = this->threadDataArr[tid]->dynFile.GetRecordType();

    while (recordType != DynamicRecordBasicBlockIdentifier) {
        if (recordType != DynamicRecordThreadEvent) {
            SINUCA3_ERROR_PRINTF("Type is not thread event!\n");
            return 1;
        }

        int eventType = this->threadDataArr[tid]->dynFile.GetEventType();
        if (eventType == ThreadEventCreateThread) {
            if (tid != 0) {
                SINUCA3_ERROR_PRINTF(
                    "Thread [%d] is not expected to create new threads!\n",
                    tid);
                return 1;
            }
            this->numberOfActiveThreads++;
            this->threadDataArr[this->numberOfActiveThreads - 1]
                ->parentThreadId = 0;
            this->threadDataArr[this->numberOfActiveThreads - 1]
                ->isThreadAwake = true;
        } else if (eventType == ThreadEventDestroyThread) {
            if (tid != 0) {
                SINUCA3_ERROR_PRINTF(
                    "Expected destroy event to be called in thread [0]!\n");
                return 1;
            }
            if (this->numberOfActiveThreads > 1) {
                this->threadDataArr[0]->isThreadAwake = false;
                this->numberOfActiveThreads--;
                return 0; /* no basic blocks to be fetched. */
            }
            this->numberOfActiveThreads = 1;
        } else if (eventType == ThreadEventHaltThread) {
            if (tid == 0) {
                SINUCA3_ERROR_PRINTF(
                    "Thread [0] should not have thread halt event!\n");
                return 1;
            }
            this->threadDataArr[tid]->isThreadAwake = false;
            this->numberOfActiveThreads--;
            if (this->numberOfActiveThreads == 0) {
                /* wake parent thread */
                this->threadDataArr[this->threadDataArr[tid]->parentThreadId]
                    ->isThreadAwake = true;
                this->numberOfActiveThreads++;
            }
        } else if (eventType == ThreadEventLockRequest) {
            if (this->threadDataArr[tid]->dynFile.IsGlobalLock()) {
                if (this->globalLock.isBusy) {
                    this->threadDataArr[tid]->isThreadAwake = false;
                    this->globalLock.waitingThreadsQueue.Enqueue(&tid);
                    return 0; /* no basic block to be fetched. */
                } else {
                    this->globalLock.isBusy = true;
                    this->globalLock.owner = tid;
                }
            } else {
                unsigned long lockAddr =
                    this->threadDataArr[tid]->dynFile.GetLockAddress();

                Lock *lock = NULL;
                for (unsigned long i = 0; i < privateLockVec.size(); ++i) {
                    if (this->privateLockVec[i].addr == lockAddr) {
                        lock = &this->privateLockVec[i];
                    }
                }
                if (lock == NULL) {
                    Lock newLock;
                    this->SetNewLock(&newLock);
                    this->privateLockVec.push_back(newLock);
                    this->privateLockVec.back().addr = lockAddr;
                    lock = &newLock;
                }

                if (lock->isBusy) {
                    if (!this->threadDataArr[tid]->dynFile.IsTestLock()) {
                        if (this->threadDataArr[tid]->dynFile.IsNestedLock() &&
                            lock->owner == tid) {
                            lock->recCont++;
                        } else {
                            this->threadDataArr[tid]->isThreadAwake = false;
                            this->globalLock.waitingThreadsQueue.Enqueue(&tid);
                            return 0; /* no basic block to be fetched */
                        }
                    }
                } else {
                    lock->isBusy = true;
                    lock->owner = tid;
                    if (this->threadDataArr[tid]->dynFile.IsNestedLock()) {
                        lock->recCont = 1;
                    }
                }
            }
        } else if (eventType == ThreadEventUnlockRequest) {
            if (this->threadDataArr[tid]->dynFile.IsGlobalLock()) {
                if (!this->globalLock.isBusy) {
                    SINUCA3_ERROR_PRINTF("Expected global lock to be busy!\n");
                    return 1;
                }
                if (this->globalLock.owner != tid) {
                    SINUCA3_ERROR_PRINTF(
                        "Thread [%d] is not owner of global lock and is trying "
                        "to unlock!\n",
                        tid);
                    return 1;
                }

                if (!this->globalLock.waitingThreadsQueue.IsEmpty()) {
                    int sleepingThreadId;
                    this->globalLock.waitingThreadsQueue.Dequeue(
                        &sleepingThreadId);
                    this->threadDataArr[sleepingThreadId]->isThreadAwake = true;
                    /* changing to new global lock owner. */
                    this->globalLock.owner = sleepingThreadId;
                } else {
                    this->ResetLock(&this->globalLock);
                }
            } else {
                unsigned long lockAddr =
                    this->threadDataArr[tid]->dynFile.GetLockAddress();

                Lock *lock = NULL;
                for (unsigned long i = 0; i < privateLockVec.size(); ++i) {
                    if (this->privateLockVec[i].addr == lockAddr) {
                        lock = &this->privateLockVec[i];
                    }
                }
                if (lock == NULL) {
                    SINUCA3_ERROR_PRINTF(
                        "Private lock with address [%ld] was not created!\n",
                        lockAddr);
                    return 1;
                }

                if (!lock->isBusy) {
                    SINUCA3_ERROR_PRINTF("Private lock expected to be busy!\n");
                    return 1;
                }
                if (lock->owner != tid) {
                    SINUCA3_ERROR_PRINTF(
                        "Thread [%d] is not owner of priv lock and is trying "
                        "to unlock!\n",
                        tid);
                    return 1;
                }

                bool isReadyToChangeLockOwnership = true;
                if (this->threadDataArr[tid]->dynFile.IsNestedLock()) {
                    if (lock->recCont > 1) {
                        isReadyToChangeLockOwnership = false;
                    }
                    lock->recCont--;
                }

                if (isReadyToChangeLockOwnership) {
                    if (!lock->waitingThreadsQueue.IsEmpty()) {
                        int sleepingThreadId;
                        lock->waitingThreadsQueue.Dequeue(&sleepingThreadId);
                        this->threadDataArr[sleepingThreadId]->isThreadAwake =
                            true;
                        /* changing to new priv lock owner. */
                        lock->owner = sleepingThreadId;
                    } else {
                        this->ResetLock(lock);
                    }
                }
            }
        } else if (eventType == ThreadEventBarrier) {
            int waitingThreads = this->globalBarrier.thrCont;
            this->globalBarrier.thrCont++;

            if (waitingThreads < this->numberOfActiveThreads) {
                this->threadDataArr[tid]->isThreadAwake = false;
                return 0; /* no basic block to be fetched. */
            } else if (waitingThreads == this->numberOfActiveThreads) {
                this->ResetBarrier(&this->globalBarrier);
                /* wake all sleeping threads. */
                for (int i = 0; i < this->numberOfActiveThreads; i++) {
                    this->threadDataArr[i]->isThreadAwake = true;
                }
            } else {
                SINUCA3_ERROR_PRINTF(
                    "Global barrier thread counter is not expected to be "
                    "greater than the current number of active threds!\n");
                return 1;
            }
        } else {
            SINUCA3_ERROR_PRINTF("Unkown thread event [%d]!\n", eventType);
            return 1;
        }

        this->threadDataArr[tid]->dynFile.ReadDynamicRecord();
        recordType = this->threadDataArr[tid]->dynFile.GetRecordType();
    }

    this->threadDataArr[tid]->currentBasicBlock =
        this->threadDataArr[tid]->dynFile.GetBasicBlockIdentifier();

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
    FetchResult res = reader->Fetch(&instPkt, 0);
    while (1) {
        for (int i = 0; i < reader->GetTotalThreads(); i++) {
        }
    }

    if (res == FetchResultError) {
        SINUCA3_ERROR_PRINTF("Fetch result error!\n");
    }

    delete reader;

    return 0;
}
#endif
