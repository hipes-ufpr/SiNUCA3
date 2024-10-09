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
 * @file processor.cpp
 * @brief Implementation of the Processor class.
 */

#include "processor.hpp"

#include <cstdio>

#include "../sinuca3.hpp"

Processor::Processor() {}

void Processor::Process() {
    printf("Processor is working!\n");
    sinuca::MemoryPacket packet = {
        this,
        this->cache,
    };
    cache->Request(packet);
}

void Processor::Response(sinuca::MemoryPacket packet) {
    printf("Processor received memory packet back from %p\n", packet.responser);
}
