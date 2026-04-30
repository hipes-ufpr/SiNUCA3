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

#define MOD2(a, b) ((a) & ((b) - 1))
#define DIV2(a, b) ((a) >> __builtin_ctzl(b))

enum LSUPacketType {
    LSUPacketTypeLoadRequest,
    LSUPacketTypeStoreRequest,
    LSUPacketTypeInstCommit
};

const int TLB_SOLVE_LOAD_ADDRESS = 0;
const int TLB_SOLVE_STORE_ADDRESS = 1;
const int CACHE_SOLVE_LOAD_DATA = 2;
const int CACHE_SOLVE_STORE_DATA = 3;
const int SEND_TO_RESPONSE = 4;

struct LSUPacket {
    union {
        struct {
            unsigned long vtAddr; /** @brief Virtual address. */
            int size; /** @brief Size of the memory operation in bytes. */
        } operation;  /** @brief On load or store request. */
        struct {
            unsigned long vtAddr; /** @brief Virtual address. */
        } commit; /** @brief On instruction commit acknowledgment. */
    };
    LSUPacketType type;
};

struct StoreRequest {
    unsigned long virtualAddress;
    unsigned long physicalAddress;
    int accessSize;
    bool stateIsFinished;
    bool stateIsCommited;
};

struct LoadRequest {
    unsigned long virtualAddress;
    unsigned long physicalAddress;
    int accessSize;
    bool waitingTranslation;
};

/** @brief Check lsu.hpp documentation for details. */
class LoadStoreUnit : public Component<LSUPacket> {
  private:
    Component<MemoryPacket>* tlb;
    Component<MemoryPacket>* cache;
    Component<MemoryPacket>* sendTo; /* Change to rob pointer later */

    CircularBuffer pendingRequestsQueue;
    std::vector<Pair<unsigned long, LoadRequest> > loadsTable;
    std::vector<Pair<unsigned long, StoreRequest> > storesTable;

    /* Establish different connection IDs to avoid message confusion. */
    CircularBuffer pendingResponses[5];
    int connId[5];
    int sendToConnId;

    /* Loads processed */
    unsigned long loadsCounter;
    /* Stores processed */
    unsigned long storesCounter;
    /* Stores in flight. A leading load must wait for these to finish. */
    int unresolvedStores;
    /* A leading store cannot change to finished state if this variable is not
     * zero. */
    int loadsNotForwardedToFetchStage;
    /* Number of finished/completed stores in the buffer */
    long storeBufferOccupation;

    /* Configuration knobs */
    long storeBufferSize;
    bool loadBypassingEnabled;
    bool loadForwardingEnabled;

    /* Use in statistics */
    int completedStoreRequests;
    int finishedLoadRequests;

    /** @brief Check if st. unit pipe stages methods must not be called. */
    bool IsStoreUnitStalled();
    /** @brief Receive acknowledgment from the reorder buffer. */
    void CheckStoreCommit();
    /** @brief Receive new operations to process. */
    void ReceiveNewRequests();
    /** @brief Process pending requests. */
    void ProcessRequests();
    /** @brief Receive responses from connected components. */
    void ReceiveResponses();
    /** @brief Check memory updates. */
    void CheckMemoryUpdate();

    /* Load pipeline stages */
    void GenerateLoadAddress(unsigned long vtAddress, bool ready);
    void TranslateLoadAddress(unsigned long vtAddress, bool ready);
    void FetchLoadData(unsigned long physAddress, bool ready);

    /* Store pipeline stages */
    void GenerateStoreAddress(unsigned long address, bool ready);
    void TranslateStoreAddress(unsigned long address, bool ready);

    /* Optimizations */
    /** @brief Check if address aliasing exists. */
    bool IsLoadBypassingPossible(unsigned long address, int size);
    /** @brief Check if forwarding store data is possible. */
    bool IsLoadForwardingPossible(unsigned long address, int size);

  public:
    LoadStoreUnit()
        : tlb(NULL),
          cache(NULL),
          sendTo(NULL),
          loadsCounter(0),
          storesCounter(0),
          unresolvedStores(0),
          loadsNotForwardedToFetchStage(0),
          storeBufferOccupation(0),
          storeBufferSize(16),
          loadBypassingEnabled(false),
          loadForwardingEnabled(false),
          completedStoreRequests(0),
          finishedLoadRequests(0) {
        this->pendingRequestsQueue.Allocate(0, sizeof(LSUPacket));
    }
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~LoadStoreUnit() {
        this->pendingRequestsQueue.Deallocate();
        for (int i = 0; i < 5; ++i) this->pendingResponses[i].Deallocate();
    }
};

#endif  // SINUCA3_LSU_HPP_
