#ifndef SINUCA3_ENGINE_COMPONENT_HPP_
#define SINUCA3_ENGINE_COMPONENT_HPP_

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
 * @file component.hpp
 * @brief Public API of the component template class.
 */

#include "../config/config.hpp"
#include "linkable.hpp"

namespace sinuca {

/**
 * @details All components shall inherit from this class. The MessageType type
 * parameter defines the message type the component receives from other
 * components. If the component does not receive any message, int can be used as
 * a placeholder. Note that Component<T> is just a wrapper for the underlying
 * mother class Linkable. This is done to centralize the message-passing
 * implementation in a non-template class for optimization reasons. This wrapper
 * allows a nice and type-safe API on top of a single, fast and generic
 * implementation.
 *
 * Avoiding big types in MessageType is a good idea, because they're passed by
 * value.
 */
template <typename MessageType>
class Component : public engine::Linkable {
  protected:
    /**
     * @brief Sends a response to another component.
     * @param component The component to send the response to.
     * @param connectionID The connection ID.
     * @param message The message to send.
     * @return 1 if successful, 0 otherwise.
     */
    int SendResponseToConnection(int connectionID, MessageType* message) {
        return this->SendResponseUnsafe(connectionID, message);
    };

    /**
     * @brief Receives a request from another component.
     * @param connectionID The connection ID.
     * @param message The message to receive.
     * @return 1 if successful, 0 otherwise.
     */
    int ReceiveRequestFromConnection(int connectionID, MessageType* message) {
        return this->GetRequestUnsafe(connectionID, message);
    };

  public:
    /**
     * @param messageSize The size of the message that will be used by the
     * component.
     */
    inline Component() : engine::Linkable(sizeof(MessageType)) {}

    /**
     * @brief Connects to another component.
     * @param component The component to connect to.
     * @param bufferSize The size of the buffer to be used.
     * @return The connection ID.
     */
    int Connect(int bufferSize) { return this->ConnectUnsafe(bufferSize); };

    /**
     * @brief Sends a request to another component.
     * @param component The component to send the request to.
     * @param connectionID The connection ID.
     * @param message The message to send.
     * @return 1 if successful, 0 otherwise.
     */
    int SendRequest(int connectionID, MessageType* message) {
        return this->SendRequestUnsafe(connectionID, message);
    };

    /**
     * @brief Receives a response from another component.
     * @param component The component to receive the response from.
     * @param connectionID The connection ID.
     * @param message The message to receive.
     * @return 1 if successful, 0 otherwise.
     */
    int ReceiveResponse(int connectionID, MessageType* message) {
        return this->GetResponseUnsafe(connectionID, message);
    };

    inline ~Component() {}
};

}  // namespace sinuca

#endif  // SINUCA3_ENGINE_COMPONENT_HPP_
