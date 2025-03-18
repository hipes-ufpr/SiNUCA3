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

void sinuca::engine::Connection::swapBuffers() {
    CircularBuffer* aux;

    aux = &(this->requestBuffers[0]);
    this->requestBuffers[0] = this->requestBuffers[1];
    this->requestBuffers[1] = *aux;

    aux = &(this->responseBuffers[0]);
    this->responseBuffers[0] = this->responseBuffers[1];
    this->responseBuffers[1] = *aux;
};

bool sinuca::engine::Connection::InsertIntoRequestBuffer(int id,
                                                         void* messageInput) {
    return this->requestBuffers[id].Enqueue(messageInput);
};

bool sinuca::engine::Connection::InsertIntoResponseBuffer(int id,
                                                          void* messageInput) {
    return this->responseBuffers[id].Enqueue(messageInput);
};

bool sinuca::engine::Connection::RemoveFromARequestBuffer(int id,
                                                          void* messageOutput) {
    return this->requestBuffers[id].Dequeue(messageOutput);
};

bool sinuca::engine::Connection::RemoveFromAResponseBuffer(
    int id, void* messageOutput) {
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

void sinuca::engine::Linkable::PreClock() {};

void sinuca::engine::Linkable::PosClock() {};

int sinuca::engine::Linkable::ReceiveRequest(int connectionID,
                                             void* messageInput) {
    return this->connections[connectionID]->InsertIntoRequestBuffer(
        SOURCE_ID, messageInput);
};

int sinuca::engine::Linkable::GetRequest(int connectionID,
                                         void* messageOutput) {
    return this->connections[connectionID]->RemoveFromARequestBuffer(
        DEST_ID, messageOutput);
};

int sinuca::engine::Linkable::SendResponse(int connectionID,
                                           void* messageInput) {
    return this->connections[connectionID]->InsertIntoResponseBuffer(
        DEST_ID, messageInput);
};

int sinuca::engine::Linkable::GetResponse(int connectionID,
                                          void* messageOutput) {
    return this->connections[connectionID]->RemoveFromAResponseBuffer(
        SOURCE_ID, messageOutput);
};

sinuca::engine::Linkable::~Linkable(){};
