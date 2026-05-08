//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file lsu.cpp
 */

#include "lsu.hpp"

#include "engine/default_packets.hpp"
#include "utils/logger.hpp"
#include "utils/pair.hpp"

void LoadStoreUnit::PrintStatistics() {
    SINUCA3_LOG_PRINTF("=== LOAD STORE UNIT STATISTICS ===\n");
    SINUCA3_LOG_PRINTF("Total load requests: %d\n", this->requestedLoads);
    SINUCA3_LOG_PRINTF("Total store requests: %d\n", this->requestedStores);
    SINUCA3_LOG_PRINTF("Completed store requests: %d\n", this->finishedStores);
    SINUCA3_LOG_PRINTF("Completed load requests: %d\n", this->finishedLoads);
    SINUCA3_LOG_PRINTF("Pending loads: %d\n",
                       this->waitingLoads.GetOccupation());
    SINUCA3_LOG_PRINTF("Pending stores: %d\n",
                       this->waitingStores.GetOccupation());
}

int LoadStoreUnit::Configure(Config config) {
    if (config.ComponentReference("cache", &this->cache, true))
        return config.Error("cache", "missing required component reference");
    if (config.ComponentReference("tlb", &this->tlb, true))
        return config.Error("tlb", "missing required component reference");
    if (config.ComponentReference("sendTo", &this->sendTo, true))
        return config.Error("sendTo", "missing required component reference");

    /* Adjustable configuration knobs */
    config.Integer("storeBufferSize", &this->stBufferSize, false);
    config.Bool("loadBypassingEnabled", &this->ldBypassingEnabled, false);
    config.Bool("loadForwardingEnabled", &this->ldForwardingEnabled, false);

    this->connIds[TLB_SOLVE_LOAD_ADDRESS] = this->tlb->Connect(0);
    this->connIds[TLB_SOLVE_STORE_ADDRESS] = this->tlb->Connect(0);
    this->connIds[CACHE_SOLVE_LOAD_DATA] = this->cache->Connect(0);
    this->connIds[CACHE_SOLVE_STORE_DATA] = this->cache->Connect(0);
    this->connIds[SEND_TO] = this->sendTo->Connect(0);

    return 0;
}

void LoadStoreUnit::Clock() {
    this->ReceiveResponses();
    this->ReceiveRequests();
    this->RunPipeline();
}

void LoadStoreUnit::ReceiveResponses() {
    this->ReceiveFromCache();
    this->ReceiveFromTlb();
    this->ReceiveFromRob();
}

void LoadStoreUnit::ReceiveRequests() { this->ReceiveFromScheduler(); }

void LoadStoreUnit::RunPipeline() {
    this->ResetStallSignals();
    this->ClearNext();

    /*
     * Evaluate stages backwards so stalls/backpressure
     * propagate within the same cycle.
     */

    this->RunLoadPipelineBackward();
    this->RunStorePipelineBackward();

    this->UpdateRegisters();
}

void LoadStoreUnit::RunLoadPipelineBackward() {
    this->FetchLoadData();
    this->TranslateLoadAddress();
    this->GenerateLoadAddress();
    this->IssueLoadRequest();
}

void LoadStoreUnit::RunStorePipelineBackward() {
    this->TranslateStoreAddress();
    this->GenerateStoreAddress();
    this->IssueStoreRequest();
}

void LoadStoreUnit::ReceiveFromCache() {
    MemoryPacket resp;
    while (!this->cache->ReceiveResponse(this->connIds[CACHE_SOLVE_LOAD_DATA],
                                         &resp)) {
        this->HandleLoadDataAck((long)resp);
    }
    while (!this->cache->ReceiveResponse(this->connIds[CACHE_SOLVE_STORE_DATA],
                                         &resp)) {
        this->HandleStoreUpdateAck((long)resp);
    }
}

void LoadStoreUnit::ReceiveFromTlb() {
    MemoryPacket resp;
    while (!this->tlb->ReceiveResponse(this->connIds[TLB_SOLVE_LOAD_ADDRESS],
                                       &resp)) {
        this->HandleLoadTranslation((long)resp, 0);
    }
    while (!this->tlb->ReceiveResponse(this->connIds[TLB_SOLVE_STORE_ADDRESS],
                                       &resp)) {
        this->HandleStoreTranslation((long)resp, 0);
    }
}

