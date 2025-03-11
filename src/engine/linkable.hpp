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

static const int SOURCE_ID = 0;
static const int DEST_ID = 1;

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
     * @param messageInput A pointer to the message to send.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to send a message to the recipient it uses DEST_ID
     * as a parameter. Otherwise, the recipient is sending a message to the
     * source Linkable, so it uses SOURCE_ID as a parameter.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendRequest(int id, void* messageInput);

    /**
     * @brief Send a response to a certain responseBuffer.
     * @param id The id of the certain buffer.
     * @param messageInput A pointer to the message to send.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to send a response to the recipient it uses
     * DEST_ID as a parameter. Otherwise, the recipient is sending a message to
     * the source Linkable, so it uses SOURCE_ID as a parameter.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendResponse(int id, void* messageInput);

    /**
     * @brief Return a request of a certain requestBuffer.
     * @param id The id of the certain buffer.
     * @param messageOutput Address where to send the message.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to receive a request from the recipient it uses
     * SOURCE_ID as a parameter. Otherwise, the recipient is receiving a request
     * from the source Linkable, so it uses DEST_ID as a parameter.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveRequest(int id, void* messageOutput);

    /**
     * @brief Return a response of a certain responseBuffer.
     * @param id The id of the certain buffer.
     * @param messageOutput Address where to send the message.
     * @details The Linkable that connected to another through the Connect call
     * becomes the SOURCE, so to receive a message from the recipient it uses
     * SOURCE_ID as a parameter. Otherwise, the recipient is receiving a message
     * from the source Linkable, so it uses DEST_ID as a parameter.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveResponse(int id, void* messageOutput);
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
    void AllocateConnectionsBuffer(long numberOfConnections);

    /**
     * @brief Frees memory allocated for connections
     */
    void DeallocateConnectionsBuffer();

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
     * @return Returns the id of connection on the receiving component
     */
    int Connect(int bufferSize);

    /* Source Methods */

    /**
     * @brief This method is used by a Linkable Source to send a request to an
     * Linkable that it has a reference to.
     * @param dest The pointer to Linkable.
     * @param connectionID The connection ID obtained by the Connect method with
     * the desired Linkable.
     * @param messageInput The request message to be sent.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendRequestToLinkable(Linkable* dest, int connectionID,
                               void* messageInput);

    /**
     * @brief This method is used by a Linkable Source to send a response to an
     * Linkable that it has a reference to.
     * @param dest The pointer to Linkable.
     * @param connectionID The connection ID obtained by the Connect method with
     * the desired Linkable.
     * @param messageInput The response message to be sent.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendResponseToLinkable(Linkable* dest, int connectionID,
                                void* messageInput);

    /**
     * @brief This method is used by a Linkable Source to receive a request from
     * an Linkable that it has a reference to.
     * @param dest The pointer to Linkable.
     * @param connectionID The connection ID obtained by the Connect method with
     * the desired Linkable.
     * @param messageOutput Address where to send the message.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveRequestFromLinkable(Linkable* dest, int connectionID,
                                    void* messageOutput);

    /**
     * @brief This method is used by a Linkable Source to receive a response
     * from an Linkable that it has a reference to.
     * @param dest The pointer to Linkable.
     * @param connectionID The connection ID obtained by the Connect method with
     * the desired Linkable.
     * @param messageOutput Address where to send the message.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveResponseFromLinkable(Linkable* dest, int connectionID,
                                     void* messageOutput);

    /* Recipient Methods */

    /**
     * @brief This method is used by a recipient Linkable to send a request to a
     * connection.
     * @param connectionID The connection ID of the connection that Linkable
     * wants to send a request to.
     * @param messageInput The request message to be sent.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendRequestToConnection(int connectionID, void* messageInput);

    /**
     * @brief This method is used by a recipient Linkable to send a response to
     * a connection.
     * @param connectionID The connection ID of the connection that Linkable
     * wants to send a response to.
     * @param messageInput The response message to be sent.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool SendResponseToConnection(int connectionID, void* messageInput);

    /**
     * @brief This method is used by a recipient Linkable to receive a request
     * from a connection.
     * @param connectionID The connection ID of the connection that Linkable
     * wants to receive a request from.
     * @param messageOutput Address where to send the message.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveRequestFromConnection(int connectionID, void* messageOutput);

    /**
     * @brief This method is used by a recipient Linkable to receive a response
     * from a connection.
     * @param connectionID The connection ID of the connection that Linkable
     * wants to receive a response from.
     * @param messageOutput Address where to send the message.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool ReceiveResponseFromConnection(int connectionID, void* messageOutput);

  public:
    Linkable(int messageSize);
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
