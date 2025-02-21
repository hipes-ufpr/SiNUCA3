#ifndef SINUCA3_ENGINE_LINKABLE_HPP_
#define SINUCA3_ENGINE_LINKABLE_HPP_

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
 * @file linkable.hpp
 * @brief Public API of the Linkable class.
 */

#include "../config/config.hpp"

namespace sinuca {
namespace engine {

/**
 * @brief Do not inherit directly from this class.
 * @details This class implements the message-passing of the components in a
 * fast and generic manner, without using templates. For this reason, it's not
 * type-safe, but serves as a generic pointer for any component type. Pointers
 * to Linkable should be avoided, and only used when it's not certain and does
 * not matter which type of message the component receives. It's not possible to
 * send a message directly to a Linkable. The Component<T> wrapper methods
 * should be used.
 */
class Linkable {
  private:
    /**
     * @brief sizeof(MessageType).
     */
    long bufferSize;
    /**
     * @brief When zero, allows an infinite amount of messages to be buffered
     * (bounded by your computer memory).
     */
    long numberOfBuffers;
    /**
     * @brief Counts how much connections other components have initialized.
     */
    long numberOfConnections;
    /**
     * @brief Array of all connections buffers.
     */
    char* buffers;  // No I'm not using vector for this.
    /**
     * @brief Array of the request buffers, swapped each cycle.
     */
    char* requestBuffers[2];
    /**
     * @brief Array of the response buffers, swapped each cycle.
     */
    char* responseBuffers[2];

  protected:
    /**
     * @brief Constructor.
     * @param bufferSize self-explanatory (message buffer size).
     * @param numberOfBuffers number of buffers per connection. When zero,
     * allows an infinite amount of messages to be buffered.
     */
    Linkable(long bufferSize, long numberOfBuffers);
    /**
     * @brief Allocates the buffers with the specified number of connections.
     * @param numberOfConnections Self-explanatory.
     */
    void AllocateBuffers(long numberOfConnections);
    /**
     * @brief Self-explanatory. The -Linkable suffix avoids clashes with the
     * higher-level wrapper methods from the Component<T> children class
     * template.
     * @param message The buffer with the message to send.
     * @param channelID The channel ID to which send the message.
     */
    int SendMessageLinkable(const char* message, int channelID);
    /**
     * @brief Self-explanatory. The -Linkable suffix avoids clashes with the
     * higher-level wrapper methods from the Component<T> children class
     * template.
     * @param message The buffer with the message to retrieve.
     * @param channelID The channel ID from which to retrieve the message.
     */
    int RetrieveResponseLinkable(const char* message, int channelID);

  public:
    /* Usually engine methods. */

    /**
     * @brief Don't call this method.
     * @details The engine calls this method before each clock cycle to swap the
     * buffers and do other pre-clock setup jobs.
     */
    void PreClock();
    /**
     * @brief Don't call this method.
     * @details The engine calls this method after each clock cycle to swap the
     * buffers and do other pos-clock setup jobs.
     */
    void PosClock();
    /**
     * @brief This method should be declared here so the simulator can send the
     * finish setup message.
     * @details This method is called after the config file is and all
     * parameters are set, so to finish any setup required by the component.
     * Non-zero should be returned if any problem occurred (e.g., a required
     * configuration parameter was not provided). The component is responsible
     * for printing a proper error message describing what happened.
     * @returns Non-zero on error, 0 otherwise.
     */
    virtual int FinishSetup() = 0;
    /**
     * @brief This method should be declared here so the simulator can send
     * config parameters.
     * @details This method is called if the config file defines a configuration
     * parameter for the component. Again, non-zero should be returned on error
     * and the component is responsible for printing a proper error message.
     * @param parameter The name of the parameter provided.
     * @param value The value of the parameter provided.
     * @returns Non-zero on error, 0 otherwise.
     */
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value) = 0;

    /**
     * @brief This method should be declared here so the simulator can send
     * clock cycles.
     */
    virtual void Clock() = 0;

    virtual ~Linkable();
};

}  // namespace engine
}  // namespace sinuca

#endif  // SINUCA3_ENGINE_LINKABLE_HPP_