void LoadStoreUnit::ReceiveFromRob() {
    DebugPacketLSU resp;
    while (!this->sendTo->ReceiveResponse(this->connIds[SEND_TO], &resp)) {
        this->HandleStoreCommitAck((long)resp.seqNum);
    }
}

void LoadStoreUnit::ReceiveFromScheduler() {
    LSUPacket request;
    int numberOfConnections = this->GetNumberOfConnections();
    for (int i = 0; i < numberOfConnections; i++) {
        while (!this->ReceiveRequestFromConnection(i, &request)) {
            MemoryRequest* req = this->AllocateAndFillMemoryRequest(&request);
            if (request.type == LSUPacketTypeLoadRequest) {
                this->EnqueueLoadToWaitingQueue(req->seqNum);
                this->AddLoadTableEntry(req);
                this->requestedLoads++;
            } else if (request.type == LSUPacketTypeStoreRequest) {
                this->EnqueueStoreToWaitingQueue(req->seqNum);
                this->AddStoreTableEntry(req);
                this->requestedStores++;
            }
        }
    }
}

void LoadStoreUnit::HandleLoadTranslation(long id, unsigned long phyAddress) {
    (void)phyAddress;  // todo: tmp fix
    MemoryRequest* entry = pair::GetElemWithKey(&this->ldTable, id);
    if (entry == NULL) {
        SINUCA3_ERROR_PRINTF("Load entry with key [%ld] not found!\n", id);
        return;
    }

    entry->phyAddress = entry->vtAddress;  // todo: tmp fix
    entry->isTranslated = true;
}

void LoadStoreUnit::HandleStoreTranslation(long id, unsigned long phyAddress) {
    (void)phyAddress;  // todo: tmp fix
    MemoryRequest* entry = pair::GetElemWithKey(&this->stTable, id);
    if (entry == NULL) {
        SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not found!\n", id);
        return;
    }

    entry->phyAddress = entry->vtAddress;  // todo: tmp fix
    entry->isTranslated = true;

    entry->isFinished = true;
    this->stBufferOccupation++;
    this->stUnitwaitingFor--;

    DebugPacketLSU update = {.address = entry->vtAddress,
                             .seqNum = entry->seqNum};

    this->sendTo->SendRequest(this->connIds[SEND_TO], &update);
}

void LoadStoreUnit::HandleStoreCommitAck(long id) {
    MemoryRequest* entry = pair::GetElemWithKey(&this->stTable, id);
    if (entry == NULL) {
        SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not found!\n", id);
        return;
    }
    if (!entry->isFinished) {
        SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not finished!\n", id);
        return;
    }
    if (entry->isCommited) {
        SINUCA3_ERROR_PRINTF("Store entry with key [%ld] commited!\n", id);
        return;
    }

    entry->isCommited = true;
    MemoryPacket update;
    update = entry->seqNum; /* seqNum is key for now. */

    this->cache->SendRequest(this->connIds[CACHE_SOLVE_STORE_DATA], &update);
}

void LoadStoreUnit::HandleLoadDataAck(long id) {
    MemoryRequest* entry = pair::ErasePairWithKey(&this->ldTable, id);
    ++this->finishedLoads;
    delete entry;
}

void LoadStoreUnit::HandleStoreUpdateAck(long id) {
    MemoryRequest* entry = pair::ErasePairWithKey(&this->stTable, id);

    if (entry == NULL) {
        SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not found!\n", id);
        return;
    } else {
        if (!entry->isFinished) {
            SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not finished!\n",
                                 id);
        }
        if (!entry->isCommited) {
            SINUCA3_ERROR_PRINTF("Store entry with key [%ld] not commited!\n",
                                 id);
        }
        delete entry;
    }

    ++this->finishedStores;
    --this->stBufferOccupation;

    delete entry;
}

