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
 * @file
 */

// todo: add more comments to the code
// todo: adapt code to segmented buffer
// todo: create component to be 'middle-man' between sendTo and rob
// todo: add debug prints to the code
// todo: break unaligned load operations into multiple aligned load operations

#include "lsu.hpp"

#include <cmath>

#include "engine/default_packets.hpp"
#include "utils/logger.hpp"

void LoadStoreUnit::PrintStatistics() {
    SINUCA3_LOG_PRINTF(
        "Total load requests: %d\nTotal store requests: %d\nFinished store "
        "requests: %d\nCompleted store requests: %d\nFinished load requests: "
        "%d\nPending requests: %d\n",
        this, this->totalLoadRequests, this->totalStoreRequests,
        this->finishedStoreRequests, this->completedStoreRequests,
        this->finishedLoadRequests, this->pendingRequestsQueue.GetOccupation());
}

int LoadStoreUnit::Configure(Config config) {
    if (config.ComponentReference("Cache", &this->cache, true))
        return config.Error("Cache", "missing required component reference");
    if (config.ComponentReference("Tlb", &this->tlb, true))
        return config.Error("Tlb", "missing required component reference");
    if (config.ComponentReference("sendTo", &this->sendTo, true))
        return config.Error("sendTo", "missing required component reference");

    /* Adjustable configuration knobs */
    config.Integer("numCacheReadPorts", &this->numberOfCacheReadPorts, false);
    config.Integer("numTlbReadPorts", &this->numberOfTlbReadPorts, false);
    config.Integer("numCacheWritePorts", &this->numberOfCacheWritePorts, false);
    config.Integer("numTlbWritePorts", &this->numberOfTlbWritePorts, false);
    config.Integer("cacheLineSize", &this->cacheLineSize, false);
    config.Integer("finishedBufSize", &this->finishedStoreBufferSize, false);
    config.Integer("completedBufSize", &this->completedStoreBufferSize, false);
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
    /* Reset port usage for the current cycle. */
    this->ResetPortUsage();
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

    while (1) {
        if (!hasResponse &&
            this->pendingResponses[SEND_TO_RESPONSE].Dequeue(&response)) {
            return; /* No pending responses. */
        }

        hasResponse = true;
        if ((long)this->completedStoreBuffer.size() >=
            this->completedStoreBufferSize) {
            return; /* Completed store buffer is full. */
        }
        if (!EraseElem(&this->finishedStoreBuffer, response)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in finished buffer");
            return;
        }

        /* Move store request from finished buffer to completed buffer. */
        PushBackElem(&this->completedStoreBuffer, response);
        hasResponse = false;
    }
}

void LoadStoreUnit::CheckMemoryUpdate() {
    static MemoryPacket response;
    static bool hasResponse = false;

    while (1) {
        if (!hasResponse &&
            this->pendingResponses[CACHE_SOLVE_LOAD_DATA].Dequeue(&response)) {
            return; /* No pending responses. */
        }

        hasResponse = true;
        if (!EraseElem(&this->completedStoreBuffer, response)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in completed buffer");
            return;
        }
        if (!ErasePairWithKey(&this->storeAddressToSize, response)) {
            SINUCA3_ERROR_PRINTF(
                "store address not registered in store address to size buffer");
            return;
        }

        ++this->completedStoreRequests;
        hasResponse = false;
    }
}

void LoadStoreUnit::ResetPortUsage() {
    this->cacheReadPortUsage = 0;
    this->cacheWritePortUsage = 0;
    this->tlbReadPortUsage = 0;
    this->tlbWritePortUsage = 0;
}

void LoadStoreUnit::BuildAlignedLoadSubrequests(unsigned long address,
                                                int size) {
    int lineOfFirstByte = (int)DIV2(address, this->cacheLineSize);
    int lineOfLastByte = (int)DIV2(address + size - 1, this->cacheLineSize);

    LSUPacket subrequest;
    subrequest.type = LSUPacketTypeLoadRequest;
    int tmp;
    int subReqSize;
    int splitCount = 0;

    while (lineOfFirstByte <= lineOfLastByte) {
        /* Align to closest greater power of 2 */
        tmp = this->cacheLineSize - MOD2(address, this->cacheLineSize);
        subReqSize = 1 << ((int)log2(tmp) + 1);
        subrequest.operation.vtAddr -= (subReqSize - tmp);
        subrequest.operation.size = subReqSize;
        /* Associate a unique id and size to the subrequest */
        ++splitCount;
        Pair<unsigned int, int> pair;
        pair.key = this->globalLoadIdentifier;
        pair.elem = subrequest.operation.size;
        PushBackElemWithKey(&this->loadAddressToIdAndSize,
                            subrequest.operation.vtAddr, pair);
        /* Enqueue the subrequest */
        this->pendingRequestsQueue.Enqueue(&subrequest);
        /* Update the line number and address */
        ++lineOfFirstByte;
        address = lineOfFirstByte * this->cacheLineSize;
        size -= subrequest.operation.size;
    }

    /* Associate the load identifier with the number of subrequests created. */
    PushBackElemWithKey(&this->loadIdentifierToDebt, this->globalLoadIdentifier,
                        splitCount);
    /* Increment global load identifier to distinguish between requests */
    ++this->globalLoadIdentifier;
}

