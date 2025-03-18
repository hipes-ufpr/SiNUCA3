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
    /**
     * @param messageSize The size of the message that will be used by the
     * component.
     */
    inline Component() : engine::Linkable(sizeof(MessageType)) {}

    int SendRequestTo(Linkable* component, int connectionID, void* message) {
        return component->ReceiveRequest(connectionID, message);
    };

    int ReceiveResponseFrom(Linkable* component, int connectionID,
                            void* message) {
        return component->GetResponse(connectionID, message);
    };

    int SendResponseToConnection(int connectionID, void* message) {
        return this->SendResponse(connectionID, message);
    };

    int ReceiveRequestFromConnection(int connectionID, void* message) {
        return this->GetRequest(connectionID, message);
    };

    inline ~Component() {}
};

}  // namespace sinuca

#endif  // SINUCA3_ENGINE_COMPONENT_HPP_
