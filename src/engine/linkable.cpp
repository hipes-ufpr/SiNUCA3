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

void Connection::CreateBuffers(int bufferSize, int messageSize) {
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
}

void Connection::DeleteBuffers() {
    delete this->requestBuffers[0];
    delete this->requestBuffers[1];
    delete this->responseBuffers[0];
    delete this->responseBuffers[1];
}

inline int Connection::GetBufferSize() const { return this->bufferSize; }

inline int Connection::GetMessageSize() const { return this->messageSize; }

void Connection::CommitBuffers() {
    CircularBuffer* aux;

    aux = this->requestBuffers[0];
    this->requestBuffers[0] = this->requestBuffers[1];
    this->requestBuffers[1] = aux;

    aux = this->responseBuffers[0];
    this->responseBuffers[0] = this->responseBuffers[1];
    this->responseBuffers[1] = aux;

    this->requestBuffers[SOURCE_ID]->Flush();
    this->responseBuffers[DEST_ID]->Flush();
}

bool Connection::InsertIntoRequestBuffer(int id, void* messageInput) {
    return this->requestBuffers[id]->Enqueue(messageInput);
}

bool Connection::InsertIntoResponseBuffer(int id, void* messageInput) {
    return this->responseBuffers[id]->Enqueue(messageInput);
}

bool Connection::RemoveFromARequestBuffer(int id, void* messageOutput) {
    return this->requestBuffers[id]->Dequeue(messageOutput);
}

bool Connection::RemoveFromAResponseBuffer(int id, void* messageOutput) {
    return this->responseBuffers[id]->Dequeue(messageOutput);
}

Linkable::Linkable(int messageSize)
    : messageSize(messageSize), numberOfConnections(0) {}

void Linkable::AllocateConnectionsBuffer(long numberOfConnections) {
    this->numberOfConnections = numberOfConnections;
    this->connections.reserve(numberOfConnections);
}

void Linkable::DeallocateConnectionsBuffer() {
    for (unsigned int i = 0; i < this->connections.size(); ++i) {
        this->connections[i]->DeleteBuffers();
        delete connections[i];
    }
    this->connections.clear();
    this->numberOfConnections = 0;
}

void Linkable::AddConnection(Connection* newConnection) {
    this->connections.push_back(newConnection);
    this->numberOfConnections += 1;
}

long Linkable::GetNumberOfConnections() { return this->numberOfConnections; }

void Linkable::PosClock() {
    for (unsigned int i = 0; i < this->connections.size(); ++i){
        this->connections[i]->CommitBuffers();
    }
}

int Linkable::ConnectUnsafe(int bufferSize) {
    int index = this->connections.size();

    Connection* newConnection = new Connection();
    newConnection->CreateBuffers(bufferSize, this->messageSize);
    this->AddConnection(newConnection);

    return index;
}

int Linkable::SendRequestUnsafe(int connectionID, void* messageInput) {
    return this->connections[connectionID]->InsertIntoRequestBuffer(
        SOURCE_ID, messageInput);
}

int Linkable::GetRequestUnsafe(int connectionID, void* messageOutput) {
    return this->connections[connectionID]->RemoveFromARequestBuffer(
        DEST_ID, messageOutput);
}

int Linkable::SendResponseUnsafe(int connectionID, void* messageInput) {
    return this->connections[connectionID]->InsertIntoResponseBuffer(
        DEST_ID, messageInput);
}

int Linkable::GetResponseUnsafe(int connectionID, void* messageOutput) {
    return this->connections[connectionID]->RemoveFromAResponseBuffer(
        SOURCE_ID, messageOutput);
}

Linkable::~Linkable() { DeallocateConnectionsBuffer(); }
