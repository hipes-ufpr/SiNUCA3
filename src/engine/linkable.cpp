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
 * @file linkable.cpp
 * @brief Implementation of the Linkable class.
 */

#include "linkable.hpp"

sinuca::engine::Linkable::Linkable(long bufferSize, long numberOfBuffers)
    : bufferSize(bufferSize), numberOfBuffers(numberOfBuffers) {}

sinuca::engine::Linkable::~Linkable() { delete[] this->buffers; }

void sinuca::engine::Linkable::PreClock() {}
void sinuca::engine::Linkable::PosClock() {}
