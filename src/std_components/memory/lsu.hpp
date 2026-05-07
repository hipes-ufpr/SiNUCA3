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

const int TLB_SOLVE_LOAD_ADDRESS = 0;
const int TLB_SOLVE_STORE_ADDRESS = 1;
const int CACHE_SOLVE_LOAD_DATA = 2;
const int CACHE_SOLVE_STORE_DATA = 3;
const int SEND_TO = 4;

const int FETCH_LOAD = 0;
const int TRANS_LOAD = 1;
const int GEN_LOAD = 2;
const int ISSUE_LOAD = 3;
const int TRANS_STORE = 4;
const int GEN_STORE = 5;
const int ISSUE_STORE = 6;

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
    bool isTranslated;
    bool isFinished; /* For stores */
    bool isCommited; /* For stores */
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
    int connIds[5];

    /** @brief Queue with pointers to pending load requests */
    CircularBuffer ldReqs;
    /** @brief Queue with pointers to pending store requests */
    CircularBuffer stReqs;
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
    PipelineData pipeline[7];

    /** @brief Get commit responses. */
    void ReceiveCommit();
    /** @brief Get memory update responses. */
    void ReceiveUpdate();
    /** @brief Get translation responses. */
    void ReceiveTranslation();
    /** @brief Get fetched data responses. */
    void ReceiveFetchedData();
    /** @brief Get new requests. Add operation to table and enqueue request. */
    void ReceiveRequests();
    /** @brief Run the pipeline calls. */
    void RunPipeline();
    /** @brief Invalidate next cycle registers. */
    void ClearNext();
    /** @brief Update registers. */
    void UpdateRegisters();
    /** @brief Set stall to false */
    void ResetStallSignals();
    /** @brief Set stall signal and keep current register value */
    void Stall(int stage);
    /** @brief Check if the next stage is stalled */
    bool MustStall(int stage);
    /** @brief Check if the input to a stage is valid */
    bool InvalidInput(const PipelineRegister* reg);
    /** @brief Handle load completion */
    void OnLoadCompletion(MemoryRequest* req);
    /** @brief Handle store commit */
    void OnStoreFinish(MemoryRequest* req);
    /** @brief Handle store completion */
    void OnStoreCompletion(MemoryRequest* req);

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

    /* Optimizations */
    /** @brief Check if address aliasing exists. */
    bool IsLoadBypassingPossible(unsigned long address, int size);
    /** @brief Check if forwarding store data is possible. */
    bool IsLoadForwardingPossible(unsigned long address, int size);

  public:
    LoadStoreUnit()
        : stUnitwaitingFor(0),
          stBufferOccupation(0),
          globalSeq(0),
          stBufferSize(16),
          ldBypassingEnabled(true),
          ldForwardingEnabled(true),
          finishedStores(0),
          finishedLoads(0),
          requestedLoads(0),
          requestedStores(0) {
        this->ldReqs.Allocate(0, sizeof(void*));
        this->stReqs.Allocate(0, sizeof(void*));
        
        /* Initialize pipeline data */
        memset(this->pipeline, 0, sizeof(this->pipeline));

        this->pipeline[ISSUE_LOAD].nextStage = GEN_LOAD;
        this->pipeline[GEN_LOAD].nextStage = TRANS_LOAD;
        this->pipeline[TRANS_LOAD].nextStage = FETCH_LOAD;
        this->pipeline[ISSUE_STORE].nextStage = GEN_STORE;
        this->pipeline[GEN_STORE].nextStage = TRANS_STORE;

        for (int i = 0; i < 5; i++) this->connIds[i] = -1;
    }
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~LoadStoreUnit() {
        this->ldReqs.Deallocate();
        this->stReqs.Deallocate();
    }
};

#endif  // SINUCA3_LSU_HPP_
