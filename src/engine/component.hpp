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
  public:
    /* Other component methods. */

    /**
     * @brief This method should be called by other components to send a message
     * to the component.
     * @param message The message to send.
     * @param channelID The ID of the channel on which to send the message.
     * @returns Non-zero if the message cannot be sent (i.e., there's no space
     * left for buffering, case where the sender may want to buffer itself the
     * message for trying again in the next cycles or just give up trying to
     * send). 0 otherwise.
     */
    inline int SendMessage(const MessageType message, int channelID) {
        return this->SendMessageLinkable((const char*)&message, channelID);
    }
    /**
     * @brief This method should be called by other components to read the
     * response buffered.
     * @param message The buffer on which to copy the response message.
     * @param channelID The ID of the channel from which to retrieve a response.
     * @returns Non-zero if there's no response buffered yet. In this case,
     * message is not touched. 0 otherwise, and message is populated with the
     * response.
     */
    inline int RetrieveResponse(MessageType* message, int channelID) {
        return this->RetrieveResponseLinkable((const char*)&message, channelID);
    }

    /**
     * @param numberOfBuffers 0 to allow an infinite amount of messages to be
     * buffered (i.e., the simulation ignores the fact that messages have to be
     * buffered at all). This is per connection.
     */
    inline Component(long numberOfBuffers = 0)
        : engine::Linkable(sizeof(MessageType), numberOfBuffers) {}

    inline ~Component() {}
};

}  // namespace sinuca

#endif  // SINUCA3_ENGINE_COMPONENT_HPP_