void LoadStoreUnit::BuildAlignedStoreSubrequests(unsigned long address,
                                                 int size) {
    LSUPacket request;
    Pair<unsigned long, int> pair;
    // Not breaking store requests into subrequests because
    // dont see speedup, but this can be changed in the future
    request.type = LSUPacketTypeStoreRequest;
    request.operation.vtAddr = address;
    request.operation.size = size;
    this->pendingRequestsQueue.Enqueue(&request);

    /* Associate the store address with its size */
    pair.key = request.operation.vtAddr;
    pair.elem = request.operation.size;
    PushBackElemWithKey(&this->storeAddressToSize, pair.key, pair.elem);
}

bool LoadStoreUnit::IsStoreUnitStalled() {
    return ((int)this->finishedStoreBuffer.size() >=
            this->finishedStoreBufferSize);
}

void LoadStoreUnit::ReceiveNewRequests() {
    LSUPacket request;
    int numberOfConnections = this->GetNumberOfConnections();
    for (int i = 0; i < numberOfConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &request) == 0) {
            if (request.type == LSUPacketTypeLoadRequest) {
                this->BuildAlignedLoadSubrequests(request.operation.vtAddr,
                                                  request.operation.size);
            } else {
                this->BuildAlignedStoreSubrequests(request.operation.vtAddr,
                                                   request.operation.size);
            }
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

    if (!hasPendingReq && this->pendingRequestsQueue.Dequeue(&req)) {
        return; /* No pending requests. */
    }

    if (req.type == LSUPacketTypeLoadRequest &&
        this->unresolvedStoreAddressRequests == 0) {
        ++this->totalLoadRequests;
        loadReady = true;
        hasPendingReq = this->pendingRequestsQueue.Dequeue(&req);
    }
    /* Call 1st stage of load unit. */
    this->GenerateLoadAddress(req.operation.vtAddr, loadReady);

    if (!this->IsStoreUnitStalled()) {
        if (req.type == LSUPacketTypeStoreRequest) {
            ++this->totalStoreRequests;
            ++this->unresolvedStoreAddressRequests;
            storeReady = true;
            hasPendingReq = false;
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
    static std::vector<MemoryPacket> addressTranslations;
    static unsigned long addressToTranslate = 0;
    static bool translationReady = false;
    Pair<unsigned int, int> idAndSize;

    this->FetchLoadData(&addressTranslations);
    MemoryPacket tlbRequest;

    if (translationReady &&
        (this->tlbWritePortUsage < this->numberOfTlbWritePorts)) {
        tlbRequest = addressToTranslate;
        this->tlb->SendRequest(this->connId[TLB_SOLVE_LOAD_ADDRESS],
                               &tlbRequest);
        /* Assuming 1 tlb write port per request. */
        ++this->tlbWritePortUsage;
    }

    translationReady = ready;
    addressToTranslate = address;

    // this piece of code may not seem to make sense now bc the tlb/cache is
    // not implemented. But when they are implemented, the tlb response
    // should contain both the translated physical address and the original
    // virtual address, so we can forward the translated address
    MemoryPacket tlbResponse;
    while (
        !this->pendingResponses[TLB_SOLVE_LOAD_ADDRESS].Dequeue(&tlbResponse) &&
        (this->tlbReadPortUsage >= this->numberOfTlbReadPorts)) {
        GetElemWithKey(&this->loadAddressToIdAndSize, tlbResponse, &idAndSize);
        int correspondingSize = idAndSize.key;

        if (!(this->loadBypassingEnabled &&
              this->IsLoadBypassingPossible(tlbResponse, correspondingSize)) &&
            !(this->loadForwardingEnabled &&
              this->IsLoadForwardingPossible(tlbResponse, correspondingSize))) {
            return; /* No valid bypassing and forwarding possible. */
        }
        ChangeKeyOfElem(&this->loadAddressToIdAndSize, tlbResponse,
                        tlbResponse);
        PushBackElem(&addressTranslations, tlbResponse);
        /* Assuming 1 tlb read port per request. */
        ++this->tlbReadPortUsage;
    }
}

void LoadStoreUnit::FetchLoadData(std::vector<unsigned long>* addresses) {
    MemoryPacket cacheReq;
    for (int i = 0; i < (int)addresses->size(); i++) {
        if (this->cacheWritePortUsage >= this->numberOfCacheWritePorts) {
            break; /* No available cache write ports. */
        }

        // cache request should contain the size info
        // change in the future when cache is implemented
        cacheReq = addresses->at(i);
        this->cache->SendRequest(this->connId[CACHE_SOLVE_LOAD_DATA],
                                 &cacheReq);
        /* Remove the address to avoid reprocessing */
        EraseElem(addresses, cacheReq);
        /* Assuming 1 cache write port per request. */
        ++this->cacheWritePortUsage;
    }

    Pair<unsigned int, int> idAndSize;
    MemoryPacket cacheResponse;
    int debt;
    unsigned int identifier;
    while ((this->cacheReadPortUsage >= this->numberOfCacheReadPorts) &&
           !this->pendingResponses[CACHE_SOLVE_LOAD_DATA].Dequeue(
               &cacheResponse)) {
        if (!GetElemWithKey(&this->loadAddressToIdAndSize, cacheResponse,
                            &idAndSize)) {
            SINUCA3_ERROR_PRINTF(
                "load address not registered in id and size buffer");
            return;
        }
        identifier = idAndSize.key;
        if (!GetElemWithKey(&this->loadIdentifierToDebt, identifier, &debt)) {
            SINUCA3_ERROR_PRINTF(
                "load identifier not registered in debt buffer");
            return;
        }

        /* Remove this subrequest's entry to avoid unlimited growth. */
        ErasePairWithKey(&this->loadAddressToIdAndSize, cacheResponse);

        if (debt <= 0) {
            /* Notify the next stage of the completed load request. */
            this->sendTo->SendRequest(this->sendToConnId, &cacheResponse);
            /* Remove this subrequest's entry to avoid unlimited growth. */
            ErasePairWithKey(&this->loadIdentifierToDebt, identifier);
            ++this->finishedLoadRequests;
        } else {
            --debt;
            UpdateElemWithKey(&this->loadIdentifierToDebt, identifier, debt);
        }

        /* Assuming 1 cache read port per request. */
        ++this->cacheReadPortUsage;
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
    static unsigned long addressToTranslate = 0;
    static bool translationReady = false;
    MemoryPacket tlbRequest;

    if (translationReady &&
        (this->tlbWritePortUsage < this->numberOfTlbWritePorts)) {
        tlbRequest = addressToTranslate;
        this->tlb->SendRequest(this->connId[TLB_SOLVE_STORE_ADDRESS],
                               &tlbRequest);
        /* Assuming 1 tlb write port per request. */
        ++this->tlbWritePortUsage;
    }

    translationReady = ready;
    addressToTranslate = address;

    MemoryPacket tlbResponse;
    while ((this->tlbReadPortUsage >= this->numberOfTlbReadPorts) &&
           ((int)this->finishedStoreBuffer.size() >=
            this->finishedStoreBufferSize) &&
           !this->pendingResponses[TLB_SOLVE_STORE_ADDRESS].Dequeue(
               &tlbResponse)) {
        PushBackElem(&this->finishedStoreBuffer, tlbResponse);
        /* Assuming 1 tlb read port per request. */
        ++this->tlbReadPortUsage;
        --this->unresolvedStoreAddressRequests;
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

bool LoadStoreUnit::IsLoadBypassingPossible(unsigned long address, int size) {
    for (int i = 0; i < (int)this->finishedStoreBuffer.size(); i++) {
        unsigned long stAddress = this->storeAddressToSize[i].key;
        int stSize = this->storeAddressToSize[i].elem;
        if (HasIntersection(address, size, stAddress, stSize)) {
            return false;
        }
    }
    for (int i = 0; i < (int)this->completedStoreBuffer.size(); i++) {
        unsigned long stAddress = this->storeAddressToSize[i].key;
        int stSize = this->storeAddressToSize[i].elem;
        if (HasIntersection(address, size, stAddress, stSize)) {
            return false;
        }
    }
    return true; /* No intersection found. Bypassing possible. */
}

bool LoadStoreUnit::IsLoadForwardingPossible(unsigned long address, int size) {
    // More than one store request may contain the load request, but we are not
    // emulating the actual data forwarding, so one match is enough
    for (int i = 0; i < (int)this->finishedStoreBuffer.size(); i++) {
        unsigned long stAddress = this->storeAddressToSize[i].key;
        int stSize = this->storeAddressToSize[i].elem;
        if (ContainsInterval(address, size, stAddress, stSize)) {
            return true;
        }
    }
    for (int i = 0; i < (int)this->completedStoreBuffer.size(); i++) {
        unsigned long stAddress = this->storeAddressToSize[i].key;
        int stSize = this->storeAddressToSize[i].elem;
        if (ContainsInterval(address, size, stAddress, stSize)) {
            return true;
        }
    }
    return false; /* No store contains load. Forwarding not possible. */
}
