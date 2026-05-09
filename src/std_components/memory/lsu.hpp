#ifndef SINUCA3_LSU_HPP_
#define SINUCA3_LSU_HPP_

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
 * @file lsu.hpp
 * @brief Load store unit component.
 * @details This component expects to receive load and store requests from
 * a reservation station with all operands ready. It is responsible for
 * simulating the pipeline of a load store unit which interacts with a
 * tlb and a cache. It also implements optimizations such as load bypassing and
 * load forwarding. These optimizations can be turned on and off via
 * configuration knobs. After the successful ending of a request, this
 * component sends a message to the next stage notifying the completion of the
 * request. Store requests must sit in the store buffer until they are committed
 * and update the cache. The size of this buffer can also be configured.
 */

#include <sinuca3.hpp>

#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include "utils/circular_buffer.hpp"
#include "utils/pair.hpp"

// todo: remove
struct DebugPacketLSU {
    unsigned long address;
    long seqNum;
};

enum LSUPacketType { LSUPacketTypeLoadRequest, LSUPacketTypeStoreRequest };

enum ConnectionType {
    TLB_SOLVE_LOAD_ADDRESS = 0,
    TLB_SOLVE_STORE_ADDRESS,
    CACHE_SOLVE_LOAD_DATA,
    CACHE_SOLVE_STORE_DATA,
    SEND_TO,
    TOTAL_CONNECTIONS
};

enum PipelineStage {
    FETCH_LOAD = 0,
    TRANS_LOAD,
    GEN_LOAD,
    ISSUE_LOAD,
    TRANS_STORE,
    GEN_STORE,
    ISSUE_STORE,
    NUM_STAGES
};

struct LSUPacket {
    struct {
        unsigned long vtAddr; /** @brief Virtual address. */
        int size; /** @brief Size of the memory operation in bytes. */
    } operation;  /** @brief On load or store request. */
    LSUPacketType type;
};

struct MemoryRequest {
    unsigned long vtAddress;
    unsigned long phyAddress;
    long seqNum;
    int accSize;
    bool requestedFetch; /* For loads */
    bool wasIssued;
    bool wasTranslated;
    bool wasCommited; /* For stores */
};

struct PipelineRegister {
    MemoryRequest* op;
    bool isValid;
};

struct PipelineData {
    PipelineRegister reg;
    PipelineRegister regNext;
    int nextStage;
    bool stall;
};

/** @brief Check lsu.hpp documentation for details. */
class LoadStoreUnit : public Component<LSUPacket> {
  private:
    Component<MemoryPacket>* tlb;
    Component<MemoryPacket>* cache;
    Component<DebugPacketLSU>* sendTo; // change to rob pointer later
    int connIds[TOTAL_CONNECTIONS];

    /** @brief Queue with pointers to pending load requests */
    CircularBuffer waitingLoads;
    /** @brief Queue with pointers to pending store requests */
    CircularBuffer waitingStores;
    /** @brief Table for load requests */
    std::vector<pair::Pair<long, MemoryRequest*> > ldTable;
    /** @brief Table for store requests */
    std::vector<pair::Pair<long, MemoryRequest*> > stTable;

    /** @brief Number of translations not yet received by st unit */
    long stUnitwaitingFor;
    /** @brief Occupation of the store buffer */
    long stBufferOccupation;
    /** @brief Global sequence number */
    long globalSeq;

    /* Configuration knobs */
    /** @brief Size of the store buffer */
    long stBufferSize;
    /** @brief Whether load bypassing is enabled */
    bool ldBypassingEnabled;
    /** @brief Whether load forwarding is enabled */
    bool ldForwardingEnabled;

    /* Use in statistics */
    int finishedStores;
    int finishedLoads;
    int requestedLoads;
    int requestedStores;

    /** @brief Control data for each stage. */
    PipelineData pipeline[NUM_STAGES];

    /** @brief Get requests from connected components. */
    void ReceiveRequests();
    /** @brief Get responses from connected components. */
    void ReceiveResponses();
    /** @brief Run the pipeline calls. */
    void RunPipeline();

    /** @brief Run the store pipeline backwards. */
    void RunStorePipelineBackward();
    /** @brief Run the load pipeline backwards. */
    void RunLoadPipelineBackward();

    /** @brief Get responses from the cache. */
    void ReceiveFromCache();
    /** @brief Get responses from the tlb. */
    void ReceiveFromTlb();
    /** @brief Get responses from the rob. */
    void ReceiveFromRob();
    /** @brief Get requests from the scheduler. */
    void ReceiveFromScheduler();

    /** @brief Handle load data acknowledgment */
    void HandleLoadDataAck(long id);
    /** @brief Handle store update acknowledgment */
    void HandleStoreUpdateAck(long id);
    /** @brief Handle load translation */
    void HandleLoadTranslation(long id, unsigned long address);
    /** @brief Handle store translation */
    void HandleStoreTranslation(long id, unsigned long address);
    /** @brief Handle store commit acknowledgment */
    void HandleStoreCommitAck(long id);
    /** @brief Allocate and fill a memory request */
    MemoryRequest* AllocateAndFillMemoryRequest(LSUPacket* lsuRequest);

