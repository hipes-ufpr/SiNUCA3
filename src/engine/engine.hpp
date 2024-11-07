#ifndef SINUCA3_ENGINE_HPP_
#define SINUCA3_ENGINE_HPP_

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
 * @file engine.hpp
 * @brief Public API of the simulation engine.
 */

#include "../sinuca3.hpp"
#include "component.hpp"
#include "linkable.hpp"

namespace sinuca {
namespace engine {

/** @brief The engine itself. */
class Engine {
  private:
    /**
     * @brief This component receives the instruction packets from the engine.
     */
    Component<InstructionPacket>* root;

  public:
    /**
     * @brief Don't call this method.
     * @details After reading the configuration file, the simulator calls this
     * method to set which component should receive the instruction packets from
     * the engine. This component should inherit from
     * Component<InstructionPacket>.
     * @param root A pointer to a Component<InstructionPacket>.
     * @returns Non-zero if root is not a pointer to
     * Component<InstructionPacket>. 0 otherwise. Trying to start the simulation
     * after this method returns non-zero is undefined behaviour.
     */
    int AddRoot(Linkable* root);
    /**
     * @brief Self-explanatory. Should be called only after AddRoot returned 0.
     * Causes undefined behaviour otherwise.
     * @returns Non-zero if the simulation stopped because of a problem. 0 if it
     * stopped normally.
     */
    int Simulate();
};

}  // namespace engine
}  // namespace sinuca

#endif  // SINUCA3_ENGINE_HPP_
