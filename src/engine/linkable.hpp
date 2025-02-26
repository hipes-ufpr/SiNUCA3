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
#include "../utils/circularBuffer.hpp"

#define SOURCE_ID 0
#define DEST_ID 1

namespace sinuca {
namespace engine {

struct Connection {
  private:
    int bufferSize;
    int messageSize;
    CircularBuffer requestBuffers[2];  /**<Array of the request buffers, swapped
                                           each cycle.*/
    CircularBuffer responseBuffers[2]; /**<Array of the response buffers,
                                           swapped each cycle.*/

  public:
    Connection() : bufferSize(0), messageSize(0){};

    /**
     * @brief Allocate the buffers used to channels
     * @param bufferSize self-explanatory.
     * @param messageSize self-explanatory.
     */
    void CreateBuffers(int bufferSize, int messageSize);

    /**
     * @brief Self-explanatory
     */
    inline int GetBufferSize() const;

    /**
     * @brief Self-explanatory
     */
    inline int GetMessageSize() const;

    /**
     * @brief Send a request to a certain requestBuffer.
     * @param id The id of the certain buffer.
     * @param message A pointer to the message to send.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to send a message to the recipient it uses DEST_ID
     * as a parameter. Otherwise, the recipient is sending a message to the
     * source Linkable, so it uses SOURCE_ID as a parameter.
     */
    void SendRequest(char id, void* message);

    /**
     * @brief Return a response of a certain responseBuffer.
     * @param id The id of the certain buffer.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to receive a message from the recipient it uses
     * SOURCE_ID as a parameter. Otherwise, the recipient is receiving a message
     * from the source Linkable, so it uses DEST_ID as a parameter.
     * @return A pointer to the received message
     */
    void* RecieveResponse(char id);
};

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
    long messageSize;
    long numberOfConnections; /**< Counts how much connections other components
                                  have initialized. */

    std::vector<Connection*>
        connections; /**< Array of all connections buffers.*/

  protected:
    /**
     * @brief Allocates the buffers with the specified number of connections.
     * @param numberOfConnections Self-explanatory.
     */
    void AllocateBuffers(long messageSize, long numberOfConnections);

    /**
     * @brief Frees memory allocated for connections 
     */
    void DeallocateBuffers ();

    /**
     * @brief Add the new connection in the connections array.
     * @param newConnection self-explanatory.
     */
    void AddConnection(Connection* newConnection);

    /**
     * @brief Connect to *this* component.
     * @param bufferSize The size of the buffer used in the connection.
     * @param messageSize The size of the message stored in the buffer.
     * @details Method used by other components to connect to *this* component,
     * establishing a connection where *this* component is the one that responds
     * to received messages.
     * @return Returns the connection index of the receiving component
     */
    int Connect(int bufferSize);

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

    Linkable();
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