MemoryRequest* LoadStoreUnit::AllocateAndFillMemoryRequest(
    LSUPacket* lsuRequest) {
    MemoryRequest req = {.vtAddress = lsuRequest->operation.vtAddr,
                         .phyAddress = 0,
                         .seqNum = this->globalSeq++,
                         .accSize = lsuRequest->operation.size,
                         .requestedFetch = false,
                         .wasIssued = false,
                         .isTranslated = false,
                         .isFinished = false,
                         .isCommited = false};
    return new MemoryRequest(req);
}

/* Load Unit Pipeline */

void LoadStoreUnit::IssueLoadRequest() {
    if (this->IsNextStageStalled(ISSUE_LOAD)) {
        this->Stall(ISSUE_LOAD);
    } else {
        this->TryIssueLoad(&this->pipeline[ISSUE_LOAD].regNext);
    }
}

void LoadStoreUnit::GenerateLoadAddress() {
    if (this->InvalidInput(&this->pipeline[ISSUE_LOAD].reg)) {
        this->InvalidateOutput(GEN_LOAD);
    } else if (this->IsNextStageStalled(GEN_LOAD) || this->MustStallGenLoad()) {
        this->Stall(GEN_LOAD);
    } else {
        this->PropagateInput(ISSUE_LOAD, GEN_LOAD);
    }
}

void LoadStoreUnit::TranslateLoadAddress() {
    this->TryToSelectLoadForFetch();

    if (this->InvalidInput()) {
        return;
    }

    if (this->IsNextStageStalled()) {
        this->Stall();
    } else {
        this->RequestLoadTranslation();
    }
}

void LoadStoreUnit::FetchLoadData() {
    if (!this->InvalidInput()) {
        this->RequestLoadFetch();
    }
}

/* Store Unit Pipeline */

void LoadStoreUnit::IssueStoreRequest() {
    if (this->IsNextStageStalled(ISSUE_STORE)) {
        this->Stall(ISSUE_STORE);
    } else {
        this->TryIssueStore(&this->pipeline[ISSUE_STORE].regNext);
    }
}

void LoadStoreUnit::GenerateStoreAddress() {
    if (this->InvalidInput(&this->pipeline[ISSUE_STORE].reg)) {
        this->InvalidateOutput(GEN_STORE);
    } else {
        this->PropagateInput(ISSUE_STORE, GEN_STORE);
    }
}

void LoadStoreUnit::TranslateStoreAddress() {
    if (this->InvalidInput()) {
        return;
    }

    if (this->IsNextStageStalled() || this->MustStallStoreTrans()) {
        this->Stall();
    } else {
        this->RequestStoreTranslation();
    }
}

void LoadStoreUnit::TryIssueLoad(PipelineRegister* next) {
    // change (QUEUE HAS SEQUENCE NUMBERS)
    if (this->waitingLoads.Dequeue(&next->op) != 0) {
        next->isValid = false;
        return;
    }

    next->isValid = true;
    next->op->wasIssued = true;
}

bool LoadStoreUnit::MustStallGenLoad() {
    /*
     * Check for older stores not finished, which may cause address conflicts.
     */
    for (unsigned long i = 0; i < this->stTable.size(); i++) {
        if (this->stTable[i].second->wasIssued &&
            !this->stTable[i].second->isFinished &&
            this->stTable[i].second->seqNum <
                this->pipeline[ISSUE_LOAD].reg.op->seqNum) {
            return true;
        }
    }
    return false;
}

void LoadStoreUnit::RequestLoadTranslation() {
    // Sequence number is used as key for now
    MemoryPacket vtAddr = this->pipeline[GEN_LOAD].reg.op->seqNum;
    this->tlb->SendRequest(this->connIds[TLB_SOLVE_LOAD_ADDRESS], &vtAddr);
}

void LoadStoreUnit::TryToSelectLoadForFetch() {
    for (unsigned long i = 0; i < this->ldTable.size(); i++) {
        if (this->ldTable[i].second->isTranslated &&
            !this->ldTable[i].second->requestedFetch) {
            bool canBypass = this->IsLoadBypassingPossible(
                this->ldTable[i].second->phyAddress,
                this->ldTable[i].second->accSize);
            bool canForward = this->IsLoadForwardingPossible(
                this->ldTable[i].second->phyAddress,
                this->ldTable[i].second->accSize);

            if (this->stBufferOccupation == 0 || canBypass || canForward) {
                this->pipeline[TRANS_LOAD].regNext.op = this->ldTable[i].second;
                this->pipeline[TRANS_LOAD].regNext.isValid = true;
            }
        }
    }
}

