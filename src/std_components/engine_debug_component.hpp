#ifndef SINUCA3_ENGINE_ENGINE_DEBUG_COMPONENT_HPP_
#define SINUCA3_ENGINE_ENGINE_DEBUG_COMPONENT_HPP_

#ifndef NDEBUG

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
 * @file engine_debug_component.hpp
 * @brief API of the EngineDebugComponent. THIS FILE SHALL ONLY BE INCLUDED BY
 * CODE PATHS THAT ONLY COMPILE IN DEBUG MODE.
 */

#include <sinuca3.hpp>

/**
 * @brief A component that serves to debug the engine itself.
 * @details The component will log with SINUCA3_DEBUG_PRINTF it's parameters
 * along with their values. If passed the parameter "failNow" with any value,
 * the method SetConfigParameter will return with failure. If passed the
 * parameter "failOnFinish", the method FinishSetup will return with failure.
 */
class EngineDebugComponent : public Component<InstructionPacket> {
  private:
    EngineDebugComponent*
        other; /** @brief Another component to test sending messages. */
    Component<FetchPacket>*
        fetch; /** @brief Another component to test fetching instructions. */
    int otherConnectionID; /** @brief Connection ID for `other`. */
    int fetchConnectionID; /** @brief Connection ID for `fetch`. */
    bool send; /** @brief Tells wether we already sent a message to other. */
    bool shallFailOnFinish; /** @brief If true, fails at the FinishSetup method
                               to test the engine handling of failures. */

    /**
     * @brief Prints a config value along with the parameter name. This is
     * recursive for arrays.
     * @param parameter self-explanatory.
     * @param value self-explanatory.
     * @param indent The indentation level for printing, increased every
     * recursion.
     */
    void PrintConfigValue(const char* parameter, ConfigValue value,
                          unsigned char indent = 0);

  public:
    inline EngineDebugComponent()
        : other(NULL),
          fetch(NULL),
          otherConnectionID(-1),
          fetchConnectionID(-1),
          send(false),
          shallFailOnFinish(false) {}

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~EngineDebugComponent();
};

#endif  // NDEBUG

#endif  // SINUCA3_ENGINE_ENGINE_DEBUG_COMPONENT_HPP_
