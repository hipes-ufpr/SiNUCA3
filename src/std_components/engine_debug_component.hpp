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

#include "../sinuca3.hpp"

/**
 * @brief A component that serves to debug the engine itself.
 * @details The component will log with SINUCA3_DEBUG_PRINTF it's parameters
 * along with their values. If passed the parameter "failNow" with any value,
 * the method SetConfigParameter will return with failure. If passed the
 * parameter "failOnFinish", the method FinishSetup will return with failure.
 * It'll also log it's clock. Before each log, the pointer to the component is
 * printed to differentiate between multiple EngineDebugComponent.
 */
class EngineDebugComponent
    : public sinuca::Component<sinuca::InstructionPacket> {
  private:
    bool send;
    int connectionID;
    EngineDebugComponent* other;
    bool shallFailOnFinish;
    void PrintConfigValue(const char* parameter,
                          sinuca::config::ConfigValue value,
                          unsigned char indent = 0);

  public:
    inline EngineDebugComponent() : send(false), connectionID(-1), other(NULL), shallFailOnFinish(true) {}

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();

    virtual ~EngineDebugComponent();
};

#endif  // NDEBUG

#endif  // SINUCA3_ENGINE_ENGINE_DEBUG_COMPONENT_HPP_
