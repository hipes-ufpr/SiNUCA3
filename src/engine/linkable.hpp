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

#include <string>

#include "../config/config.hpp"

namespace sinuca {
namespace engine {

class Linkable {
  private:
    long bufferSize;
    long numberOfBuffers;
    long numberOfConnections;
    /* No I'm not using vector for this. */
    char* buffers;
    char* requestBuffers[2];
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
     */
    void AllocateBuffers(long numberOfConnections);
    int SendMessageLinkable(const char* message, int channelID);
    int RetrieveResponseLinkable(const char* message, int channelID);

    ~Linkable();

  public:
    /* Usually engine methods. */
    void PreClock();
    void PosClock();
    virtual int SetConfigParameter(const std::string* parameter,
                                   sinuca::config::ConfigValue value) = 0;

    virtual void Clock() = 0;
};

}  // namespace engine
}  // namespace sinuca

#endif  // SINUCA3_ENGINE_LINKABLE_HPP_
