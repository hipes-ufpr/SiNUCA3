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
    SINUCA3_LOG_PRINTF("Total load requests: %d\n", this->loadsCounter);
    SINUCA3_LOG_PRINTF("Total store requests: %d\n", this->storesCounter);
    SINUCA3_LOG_PRINTF("Completed store requests: %d\n",
                       this->completedStoreRequests);
    SINUCA3_LOG_PRINTF("Completed load requests: %d\n",
                       this->finishedLoadRequests);
    SINUCA3_LOG_PRINTF("Pending requests: %d\n",
                       this->pendingRequestsQueue.GetOccupation());
}

int LoadStoreUnit::Configure(Config config) {
    if (config.ComponentReference("cache", &this->cache, true))
        return config.Error("cache", "missing required component reference");
    if (config.ComponentReference("tlb", &this->tlb, true))
        return config.Error("tlb", "missing required component reference");
    if (config.ComponentReference("sendTo", &this->sendTo, true))
        return config.Error("sendTo", "missing required component reference");

    /* Adjustable configuration knobs */
    config.Integer("storeBufferSize", &this->storeBufferSize, false);
    config.Bool("loadBypassingEnabled", &this->loadBypassingEnabled, false);
    config.Bool("loadForwardingEnabled", &this->loadForwardingEnabled, false);

    this->connId[TLB_SOLVE_LOAD_ADDRESS] = this->tlb->Connect(0);
    this->connId[TLB_SOLVE_STORE_ADDRESS] = this->tlb->Connect(0);
    this->connId[CACHE_SOLVE_LOAD_DATA] = this->cache->Connect(0);
    this->connId[CACHE_SOLVE_STORE_DATA] = this->cache->Connect(0);

    this->pendingResponses[TLB_SOLVE_LOAD_ADDRESS].Allocate(
        0, sizeof(MemoryPacket));
    this->pendingResponses[TLB_SOLVE_STORE_ADDRESS].Allocate(
        0, sizeof(MemoryPacket));
    this->pendingResponses[CACHE_SOLVE_LOAD_DATA].Allocate(
        0, sizeof(MemoryPacket));
    this->pendingResponses[CACHE_SOLVE_STORE_DATA].Allocate(
        0, sizeof(MemoryPacket));
    this->pendingResponses[SEND_TO_RESPONSE].Allocate(0, sizeof(MemoryPacket));

    this->sendToConnId = this->sendTo->Connect(0);

    return 0;
}

void LoadStoreUnit::Clock() {
    /* Receive responses from connected components. */
    this->ReceiveResponses();
    /* Check store commit acknowledgments. */
    this->CheckStoreCommit();
    /* Check memory updates. */
    this->CheckMemoryUpdate();
    /* Receive new requests. */
    this->ReceiveNewRequests();
    /* Process pending requests. */
    this->ProcessRequests();
}

void LoadStoreUnit::CheckStoreCommit() {
    static MemoryPacket response;
    static bool hasResponse = false;

    while (hasResponse ||
           !this->pendingResponses[SEND_TO_RESPONSE].Dequeue(&response)) {
        hasResponse = true;

        StoreRequest* storeRequest;
        if (!GetElemWithKey(&this->storesTable, response, &storeRequest)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in the stores table");
            return;
        }
        if (!storeRequest->stateIsFinished || storeRequest->stateIsCommited) {
            SINUCA3_ERROR_PRINTF("store with invalid state");
            return;
        }

        /* Update state */
        storeRequest->stateIsCommited = true;

        hasResponse = false;
    }
}

void LoadStoreUnit::CheckMemoryUpdate() {
    static MemoryPacket response;
    static bool hasResponse = false;

    while (hasResponse ||
           !this->pendingResponses[CACHE_SOLVE_STORE_DATA].Dequeue(&response)) {
        hasResponse = true;

        StoreRequest* storeRequest;
        if (!GetElemWithKey(&this->storesTable, response, &storeRequest)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in the stores table\n");
            return;
        }
        if (!storeRequest->stateIsFinished || !storeRequest->stateIsCommited) {
            SINUCA3_ERROR_PRINTF("store with invalid state\n");
            return;
        }

        ErasePairWithKey(&this->storesTable, response);

        /* Symbolic remove from buffer */
        --this->storeBufferOccupation;
        ++this->completedStoreRequests;

        hasResponse = false;
    }
}

bool LoadStoreUnit::IsStoreUnitStalled() {
    return (this->storeBufferOccupation >= this->storeBufferSize);
}

void LoadStoreUnit::ReceiveNewRequests() {
    LSUPacket request;

    int numberOfConnections = this->GetNumberOfConnections();
    for (int i = 0; i < numberOfConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &request) == 0) {
            this->pendingRequestsQueue.Enqueue(&request);
        }
    }
}

