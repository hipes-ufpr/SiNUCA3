#ifndef SINUCA3_SIMPLE_MEMORY_HPP_
#define SINUCA3_SIMPLE_MEMORY_HPP_

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
 * @file simple_memory.hpp
 * @details Public API of the SimpleMemory: component for SiNUCA3 which just
 * responds immediatly for every request. I.e., the perfect memory: big and
 * works at the light speed!
 */

#include "../sinuca3.hpp"

/**
 * @details SimpleMemory is a MemoryComponent that just responds immediatly for
 * every request. I.e., it's the perfect memory: big and works at the light
 * speed!
 */
class SimpleMemory : public sinuca::Component<sinuca::MemoryPacket> {
  public:
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    ~SimpleMemory();
};

#endif  // SINUCA3_SIMPLE_MEMORY_HPP_
