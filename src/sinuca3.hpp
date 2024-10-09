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
 * @details This header has all public API users may want to use. This mainly
 * consists of virtual classes:
 * - Prefetch;
 * - MemoryComponent;
 * - BranchPredictor;
 * - BranchTargetPredictor;
 * - ReorderBuffer;
 */

#include <cstdio>
#include <cstring>

namespace sinuca {

enum ConfigValueType {
    ConfigValueTypeString,
    ConfigValueTypeDouble,
    ConfigValueTypeInteger,
    ConfigValueTypeObjectReference,
};

// Pre-declaration for ConfigValue.
class Component;

struct ConfigValue {
    ConfigValueType type;
    union {
        char* string;
        double doub;
        long integer;
        Component* componentReference;
    } value;
};

class Component {
  public:
    virtual int SetConfigParameter(const char* parameter,
                                   ConfigValue value) = 0;
};

#define COMPONENT(type) \
    if (!strcmp(name, #type)) return new type

#define COMPONENTS(components)                                 \
    namespace sinuca {                                         \
    Component* CreateCustomComponentByName(const char* name) { \
        components;                                            \
        return 0;                                              \
    }                                                          \
    }

Component* CreateCustomComponentByName(const char* name);

// Pre-defines for MemoryPacket.
class MemoryRequester;
class MemoryComponent;

struct MemoryPacket {
    MemoryRequester* respondTo;
    MemoryComponent* responser;
};

struct InstructionPacket {};

// A MemoryComponent receives messages from MemoryRequesters.
class MemoryComponent {
  public:
    virtual void Request(MemoryPacket packet) = 0;
};

class MemoryRequester {
  public:
    virtual void Response(MemoryPacket packet) = 0;
};

class Prefetch {};

class BranchPredictor {};

class BranchTargetPredictor {};

class ReorderBuffer {};

}  // namespace sinuca

#define SINUCA3_HPP_
#endif  // SINUCA3_HPP_
