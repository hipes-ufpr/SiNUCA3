#ifndef SINUCA3_SIMPLE_CORE_HPP_
#define SINUCA3_SIMPLE_CORE_HPP_

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
 * @file simple_core.hpp
 * @details Public API of the SimpleCore, a testing core that executes
 * everything in a single clock cycle.
 */

#include "../sinuca3.hpp"

/**
 * @details SimpleCore executes everything in a single cycle. You can optionally
 * set an instructionMemory and a dataMemory pointers to components that extend
 * Component<MemoryPacket>.
 */
class SimpleCore : public sinuca::Component<sinuca::InstructionPacket> {
  private:
    sinuca::Component<sinuca::MemoryPacket>* instructionMemory;
    sinuca::Component<sinuca::MemoryPacket>* dataMemory;

    int instructionConnectionID;
    int dataConnectionID;
    unsigned long numFetchedInstructions;

  public:
    inline SimpleCore()
        : instructionMemory(NULL),
          dataMemory(NULL),
          numFetchedInstructions(0) {}
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();
    ~SimpleCore();
};

#endif  // SINUCA3_SIMPLE_CORE_HPP_
