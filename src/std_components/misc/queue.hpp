#ifndef SINUCA3_QUEUE_HPP_
#define SINUCA3_QUEUE_HPP_

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file queue.hpp
 * @details API of the template Queue, a generic fetching queue that can be used
 * to enqueue results from caches, predictors, etc. You need to create a proper
 * component that instantiates this template class to use it.
 */

/**
 * @brief Queue is a template class for creating fetching queues.
 * @details The queue uses the connection ones creates to it as buffer. Thus,
 * the size of the queue is actually just the buffer size of the connection one
 * makes to it.
 *
 * The queue must receive a parameter `sendTo` that points to a component of the
 * same type. This component receives all messages enqueued.
 *
 * The other parameter the queue may receive is `throughput`, which sets the
 * the buffer of the connection with `sendTo`. If you don't set this parameter
 * or set it to zero, the queue will forward all messages to `sendTo` immediatly
 * as they arrive. This way, the queue actually simply adds a cycle of latency
 * to the pipeline.
 */

#include <sinuca3.hpp>

template <typename Type>
class Queue : public Component<Type> {
    Component<Type>*
        sendTo;       /** @brief Component to which send the responses. */
    long throughput;  /** @brief Size of the connection to `sendTo`. */
    int connectionID; /** @brief Connection ID with `sendTo`. */

  public:
    inline Queue() : sendTo(NULL), throughput(0) {}
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~Queue();
};

template <typename Type>
int Queue<Type>::FinishSetup() {
    if (this->sendTo == NULL) {
        SINUCA3_ERROR_PRINTF("Queue didn't received a sendTo parameter.\n");
        return 1;
    }

    this->connectionID = this->sendTo->Connect(this->throughput);

    return 0;
}

template <typename Type>
int Queue<Type>::SetConfigParameter(const char* parameter, ConfigValue value) {
    if (strcmp(parameter, "sendTo") == 0) {
        if (value.type == ConfigValueTypeComponentReference) {
            Linkable* linkable = value.value.componentReference;
            Component<Type>* component =
                dynamic_cast<Component<Type>*>(linkable);
            if (component != NULL) {
                this->sendTo = component;
                return 0;
            }

            SINUCA3_ERROR_PRINTF(
                "Queue parameter is not a component of the queue type.\n");
        }

        SINUCA3_ERROR_PRINTF(
            "Queue parameter sendTo is not a component pointer.\n");
        return 1;
    } else if (strcmp(parameter, "throughput") == 0) {
        if (value.type == ConfigValueTypeInteger) {
            this->throughput = value.value.integer;
        }

        return 0;
    }

    SINUCA3_ERROR_PRINTF("Queue received unknown parameter %s.\n", parameter);

    return 1;
}

template <typename Type>
void Queue<Type>::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    Type packet;

    for (long i = 0; i < numberOfConnections; ++i) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            if (this->sendTo->SendRequest(this->connectionID, &packet) != 0) {
                return;
            }
        }
    }
}

template <typename Type>
void Queue<Type>::PrintStatistics() {}

template <typename Type>
Queue<Type>::~Queue() {}

#ifndef NDEBUG
/** @brief Component for testing the Queue template class. */
class QueueTester : public Component<long> {
  public:
    virtual int FinishSetup() { return 0; }
    virtual int SetConfigParameter(const char* parameter, ConfigValue value) {
        (void)parameter;
        (void)value;

        return 0;
    }
    virtual void Clock() {}
    virtual void PrintStatistics() {}
    virtual ~QueueTester() {}

    /** @brief Get's a message as a `long`, returning 0 if no message is
     * available. */
    long GetMessage() {
        long msg;
        if (!this->ReceiveRequestFromConnection(0, &msg)) {
            return msg;
        }
        return 0;
    }
};

int TestQueue();
#endif  // NDEBUG

#endif  // SINUCA3_QUEUE_HPP_
