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

template <typename T>
bool EraseElem(std::vector<T>* buffer, T elem) {
    for (size_t i = 0; i < buffer->size(); i++) {
        if (buffer->at(i) == elem) {
            buffer->at(i) = buffer->back();
            buffer->pop_back();
            return true;
        }
    }
    return false;
}

template <typename T>
void PushBackElem(std::vector<T>* buffer, T elem) {
    buffer->push_back(elem);
}

/** @brief Check lsu.hpp documentation for details. */
class LoadStoreUnit : public Component<LSUPacket> {
  private:
    Component<MemoryPacket>* tlb;
    Component<MemoryPacket>* cache;
    Component<MemoryPacket>* sendTo; /* Change to rob pointer later */

    CircularBuffer pendingRequestsQueue;
    /** @brief Buffer for finished store requests and their sizes. */
    std::vector<unsigned long> finishedStoreBuffer;
    /** @brief Buffer for completed store requests and their sizes. */
    std::vector<unsigned long> completedStoreBuffer;
    /** @brief Maps store addresses to their sizes. */
    std::vector<Pair<unsigned long, int> > storeAddressToSize;
    /** @brief Maps an identifier with the number of pending responses */
    std::vector<Pair<unsigned int, int> > loadIdentifierToDebt;
    /** @brief Split loads have the same unique identifier. */
    std::vector<Pair<unsigned long, Pair<unsigned int, int> > >
        loadAddressToIdAndSize;

    /** @brief Used when creating new load identifiers. */
    unsigned int globalLoadIdentifier;

    /* Establish different connection IDs to avoid message confusion. */
    CircularBuffer pendingResponses[5];
    int connId[5];
    int sendToConnId;

    /* Loads must wait for unresolved store address requests */
    int unresolvedStoreAddressRequests;
    /** @brief Tracks the usage of cache read ports per cycle. */
    int cacheReadPortUsage;
    /** @brief Tracks the usage of cache write ports. */
    int cacheWritePortUsage;
    /** @brief Tracks the usage of tlb read ports. */
    int tlbReadPortUsage;
    /** @brief Tracks the usage of tlb write ports. */
    int tlbWritePortUsage;

    /* Configuration knobs */
    long cacheLineSize; /** @brief Needs to be a power of 2. */
    long numberOfTlbReadPorts;
    long numberOfTlbWritePorts;
    long numberOfCacheReadPorts;
    long numberOfCacheWritePorts;
    long completedStoreBufferSize;
    long finishedStoreBufferSize;
    bool loadBypassingEnabled;
    bool loadForwardingEnabled;

    /* Use in statistics */
    int finishedStoreRequests;
    int completedStoreRequests;
    int totalLoadRequests;
    int totalStoreRequests;
    int finishedLoadRequests;

    /** @brief Split load if necessary and enqueue to pending queue. */
    void BuildAlignedLoadSubrequests(unsigned long address, int size);
    /** @brief Split store if necessary and enqueue to pending queue. */
    void BuildAlignedStoreSubrequests(unsigned long address, int size);
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
    /** @brief Reset port usage for the current cycle. */
    void ResetPortUsage();

    /* Load pipeline stages */
    void GenerateLoadAddress(unsigned long vtAddress, bool ready);
    void TranslateLoadAddress(unsigned long vtAddress, bool ready);
    void FetchLoadData(std::vector<unsigned long>* addresses);

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
          globalLoadIdentifier(0),
          unresolvedStoreAddressRequests(0),
          cacheLineSize(64),
          numberOfTlbReadPorts(2),
          numberOfTlbWritePorts(2),
          numberOfCacheReadPorts(4),
          numberOfCacheWritePorts(4),
          completedStoreBufferSize(16),
          finishedStoreBufferSize(16),
          loadBypassingEnabled(true),
          loadForwardingEnabled(true),
          finishedStoreRequests(0),
          completedStoreRequests(0),
          totalLoadRequests(0),
          totalStoreRequests(0),
          finishedLoadRequests(0) {
        this->pendingRequestsQueue.Allocate(0, sizeof(LSUPacket));
        this->finishedStoreBuffer.resize(this->finishedStoreBufferSize);
        this->completedStoreBuffer.resize(this->completedStoreBufferSize);
    }
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~LoadStoreUnit() {
        this->pendingRequestsQueue.Deallocate();
        int numPendingResponses =
            sizeof(this->pendingResponses) / sizeof(this->pendingResponses[0]);
        for (int i = 0; i < numPendingResponses; ++i)
            this->pendingResponses[i].Deallocate();
    }
};

#endif  // SINUCA3_LSU_HPP_
