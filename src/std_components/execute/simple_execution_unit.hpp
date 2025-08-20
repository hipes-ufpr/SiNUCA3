#ifndef SINUCA3_SIMPLE_EXECUTION_UNIT_HPP_
#define SINUCA3_SIMPLE_EXECUTION_UNIT_HPP_

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
 * @file simple_execution_unit.hpp
 * @details Public API of the SimpleExecutionUnit: component for SiNUCA3
 * which just responds immediatly for every request. I.e., the
 * perfect execution unit: executes any instruction instantly!
 */

#include <sinuca3.hpp>

/**
 * @brief The SimpleExecutionUnit simply executes any instruction immediatly.
 */
class SimpleExecutionUnit : public Component<InstructionPacket> {
  private:
    unsigned long
        numberOfInstructions; /** @brief The number of instructions executed. */

  public:
    inline SimpleExecutionUnit() : numberOfInstructions(0) {};
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();
    ~SimpleExecutionUnit();
};

#endif  // SINUCA3_SIMPLE_EXECUTION_UNIT_HPP_
