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
#include <sinuca3.hpp>

#ifndef NDEBUG
#include <std_components/engine_debug_component.hpp>
#include <std_components/debug/memory/itlb_debug_component.hpp>
#endif  // NDEBUG

#include <std_components/cores/simple_core.hpp>
#include <std_components/execute/simple_execution_unit.hpp>
#include <std_components/fetch/fetcher.hpp>
#include <std_components/memory/simple_instruction_memory.hpp>
#include <std_components/memory/simple_memory.hpp>
#include <std_components/misc/queues.hpp>
#include <std_components/predictors/hardwired_predictor.hpp>
#include <std_components/predictors/interleavedBTB.hpp>
#include <std_components/predictors/ras.hpp>
#include <std_components/memory/itlb.hpp>
#include <std_components/trace_dumper_component.hpp>

Linkable* CreateDefaultComponentByClass(const char* name) {
#ifndef NDEBUG
    COMPONENT(EngineDebugComponent);
    COMPONENT(iTLBDebugComponent);
#endif

    COMPONENT(SimpleMemory);
    COMPONENT(SimpleInstructionMemory);
    COMPONENT(SimpleCore);
    COMPONENT(Ras);
    COMPONENT(BranchTargetBuffer);
    COMPONENT(MemoryQueue);
    COMPONENT(PredictorQueue);
    COMPONENT(InstructionQueue);
    COMPONENT(Fetcher);
    COMPONENT(SimpleExecutionUnit);
    COMPONENT(HardwiredPredictor);
    COMPONENT(iTLB);
    COMPONENT(TraceDumperComponent);

    return NULL;
}
