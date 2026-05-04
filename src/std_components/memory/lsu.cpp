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
    SINUCA3_LOG_PRINTF("Pending loads: %d\n", this->ldReqs.GetOccupation());
    SINUCA3_LOG_PRINTF("Pending stores: %d\n", this->stReqs.GetOccupation());
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
    this->ReceiveCommit();
    this->ReceiveUpdate();
    this->ReceiveTranslation();
    this->ReceiveFetchedData();
    this->ReceiveRequests();
    this->RunPipeline();
}

void LoadStoreUnit::ClearNext() {
    this->pipeline[ISSUE_LOAD].regNext.isValid = false;
    this->pipeline[ISSUE_STORE].regNext.isValid = false;
    this->pipeline[GEN_LOAD].regNext.isValid = false;
    this->pipeline[GEN_STORE].regNext.isValid = false;
    this->pipeline[TRANS_LOAD].regNext.isValid = false;
    this->pipeline[TRANS_STORE].regNext.isValid = false;
    this->pipeline[FETCH_LOAD].regNext.isValid = false;
}

void LoadStoreUnit::UpdateRegisters() {
    this->pipeline[ISSUE_LOAD].reg = this->pipeline[ISSUE_LOAD].regNext;
    this->pipeline[ISSUE_STORE].reg = this->pipeline[ISSUE_STORE].regNext;
    this->pipeline[GEN_LOAD].reg = this->pipeline[GEN_LOAD].regNext;
    this->pipeline[GEN_STORE].reg = this->pipeline[GEN_STORE].regNext;
    this->pipeline[TRANS_LOAD].reg = this->pipeline[TRANS_LOAD].regNext;
    this->pipeline[TRANS_STORE].reg = this->pipeline[TRANS_STORE].regNext;
    this->pipeline[FETCH_LOAD].reg = this->pipeline[FETCH_LOAD].regNext;
}

void LoadStoreUnit::ResetStallSignals() {
    this->pipeline[ISSUE_LOAD].stall = false;
    this->pipeline[ISSUE_STORE].stall = false;
    this->pipeline[GEN_LOAD].stall = false;
    this->pipeline[GEN_STORE].stall = false;
    this->pipeline[TRANS_LOAD].stall = false;
    this->pipeline[TRANS_STORE].stall = false;
    this->pipeline[FETCH_LOAD].stall = false;
}

bool LoadStoreUnit::MustStall(int stage) {
    /* Check if the next stage is stalled */
    return this->pipeline[stage + 1].stall;
}

bool LoadStoreUnit::InvalidInput(const PipelineRegister* reg) {
    return !reg->isValid || reg->op == NULL;
}

void LoadStoreUnit::Stall(int stage) {
    this->pipeline[stage].stall = true;
    this->pipeline[stage].regNext = this->pipeline[stage].reg;
}

void LoadStoreUnit::RunPipeline() {
    this->ResetStallSignals();
    this->ClearNext();

    this->FetchLoadData();
    this->TranslateLoadAddress();
    this->GenerateLoadAddress();
    this->IssueLoadRequest();

    this->TranslateStoreAddress();
    this->GenerateStoreAddress();
    this->IssueStoreRequest();

    this->UpdateRegisters();
}

void LoadStoreUnit::IssueLoadRequest() {
    if (this->MustStall(ISSUE_LOAD)) {
        this->Stall(ISSUE_LOAD);
        return;
    }
    this->pipeline[ISSUE_LOAD].regNext.isValid =
        !this->ldReqs.Dequeue(&this->pipeline[ISSUE_LOAD].regNext.op);
    if (this->pipeline[ISSUE_LOAD].regNext.isValid) {
        this->pipeline[ISSUE_LOAD].regNext.op->wasIssued = true;
    }
}

void LoadStoreUnit::GenerateLoadAddress() {
    if (this->InvalidInput(&this->pipeline[ISSUE_LOAD].reg)) return;
    if (this->MustStall(GEN_LOAD)) {
        this->Stall(GEN_LOAD);
        return;
    }
    for (unsigned long i = 0; i < this->stTable.size(); i++) {
        if (this->stTable[i].second->wasIssued &&
            !this->stTable[i].second->isFinished &&
            this->stTable[i].second->seqNum <
                this->pipeline[ISSUE_LOAD].reg.op->seqNum) {
            /* Wait for older store to finish */
            this->pipeline[GEN_LOAD].stall = true;
            return;
        }
    }
    this->pipeline[GEN_LOAD].regNext.isValid = true;
    this->pipeline[GEN_LOAD].regNext.op = this->pipeline[ISSUE_LOAD].reg.op;
}

