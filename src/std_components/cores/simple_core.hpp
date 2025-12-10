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

#include <sinuca3.hpp>

/**
 * @details SimpleCore fetches an instruction from the required parameter
 * `fetching`, a Component<InstructionPacket> and optionally queries two
 * memories (without caring with the result at all) with the instruction. The
 * two memories are passed as the Component<MemoryPacket> parameters
 * `instructionMemory` and `dataMemory`.
 */
class SimpleCore : public Component<InstructionPacket> {
  private:
    Component<MemoryPacket>*
        instructionMemory;               /** @brief The instruction memory. */
    Component<MemoryPacket>* dataMemory; /** @brief The data memory. */
    Component<FetchPacket>* fetching;    /** @brief The fetching. */

    unsigned long numFetchedInstructions; /** @brief The number of fetched
                                             instructions. */
    int instructionConnectionID;          /** @brief The connection ID of
                                             instructionMemory. */
    int dataConnectionID;     /** @brief The connection ID of dataMemory. */
    int fetchingConnectionID; /** @brief The connection ID of fetching. */

  public:
    inline SimpleCore()
        : instructionMemory(NULL),
          dataMemory(NULL),
          fetching(NULL),
          numFetchedInstructions(0) {}
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();
    ~SimpleCore();
};

#endif  // SINUCA3_SIMPLE_CORE_HPP_
