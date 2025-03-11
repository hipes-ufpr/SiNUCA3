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
 * @file linkable.cpp
 * @brief Implementation of the Linkable class.
 */

#include "linkable.hpp"

void sinuca::engine::Connection::CreateBuffers(int bufferSize,
                                               int messageSize) {
    this->bufferSize = bufferSize;
    this->messageSize = messageSize;

    this->requestBuffers[0].Allocate(bufferSize, messageSize);
    this->requestBuffers[1].Allocate(bufferSize, messageSize);

    this->responseBuffers[0].Allocate(bufferSize, messageSize);
    this->responseBuffers[1].Allocate(bufferSize, messageSize);
};

inline int sinuca::engine::Connection::GetBufferSize() const {
    return this->bufferSize;
};

inline int sinuca::engine::Connection::GetMessageSize() const {
    return this->messageSize;
};

bool sinuca::engine::Connection::SendRequest(int id, void* messageInput) {
    return this->requestBuffers[id].Enqueue(messageInput);
};

bool sinuca::engine::Connection::SendResponse(int id, void* messageInput) {
    return this->responseBuffers[id].Enqueue(messageInput);
};

bool sinuca::engine::Connection::ReceiveRequest(int id, void* messageOutput) {
    return this->requestBuffers[id].Dequeue(messageOutput);
};

bool sinuca::engine::Connection::ReceiveResponse(int id, void* messageOutput) {
    return this->responseBuffers[id].Dequeue(messageOutput);
};

sinuca::engine::Linkable::Linkable(int messageSize)
    : messageSize(messageSize), numberOfConnections(0){};

void sinuca::engine::Linkable::AllocateConnectionsBuffer(
    long numberOfConnections) {
    this->numberOfConnections = numberOfConnections;
    this->connections.reserve(numberOfConnections);
};

void sinuca::engine::Linkable::DeallocateConnectionsBuffer() {
    for (int i = 0; i < numberOfConnections; ++i) {
        delete this->connections[i];
    }
};

void sinuca::engine::Linkable::AddConnection(Connection* newConnection) {
    int connectionsSize = this->connections.size();
    this->connections.push_back(newConnection);

    if (connectionsSize > this->numberOfConnections)
        this->numberOfConnections = connectionsSize;
};

int sinuca::engine::Linkable::Connect(int bufferSize) {
    int index = this->connections.size();

    Connection* newConnection = new Connection();
    newConnection->CreateBuffers(bufferSize, this->messageSize);
    this->AddConnection(newConnection);

    return index;
};

bool sinuca::engine::Linkable::SendRequestToLinkable(Linkable* dest,
                                                     int connectionID,
                                                     void* messageInput) {
    return dest->connections[connectionID]->SendRequest(DEST_ID, messageInput);
};

bool sinuca::engine::Linkable::SendResponseToLinkable(Linkable* dest,
                                                      int connectionID,
                                                      void* messageInput) {
    return dest->connections[connectionID]->SendResponse(DEST_ID, messageInput);
};

bool sinuca::engine::Linkable::ReceiveRequestFromLinkable(Linkable* dest,
                                                          int connectionID,
                                                          void* messageOutput) {
    return dest->connections[connectionID]->ReceiveRequest(SOURCE_ID,
                                                           messageOutput);
};

bool sinuca::engine::Linkable::ReceiveResponseFromLinkable(
    Linkable* dest, int connectionID, void* messageOutput) {
    return dest->connections[connectionID]->ReceiveResponse(SOURCE_ID,
                                                            messageOutput);
};

bool sinuca::engine::Linkable::SendRequestToConnection(int connectionID,
                                                       void* messageInput) {
    return this->connections[connectionID]->SendRequest(SOURCE_ID,
                                                        messageInput);
};

bool sinuca::engine::Linkable::SendResponseToConnection(int connectionID,
                                                        void* messageInput) {
    return this->connections[connectionID]->SendResponse(SOURCE_ID,
                                                         messageInput);
};

bool sinuca::engine::Linkable::ReceiveRequestFromConnection(
    int connectionID, void* messageOutput) {
    return this->connections[connectionID]->ReceiveRequest(DEST_ID,
                                                           messageOutput);
};

bool sinuca::engine::Linkable::ReceiveResponseFromConnection(
    int connectionID, void* messageOutput) {
    return this->connections[connectionID]->ReceiveResponse(DEST_ID,
                                                            messageOutput);
};

void sinuca::engine::Linkable::PreClock() {}
void sinuca::engine::Linkable::PosClock() {}

sinuca::engine::Linkable::~Linkable(){};
