#ifndef SINUCA3_ITLB_HPP_
#define SINUCA3_ITLB_HPP_

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
 * @file itlb.hpp
 * @details Implementation of a instruction TLB.
 */

#include <sinuca3.hpp>
#include <utils/cache/cacheMemory.hpp>

#include "config/config.hpp"
#include "engine/component.hpp"
#include "utils/circular_buffer.hpp"

typedef unsigned long Address;

static const int NO_PENALTY = -1;

struct TLBRequest {
    int id;
    Address addr;
};

class iTLB : public Component<Address> {
  public:
    iTLB()
        : numberOfRequests(0),
          entries(0),
          numWays(0),
          pageSize(4096),
          currentPenalty(NO_PENALTY),
          missPenalty(0),
          cache(NULL),
          policyID(CacheMemoryNS::Unset) {};
    virtual ~iTLB() {};

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();

  private:
    unsigned int numberOfRequests;

    unsigned int entries;
    unsigned int numWays;
    unsigned int pageSize; // default 4 KiB

    long
        currentPenalty; /**< Counter to control the paying of penalties >*/
    unsigned long missPenalty; /**< Amount of cycles to idle when a
                                  miss happens >*/

    struct TLBRequest curRequest; /**< Address currently being processed by the iTLB >*/

    CircularBuffer pendingRequests; /**< Stores requests that have not yet been processed. >*/

    CacheMemory<Address>* cache;
    CacheMemoryNS::ReplacementPoliciesID policyID;

    int ConfigEntries(ConfigValue value);
    int ConfigAssociativity(ConfigValue value);
    int ConfigPolicy(ConfigValue value);
    int ConfigPenalty(ConfigValue value);
    int ConfigPageSize(ConfigValue value);
};

#endif
