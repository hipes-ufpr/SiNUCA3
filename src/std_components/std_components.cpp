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
 * @file std_components.cpp
 * @brief Declaration of the std components.
 */

#include <cstdlib>
#include <cstring>

#include "../sinuca3.hpp"
#include "cores/simple_core.hpp"
#include "engine_debug_component.hpp"
#include "fetch/queues.hpp"
#include "memory/simple_memory.hpp"
#include "predictors/interleavedBTB.hpp"
#include "predictors/ras.hpp"

using namespace sinuca;

engine::Linkable* sinuca::CreateDefaultComponentByClass(const char* name) {
#ifndef NDEBUG
    COMPONENT(EngineDebugComponent);
#endif

    COMPONENT(SimpleMemory);
    COMPONENT(SimpleCore);
    COMPONENT(Ras);
    COMPONENT(BranchTargetBuffer);
    COMPONENT(MemoryQueue);
    COMPONENT(PredictorQueue);
    COMPONENT(InstructionQueue);

    return NULL;
}
