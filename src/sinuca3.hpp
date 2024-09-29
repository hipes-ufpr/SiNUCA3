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

//
// This header has all public API users may want to use. This mainly consists of
// virtual classes:
// - Prefetch;
// - MemoryComponent;
// - BranchPredictor;
// - BranchTargetPredictor;
// - ReorderBuffer;
//

#include <cstdio>

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

#define SINUCA3_HPP_
#endif  // SINUCA3_HPP_
