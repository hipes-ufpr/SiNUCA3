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
 * @file simple_memory.cpp
 * @brief Implementation of the SimpleMemory.
 */

#include "simple_memory.hpp"

int SimpleMemory::SetConfigParameter(const char* parameter,
                                     sinuca::config::ConfigValue value) {
    (void)parameter;
    (void)value;

    return 1;
}

int SimpleMemory::FinishSetup() { return 0; }

void SimpleMemory::Clock() {}

SimpleMemory::~SimpleMemory() {}
