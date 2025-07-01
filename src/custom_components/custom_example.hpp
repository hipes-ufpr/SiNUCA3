#ifndef SINUCA3_CUSTOM_EXAMPLE_HPP_
#define SINUCA3_CUSTOM_EXAMPLE_HPP_

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
 * @file custom_example.hpp
 * @brief The public API of an example custom component, to show how to create
 * them.
 */

#include "../sinuca3.hpp"

/**
 * @details All custom components should inherit from sinuca::Component<T>,
 * where T is the type of messages it receives. When no messages are received,
 * one may just use int as a placeholder.
 */
class CustomExample : public sinuca::Component<int> {
  public:
    /** @brief The engine calls this method each clock cycle. */
    virtual void Clock();
    /**
     * @details This method is called after the config file is and all
     * parameters are set, so to finish any setup required by the component.
     * Non-zero should be returned if any problem occurred (e.g., a required
     * configuration parameter was not provided). The component is responsible
     * for printing a proper error message describing what happened.
     * @returns Non-zero on error, 0 otherwise.
     */
    virtual int FinishSetup();
    /**
     * @details This method is called if the config file defines a configuration
     * parameter for the component. Again, non-zero should be returned on error
     * and the component is responsible for printing a proper error message.
     * @param parameter The name of the parameter provided.
     * @param value The value of the parameter provided.
     * @returns Non-zero on error, 0 otherwise.
     */
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    /**
     * @brief This method is called by the engine when a flush should occur.
     * It's always called at the beggining of the cycle. Components can request
     * a flush calling sinuca::ENGINE->flush().
     */
    virtual void Flush();

    /**
     * @brief This method is called at the end of the simulation for each
     * component to print it's useful statistics. A simulator is worth nothing
     * if you cannot gather any data from it.
     */
    virtual void PrintStatistics();
};

#endif  // SINUCA3_CUSTOM_EXAMPLE_HPP_
