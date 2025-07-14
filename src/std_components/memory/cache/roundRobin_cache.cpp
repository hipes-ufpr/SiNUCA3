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
 * @file roundRobin_cache.cpp
 * @brief Implementation of a cache using RoundRobin as replacement policy.
 */

#include "roundRobin_cache.hpp"

#include <cassert>

#include "../../../utils/logging.hpp"
#include "cache.hpp"

RoundRobinCache::RoundRobinCache(){

}

RoundRobinCache::~RoundRobinCache(){

}

bool RoundRobinCache::Read(unsigned long addr, CacheEntry **result){

}

void RoundRobinCache::Write(unsigned long addr, unsigned long value){

}
