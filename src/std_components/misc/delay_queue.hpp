#ifndef SINUCA3_DELAY_QUEUE_HPP
#define SINUCA3_DELAY_QUEUE_HPP

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
 * @file delay_queue.hpp
 * @brief Queue designed to simulate delay
 * @details The delay queue component is a queue designed to simulate delay
 * existing in real life components. Therefore, it has a `delay` parameter
 * that corresponds to the waiting time in the queue. Beware that any component
 * has a natural delay of two cycles: 
 * 
 * clock #1: component 1 sends message to component 2
 * clock #2: component 2 receives message and forwards it to component 3
 * clock #3: component 3 receives message
 *
 * The delay queue allows messages from multiple components to be received, but
 * any request that exceeds the total capacity of the queue mau be  discarded,
 * so the `throughput` parameter must be set carefully.
 */

#include <engine/component.hpp>
#include <sinuca3.hpp>
#include <utils/circular_buffer.hpp>

#include "config/config.hpp"
#include "utils/logging.hpp"

template <typename Type>
class DelayQueue : public Component<Type> {
  private:
    struct Input {
        Type elem;
        unsigned long removeAt; /**<Cycles until removal> */
        void SetRemoval(unsigned long l) { this->removeAt = l; }
    };

    Input queueFirst;           /**<Oldest input in the delay buffer> */
    Input elemToInsert;         /**<Input to be inserted in delay buffer> */
    Input elemToRemove;         /**<Input removed from delay buffer> */
    CircularBuffer delayBuffer; /**<Used if delay >= 1 */
    Component<Type>* sendTo;
    unsigned long cyclesClock; /**<A cycles clock> */
    unsigned long occupation;
    unsigned long delayBufferSize;
    long throughput;
    long delay; /**<Number of cycles of delay> */
    int sendToId;

    inline bool IsEmpty() { return !this->occupation; }
    inline bool IsFull() { return this->delayBuffer.IsFull(); }
    void CalculateDelayBufferSize();
    /**
     * @brief No insertion if full. If empty, the queueFirst is set with new
     * elemToInsert. Otherwise, it is inserted in delay buffer.
     */
    int Enqueue();
    /**
     * @brief No removal if empty or not time to remove queueFirst. Else, 
     * elemToRemove receives queueFirst and if occupation is greater than one,
     * queueFirst receives the oldest input from delay buffer.
     */
    int Dequeue();

  public:
    DelayQueue();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics() {}
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~DelayQueue();
};

template <typename Type>
DelayQueue<Type>::DelayQueue()
    : sendTo(NULL), cyclesClock(0), occupation(0), delayBufferSize(0) {}

template <typename Type>
DelayQueue<Type>::~DelayQueue() {
    this->delayBuffer.Deallocate();
}

template <typename Type>
void DelayQueue<Type>::CalculateDelayBufferSize() {
    this->delayBufferSize = this->delay * this->throughput;
}

template <typename Type>
int DelayQueue<Type>::Enqueue() {
    if (this->IsFull()) {
        return 1;
    }
    if (this->IsEmpty()) {
        this->queueFirst = this->elemToInsert;
    } else {
        this->delayBuffer.Enqueue(&this->elemToInsert);
    }
    this->occupation++;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::Dequeue() {
    if (this->IsEmpty()) {
        return 1;
    }
    if (this->queueFirst.removeAt > this->cyclesClock) {
        return 1;  // Not yet time to remove
    }
    this->elemToRemove = this->queueFirst;
    if (this->occupation > 1) {
        this->delayBuffer.Dequeue(&this->queueFirst);
    }
    this->occupation--;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::SetConfigParameter(const char* param, ConfigValue val) {
    if (!strcmp(param, "delay")) {
        if (val.type != ConfigValueTypeInteger) {
            return 1;
        }
        long delay = val.value.integer;
        if (delay < 0) {
            return 1;
        }
        this->delay = delay;
        return 0;
    }
    if (!strcmp(param, "throughput")) {
        if (val.type != ConfigValueTypeInteger) {
            return 1;
        }
        long throughput = val.value.integer;
        if (throughput <= 0) {
            return 1;
        }
        this->throughput = throughput;
        return 0;
    }
    if (!strcmp(param, "sendTo")) {
        if (val.type != ConfigValueTypeComponentReference) {
            return 1;
        }
        Component<Type>* comp =
            dynamic_cast<Component<Type>*>(val.value.componentReference);
        if (comp == NULL) {
            return 1;
        }
        this->sendTo = comp;
        return 0;
    }
    return 1;
}

template <typename Type>
int DelayQueue<Type>::FinishSetup() {
    if (this->sendTo == NULL) {
        return 1;
    }
    this->sendToId = this->sendTo->Connect(this->throughput);
    this->CalculateDelayBufferSize();
    if (this->delayBufferSize > 0) {
        this->delayBuffer.Allocate(this->delayBufferSize, sizeof(Input));
    }
    return 0;
}

template <typename Type>
void DelayQueue<Type>::Clock() {
    long totalConnections = this->GetNumberOfConnections();

    this->cyclesClock++;
    if (this->cyclesClock + (unsigned long)this->delay == (unsigned long)~0) {
        SINUCA3_ERROR_PRINTF(
            "Congratulations! You've achieved something deemed impossible "
            "[%lu] cycles\n",
            this->cyclesClock);
    }

    if (this->delay == 0) {
        Type elem;
        for (long i = 0; i < totalConnections; i++) {
            while (!this->ReceiveRequestFromConnection(i, &elem)) {
                if (this->sendTo->SendRequest(sendToId, &elem)) return;
            }
        }
        return;
    }

    while (!this->Dequeue()) {
        this->sendTo->SendRequest(sendToId, &this->elemToRemove.elem);
    }

    this->elemToInsert.SetRemoval(this->cyclesClock + this->delay);
    for (long i = 0; i < totalConnections; i++) {
        while (
            !this->ReceiveRequestFromConnection(i, &this->elemToInsert.elem)) {
            if (this->Enqueue()) return;
        }
    }
}

#ifndef NDEBUG
int TestDelayQueue();
#endif

#endif  // SINUCA3_DELAY_QUEUE