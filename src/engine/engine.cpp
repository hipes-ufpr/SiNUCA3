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
 * @file engine.cpp
 * @brief Implementation of the simulation engine.
 */

#include "engine.hpp"

#include "../utils/logging.hpp"
#include "component.hpp"
#include "linkable.hpp"

int sinuca::engine::Engine::AddRoot(sinuca::engine::Linkable* root) {
    this->root = dynamic_cast<Component<InstructionPacket>*>(root);
    return this->root == NULL;
}

int sinuca::engine::Engine::Simulate() {
    SINUCA3_LOG_PRINTF("Engine is running!\n");
    return 0;
}