void LoadStoreUnit::TranslateLoadAddress() {
    if (this->MustStall(TRANS_LOAD)) {
        this->Stall(TRANS_LOAD);
        return;
    }
    if (!this->InvalidInput(&this->pipeline[GEN_LOAD].reg)) {
        MemoryPacket address;  // tmp
        address = this->pipeline[GEN_LOAD]
                      .reg.op->seqNum; /* seqNum is key for now. */
        this->tlb->SendRequest(this->connIds[TLB_SOLVE_LOAD_ADDRESS], &address);
    }

    for (unsigned long i = 0; i < this->ldTable.size(); i++) {
        if (this->ldTable[i].second->isTranslated &&
            !this->ldTable[i].second->requestedFetch) {
            bool bypassingPossible = this->IsLoadBypassingPossible(
                this->ldTable[i].second->phyAddress,
                this->ldTable[i].second->accSize);
            bool forwardingPossible = this->IsLoadForwardingPossible(
                this->ldTable[i].second->phyAddress,
                this->ldTable[i].second->accSize);
            if (this->stBufferOccupation == 0 || bypassingPossible ||
                forwardingPossible) {
                this->pipeline[TRANS_LOAD].regNext.isValid = true;
                this->pipeline[TRANS_LOAD].regNext.op = this->ldTable[i].second;
                break;
            }
            SINUCA3_DEBUG_PRINTF("cannot forward load!\n");
        }
    }
}

void LoadStoreUnit::FetchLoadData() {
    if (this->InvalidInput(&this->pipeline[TRANS_LOAD].reg)) return;
    MemoryPacket address;  // todo: remove
    this->pipeline[TRANS_LOAD].reg.op->requestedFetch = true;
    address =
        this->pipeline[TRANS_LOAD].reg.op->seqNum; /* seqNum is key for now. */
    this->cache->SendRequest(this->connIds[CACHE_SOLVE_LOAD_DATA], &address);
    this->pipeline[FETCH_LOAD].regNext.isValid = false;  // last stage
}

void LoadStoreUnit::IssueStoreRequest() {
    if (this->MustStall(ISSUE_STORE)) {
        this->Stall(ISSUE_STORE);
        return;
    }
    this->pipeline[ISSUE_STORE].regNext.isValid =
        !this->stReqs.Dequeue(&this->pipeline[ISSUE_STORE].regNext.op);
    if (this->pipeline[ISSUE_STORE].regNext.isValid) {
        this->pipeline[ISSUE_STORE].regNext.op->wasIssued = true;
    }
}

void LoadStoreUnit::GenerateStoreAddress() {
    if (this->InvalidInput(&this->pipeline[ISSUE_STORE].reg)) return;
    if (this->MustStall(GEN_STORE)) {
        this->Stall(GEN_STORE);
        return;
    }
    this->pipeline[GEN_STORE].regNext.isValid = true;
    this->pipeline[GEN_STORE].regNext.op = this->pipeline[ISSUE_STORE].reg.op;
}

void LoadStoreUnit::TranslateStoreAddress() {
    if (this->InvalidInput(&this->pipeline[GEN_STORE].reg)) return;
    if (this->stUnitwaitingFor + this->stBufferOccupation >=
        this->stBufferSize) {
        /* Wait for store buffer space to be available. */
        this->pipeline[TRANS_STORE].stall = true;
        return;
    }
    unsigned long address;  // todo: remove
    address =
        this->pipeline[GEN_STORE].reg.op->seqNum; /* seqNum is key for now. */
    this->tlb->SendRequest(this->connIds[TLB_SOLVE_STORE_ADDRESS], &address);
    this->pipeline[TRANS_STORE].regNext.isValid = false;  // last stage
    ++this->stUnitwaitingFor;
}

