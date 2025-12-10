#ifndef SINUCA3_RANDOM_CACHE_HPP_
#define SINUCA3_RANDOM_CACHE_HPP_

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
 * @file random_cache.hpp
 * @details WP! A cache using Random as replacement policy.
 */

#include <sinuca3.hpp>
#include <utils/cache/replacement_policy.hpp>

namespace ReplacementPolicies {

const unsigned int SEED = 0;

class Random : public ReplacementPolicy {
  public:
    Random(int numSets, int numWays);
    virtual ~Random() {};

    virtual void Acess(CacheLine* entry);
    virtual void SelectVictim(unsigned long tag, unsigned long index,
                              int* resultSet, int* resultWay);
};

}  // namespace ReplacementPolicies

#endif
