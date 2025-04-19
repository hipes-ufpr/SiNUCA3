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

    this->requestBuffers[0] = new CircularBuffer;
    this->responseBuffers[0] = new CircularBuffer;

    this->requestBuffers[1] = new CircularBuffer;
    this->responseBuffers[1] = new CircularBuffer;

    this->requestBuffers[0]->Allocate(bufferSize, messageSize);
    this->requestBuffers[1]->Allocate(bufferSize, messageSize);

    this->responseBuffers[0]->Allocate(bufferSize, messageSize);
    this->responseBuffers[1]->Allocate(bufferSize, messageSize);
};

void sinuca::engine::Connection::DeleteBuffers() {
    delete this->requestBuffers[0];
    delete this->requestBuffers[1];
    delete this->responseBuffers[0];
    delete this->responseBuffers[1];
};

inline int sinuca::engine::Connection::GetBufferSize() const {
    return this->bufferSize;
};

inline int sinuca::engine::Connection::GetMessageSize() const {
    return this->messageSize;
};

void sinuca::engine::Connection::SwapBuffers() {
    CircularBuffer* aux;

    aux = this->requestBuffers[0];
    this->requestBuffers[0] = this->requestBuffers[1];
    this->requestBuffers[1] = aux;

    aux = this->responseBuffers[0];
    this->responseBuffers[0] = this->responseBuffers[1];
    this->responseBuffers[1] = aux;
};

void sinuca::engine::Connection::FlushConnection() {
    this->requestBuffers[0]->Flush();
    this->requestBuffers[1]->Flush();
    this->responseBuffers[0]->Flush();
    this->responseBuffers[1]->Flush();
};

bool sinuca::engine::Connection::InsertIntoRequestBuffer(int id,
                                                         void* messageInput) {
    return this->requestBuffers[id]->Enqueue(messageInput);
};

bool sinuca::engine::Connection::InsertIntoResponseBuffer(int id,
                                                          void* messageInput) {
    return this->responseBuffers[id]->Enqueue(messageInput);
};

bool sinuca::engine::Connection::RemoveFromARequestBuffer(int id,
                                                          void* messageOutput) {
    return this->requestBuffers[id]->Dequeue(messageOutput);
};

bool sinuca::engine::Connection::RemoveFromAResponseBuffer(
    int id, void* messageOutput) {
    return this->responseBuffers[id]->Dequeue(messageOutput);
};

sinuca::engine::Linkable::Linkable(int messageSize)
    : messageSize(messageSize), numberOfConnections(0){};

void sinuca::engine::Linkable::AllocateConnectionsBuffer(
    long numberOfConnections) {
    this->numberOfConnections = numberOfConnections;
    this->connections.reserve(numberOfConnections);
};

void sinuca::engine::Linkable::DeallocateConnectionsBuffer() {
    for (unsigned int i = 0; i < this->connections.size(); ++i) {
        this->connections[i]->DeleteBuffers();
        delete connections[i];
    }
    this->connections.clear();
    this->numberOfConnections = 0;
};

void sinuca::engine::Linkable::AddConnection(Connection* newConnection) {
    int connectionsSize = this->connections.size();
    this->connections.push_back(newConnection);

    if (connectionsSize > this->numberOfConnections)
        this->numberOfConnections = connectionsSize;
};

std::vector<sinuca::engine::Connection*>
sinuca::engine::Linkable::GetConnections() const {
    return this->connections;
};

void sinuca::engine::Linkable::LinkableFlush() {
    for (unsigned int i = 0; i < this->connections.size(); ++i)
        this->connections[i]->FlushConnection();
}

void sinuca::engine::Linkable::PosClock() {
    for (unsigned int i = 0; i < this->connections.size(); ++i)
        this->connections[i]->SwapBuffers();
};

int sinuca::engine::Linkable::ConnectUnsafe(int bufferSize) {
    int index = this->connections.size();

    Connection* newConnection = new Connection();
    newConnection->CreateBuffers(bufferSize, this->messageSize);
    this->AddConnection(newConnection);

    return index;
};

int sinuca::engine::Linkable::SendRequestUnsafe(int connectionID,
                                                void* messageInput) {
    return this->connections[connectionID]->InsertIntoRequestBuffer(
        SOURCE_ID, messageInput);
};

int sinuca::engine::Linkable::GetRequestUnsafe(int connectionID,
                                               void* messageOutput) {
    return this->connections[connectionID]->RemoveFromARequestBuffer(
        DEST_ID, messageOutput);
};

int sinuca::engine::Linkable::SendResponseUnsafe(int connectionID,
                                                 void* messageInput) {
    return this->connections[connectionID]->InsertIntoResponseBuffer(
        DEST_ID, messageInput);
};

int sinuca::engine::Linkable::GetResponseUnsafe(int connectionID,
                                                void* messageOutput) {
    return this->connections[connectionID]->RemoveFromAResponseBuffer(
        SOURCE_ID, messageOutput);
};

sinuca::engine::Linkable::~Linkable() { DeallocateConnectionsBuffer(); };