void LoadStoreUnit::ReceiveRequests() {
    LSUPacket request;
    int numberOfConnections = this->GetNumberOfConnections();
    for (int i = 0; i < numberOfConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &request) == 0) {
            MemoryRequest req = {.vtAddress = request.operation.vtAddr,
                                 .phyAddress = 0,
                                 .seqNum = this->globalSeq++,
                                 .accSize = request.operation.size,
                                 .requestedFetch = false,
                                 .wasIssued = false,
                                 .isTranslated = false,
                                 .isFinished = false,
                                 .isCommited = false};

            if (request.type == LSUPacketTypeLoadRequest) {
                /* enqueue pointer to load entry */
                void* ptr =
                    pair::PushBackElemWithKey(&this->ldTable, req.seqNum, req);
                this->ldReqs.Enqueue(&ptr);
                this->requestedLoads++;
            } else if (request.type == LSUPacketTypeStoreRequest) {
                /* enqueue pointer to store entry */
                void* ptr =
                    pair::PushBackElemWithKey(&this->stTable, req.seqNum, req);
                this->stReqs.Enqueue(&ptr);
                this->requestedStores++;
            }
        }
    }
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

void LoadStoreUnit::ReceiveUpdate() {
    MemoryPacket update;  // todo: remove
    while (this->cache->ReceiveResponse(this->connIds[CACHE_SOLVE_STORE_DATA],
                                        &update) == 0) {
        MemoryRequest* req;
        if (pair::GetElemWithKey(&this->stTable, (long)update, &req)) {
            SINUCA3_ERROR_PRINTF("key not found!\n");
            return;
        }
        if (!req->isFinished) {
            SINUCA3_ERROR_PRINTF("request not finished!\n");
            return;
        }
        if (!req->isCommited) {
            SINUCA3_ERROR_PRINTF("request not commited!\n");
            return;
        }
        this->OnStoreCompletion(req);
    }
}

void LoadStoreUnit::ReceiveTranslation() {
    MemoryPacket address;  // todo: remove
    while (this->tlb->ReceiveResponse(this->connIds[TLB_SOLVE_LOAD_ADDRESS],
                                      &address) == 0) {
        MemoryRequest* req;  // address is seqNum for now
        if (pair::GetElemWithKey(&this->ldTable, (long)address, &req)) {
            SINUCA3_ERROR_PRINTF("key not found! %ld\n", (long)address);
            return;
        }
        req->phyAddress = req->vtAddress;  // todo: change
        req->isTranslated = true;
    }
    while (this->tlb->ReceiveResponse(this->connIds[TLB_SOLVE_STORE_ADDRESS],
                                      &address) == 0) {
        MemoryRequest* req;  // address is seqNum for now
        if (pair::GetElemWithKey(&this->stTable, (long)address, &req)) {
            SINUCA3_ERROR_PRINTF("key not found! %ld\n", (long)address);
            return;
        }
        req->phyAddress = req->vtAddress;  // todo: change
        req->isTranslated = true;
        --this->stUnitwaitingFor;
        this->OnStoreFinish(req);
    }
}

void LoadStoreUnit::ReceiveFetchedData() {
    MemoryPacket data;  // todo: remove
    while (this->cache->ReceiveResponse(this->connIds[CACHE_SOLVE_LOAD_DATA],
                                        &data) == 0) {
        MemoryRequest* req;  // data is seqNum for now
        if (pair::GetElemWithKey(&this->ldTable, (long)data, &req)) {
            SINUCA3_ERROR_PRINTF("key not found!\n");
            return;
        }
        this->OnLoadCompletion(req);
    }
}

void LoadStoreUnit::OnStoreFinish(MemoryRequest* req) {
    req->isFinished = true;
    this->stBufferOccupation++;
    DebugPacketLSU update;  // todo: remove
    update.seqNum = req->seqNum;
    update.address = req->vtAddress;
    this->sendTo->SendRequest(this->connIds[SEND_TO], &update);
}

void LoadStoreUnit::OnStoreCompletion(MemoryRequest* req) {
    pair::ErasePairWithKey(&this->stTable, req->seqNum);
    ++this->finishedStores;
    --this->stBufferOccupation;
}

void LoadStoreUnit::OnLoadCompletion(MemoryRequest* req) {
    pair::ErasePairWithKey(&this->ldTable, req->seqNum);
    ++this->finishedLoads;
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
            SINUCA3_DEBUG_PRINTF("intersection of %p/%d and %p/%d\n", ldAddress,
                                 ldSize, stAddress, stSize);
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