    /* Pipeline stages */
    /** @brief Issue a load request */
    void IssueLoadRequest();
    /** @brief Issue a store request */
    void IssueStoreRequest();
    /** @brief Generate load address */
    void GenerateLoadAddress();
    /** @brief Translate load address */
    void TranslateLoadAddress();
    /** @brief Fetch load data */
    void FetchLoadData();
    /** @brief Generate store address */
    void GenerateStoreAddress();
    /** @brief Translate store address */
    void TranslateStoreAddress();

    /** @brief Try to issue a load request. */
    void TryIssueLoad(PipelineRegister* next);
    /** @brief Try to issue a store request. */
    void TryIssueStore(PipelineRegister* next);
    /** @brief Request data fetch */
    void RequestDataFetch(MemoryPacket* physAddr);
    /** @brief Request data update */
    void RequestDataUpdate(MemoryPacket* physAddr);
    /** @brief Called when there is a mmu. */
    void RequestStoreTranslation(MemoryPacket* vtAddr);
    /** @brief Called when there is a mmu. */
    void RequestLoadTranslation(MemoryPacket* vtAddr);
    /** @brief Called when there is no mmu. */
    void TranslateLoadDirectly();
    /** @brief Called when there is no mmu. */
    void TranslateStoreDirectly();
    /** @brief Check if the load generation stage should stall. */
    bool MustStallGenLoad();
    /** @brief Check if the store translation stage should stall. */
    bool MustStallStoreTrans();
    /** @brief Try to select a translated load for fetching. */
    void TryToSelectLoadForFetch();

    /* Optimizations */
    /** @brief Check if address aliasing exists. */
    bool IsLoadBypassingPossible(unsigned long address, int size);
    /** @brief Check if forwarding store data is possible. */
    bool IsLoadForwardingPossible(unsigned long address, int size);

    inline bool IsStalled(int stage) {
        return this->pipeline[stage].stall;
    }
    inline bool InvalidInput(const PipelineRegister* reg) {
        return !reg->isValid || reg->op == NULL;
    }
    inline void Stall(int stage) {
        this->pipeline[stage].stall = true;
        this->pipeline[stage].regNext = this->pipeline[stage].reg;
    }
    inline void PropagateInput(int prev, int next) {
        this->pipeline[next].regNext = this->pipeline[prev].reg;
        this->pipeline[next].regNext.isValid = true;
    }
    inline void InvalidateOutput(int stage) {
        this->pipeline[stage].regNext.isValid = false;
    }

    inline void ClearNext() {
        for (int i = 0; i < NUM_STAGES; i++) {
            this->pipeline[i].regNext.isValid = false;
        }
    }
    inline void UpdateRegisters() {
        for (int i = 0; i < NUM_STAGES; i++) {
            this->pipeline[i].reg = this->pipeline[i].regNext;
        }
    }
    inline void ResetStallSignals() {
        for (int i = 0; i < NUM_STAGES; i++) {
            this->pipeline[i].stall = false;
        }
    }

    inline void EnqueueLoadToWaitingQueue(long seqNum) {
        this->waitingLoads.Enqueue(&seqNum);
    }
    inline void EnqueueStoreToWaitingQueue(long seqNum) {
        this->waitingStores.Enqueue(&seqNum);
    }
    inline void AddLoadTableEntry(MemoryRequest* req) {
        pair::PushBackElemWithKey(&this->ldTable, req->seqNum, req);
    }
    inline void AddStoreTableEntry(MemoryRequest* req) {
        pair::PushBackElemWithKey(&this->stTable, req->seqNum, req);
    }

  public:
    LoadStoreUnit()
        : tlb(NULL),
          cache(NULL),
          sendTo(NULL),
          stUnitwaitingFor(0),
          stBufferOccupation(0),
          globalSeq(0),
          stBufferSize(16),
          ldBypassingEnabled(true),
          ldForwardingEnabled(true),
          finishedStores(0),
          finishedLoads(0),
          requestedLoads(0),
          requestedStores(0) {
        this->waitingLoads.Allocate(0, sizeof(long));
        this->waitingStores.Allocate(0, sizeof(long));

        /* Initialize pipeline data */
        memset(this->pipeline, 0, sizeof(this->pipeline));

        this->pipeline[ISSUE_LOAD].nextStage = GEN_LOAD;
        this->pipeline[GEN_LOAD].nextStage = TRANS_LOAD;
        this->pipeline[TRANS_LOAD].nextStage = FETCH_LOAD;
        this->pipeline[ISSUE_STORE].nextStage = GEN_STORE;
        this->pipeline[GEN_STORE].nextStage = TRANS_STORE;

        for (int i = 0; i < TOTAL_CONNECTIONS; i++)
            this->connIds[i] = -1;
    }
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~LoadStoreUnit() {
        this->waitingLoads.Deallocate();
        this->waitingStores.Deallocate();
    }
};

#endif  // SINUCA3_LSU_HPP_
