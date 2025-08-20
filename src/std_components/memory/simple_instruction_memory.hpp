#ifndef SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_
#define SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file simple_instruction_memory.hpp
 * @details Public API of the SimpleInstructionMemory: component for SiNUCA3
 * which just responds immediatly for every request (of instructions). I.e., the
 * perfect (instruction) memory: big and works at the light speed!
 */

#include <sinuca3.hpp>

/**
 * @brief component for SiNUCA3 which just responds immediatly for every request
 * (of instructions). I.e., the perfect (instruction) memory: big and works at
 * the light speed!
 * @details The SimpleInstructionMemory accepts the `sendTo` parameter as a
 * Component<InstructionPacket>. If it's set, the memory forwards all responses
 * to it instead of answering in the response channel. Beware that it connects
 * to `sendTo` without limits, so if that component has a limited throughput,
 * the buffer may start to eat all the system's memory and instructions will be
 * waiting idling.
 */
class SimpleInstructionMemory : public Component<InstructionPacket> {
  private:
    Component<InstructionPacket>* sendTo;
    unsigned long numberOfRequests;
    int sendToID;

  public:
    inline SimpleInstructionMemory() : numberOfRequests(0) {};
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();
    ~SimpleInstructionMemory();
};

#endif  // SINUCA3_SIMPLE_INSTRUCTION_MEMORY_HPP_
