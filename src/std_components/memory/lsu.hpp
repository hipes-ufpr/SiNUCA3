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
    bool stall;
};

/** @brief Check lsu.hpp documentation for details. */
class LoadStoreUnit : public Component<LSUPacket> {
  private:
    Component<MemoryPacket>* tlb;
    Component<MemoryPacket>* cache;
    Component<DebugPacketLSU>* sendTo; /* Change to rob pointer later */
    int connIds[5];

    /* Queue with pointers to pending load requests */
    CircularBuffer ldReqs;
    /* Queue with pointers to pending store requests */
    CircularBuffer stReqs;
    /*  Table for load requests */
    std::vector<pair::Pair<long, MemoryRequest*> > ldTable;
    /*  Table for store requests */
    std::vector<pair::Pair<long, MemoryRequest*> > stTable;

    /* Number of translations not yet received by st unit */
    long stUnitwaitingFor;
    /* Occupation of the store buffer */
    long stBufferOccupation;
    /* Global sequence number */
    long globalSeq;

    /* Configuration knobs */
    long stBufferSize;
    bool ldBypassingEnabled;
    bool ldForwardingEnabled;

    /* Use in statistics */
    int finishedStores;
    int finishedLoads;
    int requestedLoads;
    int requestedStores;

    /* Pipeline data */
    PipelineData issueLoad;
    PipelineData issueStore;
    PipelineData genLoad;
    PipelineData genStore;
    PipelineData transLoad;
    PipelineData transStore;
    PipelineData fetchLoad;

    /* Get responses */
    void ReceiveCommit();
    void ReceiveUpdate();
    void ReceiveTranslation();
    void ReceiveFetchedData();

    /* Get new requests. Add operation to table and enqueue request. */
    void ReceiveRequests();
    /* Run the pipeline calls. */
    void RunPipeline();
    /* Invalidate next cycle registers. */
    void ClearNext();
    /* Update registers. */
    void UpdateRegisters();

    /* Handle load and store completions. */
    void OnLoadCompletion(MemoryRequest* req);
    void OnStoreFinish(MemoryRequest* req);
    void OnStoreCompletion(MemoryRequest* req);

    /* Pipeline stages */
    void IssueLoadRequest();
    void IssueStoreRequest();
    void GenerateLoadAddress();
    void TranslateLoadAddress();
    void FetchLoadData();
    void GenerateStoreAddress();
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

        this->issueLoad.reg.isValid = false;
        this->issueStore.reg.isValid = false;
        this->genLoad.reg.isValid = false;
        this->genStore.reg.isValid = false;
        this->transLoad.reg.isValid = false;
        this->transStore.reg.isValid = false;
        this->fetchLoad.reg.isValid = false;

        this->issueLoad.stall = false;
        this->issueStore.stall = false;
        this->genLoad.stall = false;
        this->genStore.stall = false;
        this->transLoad.stall = false;
        this->transStore.stall = false;
        this->fetchLoad.stall = false;

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