void LoadStoreUnit::ReceiveResponses() {
    MemoryPacket response;
    while (!this->sendTo->ReceiveResponse(this->sendToConnId, &response)) {
        this->pendingResponses[SEND_TO_RESPONSE].Enqueue(&response);
    }
    while (!this->tlb->ReceiveResponse(this->connId[TLB_SOLVE_LOAD_ADDRESS],
                                       &response)) {
        this->pendingResponses[TLB_SOLVE_LOAD_ADDRESS].Enqueue(&response);
    }
    while (!this->tlb->ReceiveResponse(this->connId[TLB_SOLVE_STORE_ADDRESS],
                                       &response)) {
        this->pendingResponses[TLB_SOLVE_STORE_ADDRESS].Enqueue(&response);
    }
    while (!this->cache->ReceiveResponse(this->connId[CACHE_SOLVE_LOAD_DATA],
                                         &response)) {
        this->pendingResponses[CACHE_SOLVE_LOAD_DATA].Enqueue(&response);
    }
    while (!this->cache->ReceiveResponse(this->connId[CACHE_SOLVE_STORE_DATA],
                                         &response)) {
        this->pendingResponses[CACHE_SOLVE_STORE_DATA].Enqueue(&response);
    }
}

void LoadStoreUnit::ProcessRequests() {
    static LSUPacket req; /* A pending request. */
    static bool hasPendingReq = false;
    bool loadReady = false;
    bool storeReady = false;

    if (!hasPendingReq) {
        hasPendingReq = !this->pendingRequestsQueue.Dequeue(&req);
    }

    if (hasPendingReq && req.type == LSUPacketTypeLoadRequest && this->unresolvedStores == 0) {
        loadReady = true;

        ++this->loadsNotForwardedToFetchStage;
        ++this->loadsCounter;

        LoadRequest loadRequest;
        loadRequest.waitingTranslation = true;
        loadRequest.accessSize = req.operation.size;
        loadRequest.physicalAddress = 0;
        loadRequest.virtualAddress = req.operation.vtAddr;
        PushBackElemWithKey(&this->loadsTable, req.operation.vtAddr, loadRequest);

        SINUCA3_DEBUG_PRINTF("address is %lu\n", req.operation.vtAddr);
    }
    /* Call 1st stage of load unit. */
    this->GenerateLoadAddress(req.operation.vtAddr, loadReady);
    if (loadReady) {
        hasPendingReq = !this->pendingRequestsQueue.Dequeue(&req);
    }

    if (!this->IsStoreUnitStalled()) {
        if (hasPendingReq && req.type == LSUPacketTypeStoreRequest) {
            storeReady = true;
            hasPendingReq = false;

            ++this->unresolvedStores;
            ++this->storesCounter;

            StoreRequest storeRequest;
            storeRequest.stateIsFinished = false;
            storeRequest.stateIsCommited = false;
            storeRequest.accessSize = req.operation.size;
            storeRequest.physicalAddress = 0;
            storeRequest.virtualAddress = req.operation.vtAddr;
            PushBackElemWithKey(&this->storesTable, req.operation.vtAddr, storeRequest);
        }
        /* Call 1st stage of store unit if not stalled. */
        this->GenerateStoreAddress(req.operation.vtAddr, storeReady);
    }
}

/* Load unit pipeline stages */
void LoadStoreUnit::GenerateLoadAddress(unsigned long address, bool ready) {
    static bool generationReady = false;
    static unsigned long generatedAddress = 0;

    /* Call following pipeline stage */
    this->TranslateLoadAddress(generatedAddress, generationReady);
    generationReady = ready;
    generatedAddress = address;
}

void LoadStoreUnit::TranslateLoadAddress(unsigned long address, bool ready) {
    static MemoryPacket translation;
    static bool hasPendingLoad = false;

    /* Send request to tlb */
    if (ready) {
        MemoryPacket tlbRequest = address;
        this->tlb->SendRequest(this->connId[TLB_SOLVE_LOAD_ADDRESS],
                               &tlbRequest);
    }
    /* Process tlb response */
    bool pendingLoadReady = false;
    LoadRequest* loadRequest;
    if (hasPendingLoad ||
        !this->pendingResponses[TLB_SOLVE_LOAD_ADDRESS].Dequeue(&translation)) {
        hasPendingLoad = true;
        if (!GetElemWithKey(&this->loadsTable, translation, &loadRequest)) {
            SINUCA3_ERROR_PRINTF(
                "load address not registered in the loads table\n");
            return;
        }
        if (!loadRequest->waitingTranslation) {
            SINUCA3_ERROR_PRINTF("load with invalid state\n");
            return;
        }

        if (this->loadBypassingEnabled) {
            if (this->IsLoadBypassingPossible(translation,
                                              loadRequest->accessSize)) {
                pendingLoadReady = true;
            } else if (this->loadForwardingEnabled) {
                if (this->IsLoadForwardingPossible(translation,
                                                   loadRequest->accessSize)) {
                    pendingLoadReady = true;
                }
            }
        } else {
            if (this->storeBufferOccupation == 0) {
                pendingLoadReady = true;
            }
        }
    }
    if (pendingLoadReady) {
        loadRequest->waitingTranslation = false;
        loadRequest->physicalAddress = translation;
        hasPendingLoad = false;
        --this->loadsNotForwardedToFetchStage;
        SINUCA3_DEBUG_PRINTF("set waiting to false\n");
    }
    /* Call next stage */
    this->FetchLoadData(translation, pendingLoadReady);
}

