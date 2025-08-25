#ifndef SINUCA3_PSEUDO_LRU_CACHE_HPP_
#define SINUCA3_PSEUDO_LRU_CACHE_HPP_

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
 * @file pseudo_lru_cache.hpp
 * @details WP! A cache using PSEUDO_LRU as replacement policy.
 */

#include <sinuca3.hpp>
#include <utils/cache.hpp>

struct plruNode {
    unsigned char direction;  // 0 = left, 1 = right
    int l;                    // range left
    int r;                    // range right
};

class PseudoLRUCache : public Component<MemoryPacket> {
  public:
    PseudoLRUCache();
    virtual ~PseudoLRUCache();

    virtual bool Read(unsigned long addr, CacheEntry **result);
    virtual void Write(unsigned long addr, unsigned long value);

    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char *parameter,
                                   ConfigValue value);

  private:
    Cache cache;
    struct plruNode **plruTree;
    unsigned long numberOfRequests;
};

#endif
