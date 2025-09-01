#ifndef SINUCA3_SIMPLE_CACHE_HPP_
#define SINUCA3_SIMPLE_CACHE_HPP_

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
 * @file simple_cache.hpp
 * @details Implementation of a SimpleCache without latency or coherence
 * protocol.
 */

#include <sinuca3.hpp>

#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include "utils/cache/cache.hpp"

class SimpleCache : public Component<MemoryPacket> {
  public:
    SimpleCache() : numberOfRequests(0) {};
    virtual ~SimpleCache() {};

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();

  private:
    Cache cache;
    unsigned int numberOfRequests;
};

#endif
