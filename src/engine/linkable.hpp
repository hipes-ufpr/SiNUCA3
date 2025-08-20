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

#include <config/config.hpp>
#include <utils/circular_buffer.hpp>

static const int SOURCE_ID = 0;
static const int DEST_ID = 1;

struct Connection {
  private:
    int bufferSize;
    int messageSize;
    CircularBuffer* requestBuffers[2]; /**<Array of the request buffers, swapped
                                          each cycle.*/
    CircularBuffer* responseBuffers[2]; /**<Array of the response buffers,
                                           swapped each cycle.*/

  public:
    Connection() : bufferSize(0), messageSize(0) {};

    /**
     * @brief Allocate the buffers used to channels
     * @param bufferSize self-explanatory.
     * @param messageSize self-explanatory.
     */
    void CreateBuffers(int bufferSize, int messageSize);

    /**
     * @brief Free the memory allocated for the buffers.
     */
    void DeleteBuffers();

    /**
     * @brief Self-explanatory
     */
    inline int GetBufferSize() const;

    /**
     * @brief Self-explanatory
     */
    inline int GetMessageSize() const;

    /**
     * @brief Swap the buffers of the connection.
     */
    void SwapBuffers();

    /**
     * @brief Insert a message into a requestBuffer.
     * @param id The id of the buffer.
     * @param messageInput A pointer to the message to send.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool InsertIntoRequestBuffer(int id, void* messageInput);

    /**
     * @brief Send a message into a responseBuffer.
     * @param id The id of the buffer.
     * @param messageInput A pointer to the message to send.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool InsertIntoResponseBuffer(int id, void* messageInput);

    /**
     * @brief Remove a request from a requestBuffer.
     * @param id The id of the buffer.
     * @param messageOutput Address where to send the message.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool RemoveFromARequestBuffer(int id, void* messageOutput);

    /**
     * @brief Remove a response from a responseBuffer.
     * @param id The id of the  buffer.
     * @param messageOutput Address where to send the message.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool RemoveFromAResponseBuffer(int id, void* messageOutput);
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
    int ConnectUnsafe(int bufferSize);

    /**
     * @brief Receive a request from other component. (The other calls this
     * method)
     * @details This method is called to send a request message to *this*
     * linkable. Referencing the method name, this linkable *receives* a request
     * in its own connection request buffer.
     * @param connectionID The id of the connection.
     * @param messageInput A pointer to the message to send.
     * @return 0 if successfuly, 1 otherwise.
     */
    int SendRequestUnsafe(int connectionID, void* messageInput);

    /**
     * @brief Gets a request message in the parameter.
     * @details This method is called to get a request message from a request
     * buffer, so to receive the message, *this* linkable calls this method, and
     * the message sent is inserted into the memory region pointed to by the
     * messageOutput parameter.
     * @param connectionID The id of the connection.
     * @param messageOutput A pointer to the message to receive.
     * @return 0 if successfuly, 1 otherwise.
     */
    int GetRequestUnsafe(int connectionID, void* messageOutput);

    /**
     * @brief Sends a reply to the connection.
     * @details This method is called to insert a reply message into the
     * connection's reply buffer. Therefore, the linkable only inserts the
     * response message into the buffer.
     * @param connectionID The id of the connection.
     * @param messageInput A pointer to the message to send.
     * @return 0 if successfuly, 1 otherwise.
     */
    int SendResponseUnsafe(int connectionID, void* messageInput);

    /**
     * @brief Gets a response message in the parameter. (The other calls this
     * method)
     * @details This method is called to get a response message from a response
     * buffer, so to receive the message, the component calls this method, and
     * the message sent is inserted into the memory region pointed to by the
     * messageOutput parameter.
     * @param connectionID The id of the connection.
     * @param messageOutput A pointer to the message to receive.
     * @return 0 if successfuly, 1 otherwise.
     */
    int GetResponseUnsafe(int connectionID, void* messageOutput);

  public:
    Linkable(int messageSize);

    /**
     * @brief Self-explanatory
     */
    long GetNumberOfConnections();

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
                                   ConfigValue value) = 0;

    /**
     * @brief This method should be declared here so the simulator can send
     * clock cycles.
     */
    virtual void Clock() = 0;

    /**
     * @brief This method is called by the engine after the simulation stops, so
     * each component can print it's statistics.
     */
    virtual void PrintStatistics() = 0;

    virtual ~Linkable();
};

#endif  // SINUCA3_ENGINE_LINKABLE_HPP_
