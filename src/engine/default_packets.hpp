#ifndef SINUCA3_DEFAULT_PACKETS_HPP_
#define SINUCA3_DEFAULT_PACKETS_HPP_

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
 * @file default_packets.hpp
 * @brief Standard message types.
 */

#include <cstdlib>

namespace sinuca {

/**
 * @brief Exchanged between the engine and components.
 */
struct InstructionPacket {
    const char* opcode;
    unsigned long address;
    unsigned char size;
};

/**
 * @brief The core shall respond this to inform the engine to stall the
 * fetching for the next cycle.
 */
const InstructionPacket STALL_FETCHING = {NULL, 0, 0};

/**
 * @brief Used by SimpleMemory.
 */
struct MemoryPacket {};

}  // namespace sinuca

#endif  // SINUCA3_DEFAULT_PACKETS_HPP_
