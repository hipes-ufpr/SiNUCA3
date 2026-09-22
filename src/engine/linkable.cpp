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

#include <utils/logger.hpp>

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

    this->swapBuffer = new char[messageSize];
}

void Connection::DeleteBuffers() {
    delete this->requestBuffers[0];
    delete this->requestBuffers[1];
    delete this->responseBuffers[0];
    delete this->responseBuffers[1];
    if (this->swapBuffer) delete[] this->swapBuffer;
}

inline int Connection::GetBufferSize() const { return this->bufferSize; }

inline int Connection::GetMessageSize() const { return this->messageSize; }

inline bool Connection::IsRequestBufferAvailable(int id) const {
    return !(this->requestBuffers[id]->IsFull());
};

void Connection::SwapBuffers() {
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

void Connection::PushBuffers() {
    char* message = this->swapBuffer;

    while (!this->requestBuffers[SOURCE_ID]->IsEmpty()) {
        SINUCA3_DEBUG_PRINTF("Req Size: %d\n",
                             this->requestBuffers[SOURCE_ID]->GetSize());
        this->requestBuffers[SOURCE_ID]->Dequeue(message);
        this->requestBuffers[DEST_ID]->Enqueue(message);
    }

    while (!this->responseBuffers[DEST_ID]->IsEmpty()) {
        SINUCA3_DEBUG_PRINTF("Res Size: %d\n",
                             this->responseBuffers[DEST_ID]->GetSize());
        this->responseBuffers[DEST_ID]->Dequeue(message);
        this->responseBuffers[SOURCE_ID]->Enqueue(message);
    }
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

bool Connection::IsRequestBufferFull(int id) {
    return this->requestBuffers[id]->IsFull();
}

bool Connection::IsResponseBufferFull(int id) {
    return this->responseBuffers[id]->IsFull(); 
}

Linkable::Linkable(int messageSize)
    : messageSize(messageSize), numberOfConnections(0), context(0) {}

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
    for (unsigned int i = 0; i < this->connections.size(); ++i)
        this->connections[i]->PushBuffers();
}

bool Linkable::IsConnectionAvailable(int connectionID) {
    return this->connections[connectionID]->IsRequestBufferAvailable(SOURCE_ID);
}

int Linkable::ConnectUnsafe(int bufferSize) {
    int index = this->connections.size();

    Connection* newConnection = new Connection();
    newConnection->CreateBuffers(bufferSize, this->messageSize);
    this->AddConnection(newConnection);

    return index;
}

int Linkable::SendRequestUnsafe(int connectionID, void* messageInput) {

    if (this->connections[connectionID]->IsResponseBufferFull(SOURCE_ID)
        && this->connections[connectionID]->IsRequestBufferFull(SOURCE_ID)) {
        return 1; 
    }
    return this->connections[connectionID]->InsertIntoRequestBuffer(
        SOURCE_ID, messageInput);
}

int Linkable::GetRequestUnsafe(int connectionID, void* messageOutput) {
    return this->connections[connectionID]->RemoveFromARequestBuffer(
        DEST_ID, messageOutput);
}

int Linkable::SendResponseUnsafe(int connectionID, void* messageInput) {

    if (this->connections[connectionID]->IsResponseBufferFull(DEST_ID)) {
        return 1;
    }

    return this->connections[connectionID]->InsertIntoResponseBuffer(
        DEST_ID, messageInput);
}

int Linkable::GetResponseUnsafe(int connectionID, void* messageOutput) {
    return this->connections[connectionID]->RemoveFromAResponseBuffer(
        SOURCE_ID, messageOutput);
}

void Linkable::AddChild(Linkable* child) {
    if (child == NULL) {
        SINUCA3_ERROR_PRINTF("Cannot add a null child component.\n");
        return;
    }
    this->children.push_back(child);
}

int Linkable::SetContext(const Context* context) {
    if (context == NULL) {
        SINUCA3_ERROR_PRINTF("Cannot set a null context for a component.\n");
        return 1;
    }
    this->context = context;
    return 0;
}

const Context* Linkable::GetContext() const {
    if (this->context == NULL) {
        SINUCA3_ERROR_PRINTF("Component does not have a context.\n");
        return NULL;
    }
    return this->context;
}

long Linkable::GetNumberOfChildren() const { return this->children.size(); }

Linkable* Linkable::GetReferenceToChild(long index) const {
    if (index < 0 || index >= (long)this->children.size()) {
        SINUCA3_ERROR_PRINTF("Child index out of bounds: %ld.\n", index);
        return NULL;
    }
    return this->children[index];
}

Linkable::~Linkable() { DeallocateConnectionsBuffer(); }

Context* Context::CreateContext() {
    Context* ctx = new Context();
    if (ctx == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create context.\n");
        return NULL;
    }
    ctx->core.contextId = -1;
    ctx->core.engineConnId = -1;
    ctx->core.engine = NULL;
    contexts.push_back(ctx);
    return ctx;
}

void Context::DestroyAllContexts() {
    for (long i = 0; i < (long)contexts.size(); i++)
        if (contexts[i]) delete contexts[i];
    contexts.clear();
}

void Context::SetCoreContext(int contextId, int engineConnId, Linkable* engine) {
    this->core.contextId = contextId;
    this->core.engineConnId = engineConnId;
    this->core.engine = engine;
}

int Context::PropagateContext(Linkable* component) {
    if (component->SetContext(this) != 0) {
        SINUCA3_ERROR_PRINTF("Failed to propagate context to component.\n");
        return 1;
    }
    for (long i = 0; i < component->GetNumberOfChildren(); i++)
        PropagateContext(component->GetReferenceToChild(i));
    return 0;
}

// C++ requires the definition of static members outside the class declaration.s
std::vector<Context*> Context::contexts;
