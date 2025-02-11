#ifndef SINUCA3_HPP_

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
 * @file sinuca3.hpp
 * @details This header has all public API users may want to use. Some things
 * are actually imported from other headers.
 */

#include <cstring>

// These pragmas make clangd don't warn about unused includes when using just
// sinuca3.hpp to include the below files.
#include "config/config.hpp"     // IWYU pragma: export
#include "engine/component.hpp"  // IWYU pragma: export
#include "engine/linkable.hpp"

namespace sinuca {

#define COMPONENT(type) \
    if (!strcmp(name, #type)) return new type

#define COMPONENTS(components)                                         \
    namespace sinuca {                                                 \
    engine::Linkable* CreateCustomComponentByClass(const char* name) { \
        components;                                                    \
        return 0;                                                      \
    }                                                                  \
    }

/** @brief Don't call, used by the SimulatorBuilder. */
engine::Linkable* CreateDefaultComponentByClass(const char* name);
/** @brief Don't call, used by the SimulatorBuilder. */
engine::Linkable* CreateCustomComponentByClass(const char* name);

/**
 * @brief Exchanged between the engine, FirstStagePipeline and other pipeline
 * stages.
 */
struct InstructionPacket {};

/**
 * @brief Used by SimpleMemory.
 */
struct MemoryPacket {};

}  // namespace sinuca

#define SINUCA3_HPP_
#endif  // SINUCA3_HPP_
