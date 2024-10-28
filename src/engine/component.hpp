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

#include <string>

#include "../config/config.hpp"
#include "linkable.hpp"

namespace sinuca {

template <typename MessageType>
class Component : public engine::Linkable {
  public:
    /* Other component methods. */
    inline int SendMessage(const MessageType message, int channelID) {
        return this->SendMessageLinkable((const char*)&message, channelID);
    }
    inline int RetrieveResponse(MessageType* message, int channelID) {
        return this->RetrieveResponseLinkable((const char*)&message, channelID);
    }

    inline Component(long numberOfBuffers = 0)
        : engine::Linkable(sizeof(MessageType), numberOfBuffers) {}

    inline ~Component() {}
};

}  // namespace sinuca

#endif  // SINUCA3_ENGINE_COMPONENT_HPP_
