//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
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
 * @file pseudo_lru_cache.cpp
 * @brief Implementation of a cache using PSEUDO_LRU as replacement policy.
 */

#include "pseudo_lru_cache.hpp"

#include <cassert>
#include <cstring>
#include <utils/cache.hpp>
#include <utils/logging.hpp>

PseudoLRUCache::PseudoLRUCache() : numberOfRequests(0) {}

PseudoLRUCache::~PseudoLRUCache() {
    delete[] this->plruTree[0];
    delete[] this->plruTree;
}

bool PseudoLRUCache::Read(unsigned long addr, CacheEntry **result) {
    // Get entry early to find set and way.
    int exist = this->cache.GetEntry(addr, result);
    int set = (*result)->i;
    int way = (*result)->j;

    // Update nodes because we acessed.
    int current = 0;
    while (this->plruTree[set][current].r - this->plruTree[set][current].l >
           1) {
        int mid =
            (this->plruTree[set][current].l + this->plruTree[set][current].r) /
            2;
        unsigned char direction = (way <= mid) ? 0 : 1;

        this->plruTree[set][current].direction =
            !direction;  // Switch this node to opposite direction.

        // Go to next node in the path
        current = 2 * current + 1 + direction;
    }

    return exist;
}

void PseudoLRUCache::Write(unsigned long addr, unsigned long value) {
    unsigned long tag = this->cache.GetTag(addr);
    unsigned long set = this->cache.GetIndex(addr);
    int j = 0;

    CacheEntry *entry;
    if (this->cache.FindEmptyEntry(addr, &entry)) {
        *entry = CacheEntry(entry, tag, set, value);
        return;
    }

    while (j < this->cache.numWays - 1) {
        unsigned char old_direction =
            this->plruTree[set][j].direction;  // 0 is left, 1 is right
        this->plruTree[set][j].direction =
            !this->plruTree[set][j]
                 .direction;  // Switch this node to opposite direction.
        j = 2 * j + 1 + old_direction;
    }

    int way = j - (this->cache.numWays - 1);
    CacheEntry *plruEntry = &this->cache.entries[set][way];
    *plruEntry = CacheEntry(plruEntry, tag, set, value);
}

void PseudoLRUCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: PseudoLRUCache Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    MemoryPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            ++this->numberOfRequests;

            CacheEntry *result;

            // We dont have (and dont need) data to send back, so a
            // MemoryPacket is send back to to signal
            // that the cache's operation has been completed.

            // Read() returns true if it was hit.
            if (this->Read(packet, &result)) {
                this->SendResponseToConnection(i, &packet);
            } else {
                // TODO
                // Call the page-table walker.
                // This is a memory access, if the memory is perfect,
                // there is no penalty, so we still need to decide what happens
                // in this case.
                //
                // Then, call Write() to insert a new addr in cache
                // according to the replacement policy.
            }
        }
    }
}

void PseudoLRUCache::Flush() {}

void PseudoLRUCache::PrintStatistics() {}

int PseudoLRUCache::FinishSetup() {
    if (!(this->cache.numWays % 2)) {
        SINUCA3_ERROR_PRINTF(
            "Pseudo LRU Cache with an odd quantity of ways is not implemented "
            "yet.\n");
        return 1;
    }

    if (this->cache.FinishSetup()) return 1;

    this->plruTree = new struct plruNode *[this->cache.numSets];
    int n = this->cache.numSets * (this->cache.numWays - 1);
    this->plruTree[0] = new struct plruNode[n];
    memset(this->plruTree[0], 0, n * sizeof(struct plruNode));
    for (int i = 1; i < this->cache.numSets; ++i) {
        this->plruTree[i] = this->plruTree[0] + (i * this->cache.numWays);
    }

    // Inicialize tree node's range
    int nodeCount = this->cache.numWays - 1;
    int leafNodeCount = this->cache.numWays / 2;
    int internalNodeCount = leafNodeCount - 1;
    for (int i = 0; i < this->cache.numSets; ++i) {
        this->plruTree[i][0].l = 0;
        this->plruTree[i][0].r = this->cache.numWays - 1;

        struct plruNode *nodes = this->plruTree[i];
        for (int j = 0; j < internalNodeCount; ++j) {
            int mid = (nodes[j].l + nodes[j].r) / 2;

            int leftChild = 2 * j + 1;
            int rightChild = 2 * j + 2;
            if (leftChild < nodeCount) {
                nodes[leftChild].l = nodes[j].l;
                nodes[leftChild].r = mid;
            }
            if (rightChild < nodeCount) {
                nodes[rightChild].l = mid + 1;
                nodes[rightChild].r = nodes[j].r;
            }
        }
    }

    return 0;
}

int PseudoLRUCache::SetConfigParameter(const char *parameter,
                                       ConfigValue value) {
    return this->cache.SetConfigParameter(parameter, value);
}