void LoadStoreUnit::RequestLoadFetch() {
    MemoryPacket physAddr = this->pipeline[TRANS_LOAD].reg.op->phyAddress;
    this->cache->SendRequest(this->connIds[CACHE_SOLVE_LOAD_DATA], &physAddr);
    this->pipeline[TRANS_LOAD].reg.op->requestedFetch = true;
}

void LoadStoreUnit::TryIssueStore(PipelineRegister* next) {
    // change (QUEUE HAS SEQUENCE NUMBERS)
    if (this->waitingStores.Dequeue(&next->op) != 0) {
        next->isValid = false;
        return;
    }
    next->isValid = true;
    next->op->wasIssued = true;
}

void LoadStoreUnit::RequestStoreTranslation() {
    // Sequence number is used as key for now
    MemoryPacket vtAddr = this->pipeline[GEN_STORE].reg.op->seqNum;
    this->tlb->SendRequest(this->connIds[TLB_SOLVE_STORE_ADDRESS], &vtAddr);
    ++this->stUnitwaitingFor;
}

bool LoadStoreUnit::MustStallStoreTrans() {
    return this->stUnitwaitingFor + this->stBufferOccupation >=
           this->stBufferSize;
}

void LoadStoreUnit::ReceiveCommit() {
    DebugPacketLSU commit;  // todo: remove
    while (this->sendTo->ReceiveResponse(this->connIds[SEND_TO], &commit) ==
           0) {
        MemoryRequest* req;
        if (pair::GetElemWithKey(&this->stTable, commit.seqNum, &req)) {
            SINUCA3_ERROR_PRINTF("key not found!\n");
            return;
        }
        if (!req->isFinished) {
            SINUCA3_ERROR_PRINTF("request not finished!\n");
            return;
        }
        if (req->isCommited) {
            SINUCA3_ERROR_PRINTF("request already commited!\n");
            return;
        }
        req->isCommited = true;
        MemoryPacket update;
        update = req->seqNum; /* seqNum is key for now. */
        this->cache->SendRequest(this->connIds[CACHE_SOLVE_STORE_DATA],
                                 &update);
    }
}

/* Optimization checks */

bool HasIntersection(unsigned long addr1, int size1, unsigned long addr2,
                     int size2) {
    unsigned long endAddr1 = addr1 + size1 - 1;
    unsigned long endAddr2 = addr2 + size2 - 1;
    return ((addr1 <= endAddr2) && (addr2 <= endAddr1));
}

bool ContainsInterval(unsigned long addr1, int size1, unsigned long addr2,
                      int size2) {
    int offset;
    if (addr1 <= addr2) {
        offset = (int)(addr2 - addr1);
        return (offset + size2 <= size1);
    }
    offset = (int)(addr1 - addr2);
    return (offset + size1 <= size2);
}

bool LoadStoreUnit::IsLoadBypassingPossible(unsigned long ldAddress,
                                            int ldSize) {
    if (!this->ldBypassingEnabled) return false;
    for (unsigned long i = 0; i < this->stTable.size(); i++) {
        if (!this->stTable[i].second->isFinished) {
            continue; /* Store not finished, ignore for bypassing. */
        }
        unsigned long stAddress = this->stTable[i].second->phyAddress;
        int stSize = this->stTable[i].second->accSize;
        if (HasIntersection(ldAddress, ldSize, stAddress, stSize)) {
            return false;
        }
    }

    return true;
}

bool LoadStoreUnit::IsLoadForwardingPossible(unsigned long ldAddress,
                                             int ldSize) {
    if (!this->ldForwardingEnabled) return false;
    for (unsigned long i = 0; i < this->stTable.size(); i++) {
        if (!this->stTable[i].second->isFinished) {
            continue; /* Store not finished, ignore for forwarding. */
        }
        unsigned long stAddress = this->stTable[i].second->phyAddress;
        int stSize = this->stTable[i].second->accSize;
        if (ContainsInterval(ldAddress, ldSize, stAddress, stSize)) {
            return true;
        }
    }

    return false;
}
