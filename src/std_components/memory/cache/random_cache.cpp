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
 * @file random_cache.cpp
 * @brief Implementation of a cache using Random as replacement policy.
 */

#include "random_cache.hpp"

#include <cassert>

#include "../../../utils/logging.hpp"
#include "cache.hpp"

RandomCache::RandomCache() : numberOfRequests(0) {}

RandomCache::~RandomCache() {}

bool RandomCache::Read(unsigned long addr, CacheEntry **result) {}

void RandomCache::Write(unsigned long addr, unsigned long value) {}

void RandomCache::Clock() {
    SINUCA3_DEBUG_PRINTF("%p: CacheNWay Clock!\n", this);
    long numberOfConnections = this->GetNumberOfConnections();
    sinuca::MemoryPacket packet;
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

void RandomCache::Flush(){}

void RandomCache::PrintStatistics(){}

int RandomCache::FinishSetup(){
    return this->c.FinishSetup();
}

int RandomCache::SetConfigParameter(const char *parameter,
                               sinuca::config::ConfigValue value){
                                   return this->c.SetConfigParameter(parameter, value);
                               }