void LoadStoreUnit::FetchLoadData(unsigned long address, bool ready) {
    /* Send request to cache */
    if (ready) {
        MemoryPacket cacheRequest = address;
        this->cache->SendRequest(this->connId[CACHE_SOLVE_LOAD_DATA],
                                 &cacheRequest);
    }
    /* Process cache response */
    MemoryPacket cacheResponse;
    if (!this->pendingResponses[CACHE_SOLVE_LOAD_DATA].Dequeue(
            &cacheResponse)) {
        if (!ErasePairWithKey(&this->loadsTable, cacheResponse)) {
            SINUCA3_ERROR_PRINTF(
                "load address not registered in the loads table\n");
            return;
        }
        ++this->finishedLoadRequests;
        /* Notify component */
        // Todo: uncomment
        // this->sendTo->SendRequest(this->sendToConnId, &cacheResponse);
    }
}

/* Store unit pipeline stages */
void LoadStoreUnit::GenerateStoreAddress(unsigned long address, bool ready) {
    static bool generationReady = false;
    static unsigned long generatedAddress = 0;

    /* Call following pipeline stage */
    this->TranslateStoreAddress(generatedAddress, generationReady);
    generationReady = ready;
    generatedAddress = address;
}

void LoadStoreUnit::TranslateStoreAddress(unsigned long address, bool ready) {
    static MemoryPacket translation;
    static bool hasPendingStore = false;

    /* Send request to tlb */
    if (ready) {
        MemoryPacket tlbRequest = address;
        this->tlb->SendRequest(this->connId[TLB_SOLVE_STORE_ADDRESS],
                               &tlbRequest);
    }
    /* Deal with tlb response */
    if (hasPendingStore ||
        !this->pendingResponses[TLB_SOLVE_STORE_ADDRESS].Dequeue(
            &translation)) {
        hasPendingStore = true;

        if (this->loadsNotForwardedToFetchStage > 0) {
            return; /* Store operation must wait. */
        }
        StoreRequest* storeRequest;
        if (!GetElemWithKey(&this->storesTable, translation, &storeRequest)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in the stores table\n");
            return;
        }
        if (!storeRequest->stateIsFinished) {
            SINUCA3_ERROR_PRINTF("store with invalid state\n");
            return;
        }
        storeRequest->physicalAddress = translation;
        /* Update state */
        storeRequest->stateIsFinished = true;
        /* Symbolic add to buffer */
        ++this->storeBufferOccupation;
        /* Notify component */
        this->sendTo->SendRequest(this->sendToConnId, &translation);
        /* Store is considered resolved */
        --this->unresolvedStores;

        hasPendingStore = false;
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
    for (unsigned long i = 0; i < this->storesTable.size(); i++) {
        if (!this->storesTable[i].second.stateIsFinished) {
            continue; /* Store not finished, ignore for bypassing. */
        }
        unsigned long stAddress = this->storesTable[i].second.physicalAddress;
        int stSize = this->storesTable[i].second.accessSize;
        if (HasIntersection(ldAddress, ldSize, stAddress, stSize)) {
            return false;
        }
    }

    return true; /* No intersection found. Bypassing possible. */
}

bool LoadStoreUnit::IsLoadForwardingPossible(unsigned long ldAddress,
                                             int ldSize) {
    for (unsigned long i = 0; i < this->storesTable.size(); i++) {
        if (!this->storesTable[i].second.stateIsFinished) {
            continue; /* Store not finished, ignore for forwarding. */
        }
        unsigned long stAddress = this->storesTable[i].second.physicalAddress;
        int stSize = this->storesTable[i].second.accessSize;
        if (ContainsInterval(ldAddress, ldSize, stAddress, stSize)) {
            return true;
        }
    }

    return false; /* No store contains load. Forwarding not possible. */
}
