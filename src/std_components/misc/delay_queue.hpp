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
 * any request that exceeds the total capacity of the queue will be discarded,
 * so the `throughput` parameter must be set carefully.
 *
 * This component uses a cycles counter to set and track the age of the elements
 * enqueued. If the trace being simulated runs for more cycles than 2^64 - 1 -
 * `delay`, the queue may not work properly.
 */

#include <cassert>
#include <engine/component.hpp>
#include <sinuca3.hpp>
#include <utils/circular_buffer.hpp>

template <typename Type>
class DelayQueue : public Component<Type> {
  private:
    struct Input {
        Type elem;
        unsigned long removeAt; /**<Cycle of removal> */
        void SetRemoval(unsigned long l) { this->removeAt = l; }
    };

    Input queueFirst;           /**<Oldest input in the delay buffer> */
    CircularBuffer delayBuffer; /**<Used if delay >= 1 */
    Component<Type>* sendTo;
    unsigned long cyclesClock; /**<A cycles clock> */
    unsigned long occupation;
    unsigned long delayBufferSize;
    unsigned long throughput;
    unsigned long delay; /**<Number of cycles of delay> */
    int sendToId;

    inline bool IsEmpty() { return !this->occupation; }
    inline bool IsFull() { return this->delayBuffer.IsFull(); }
    inline bool UseDelayBuffer() { return this->delay >= 1; }
    inline void SetDelayBufferSize() {
        this->delayBufferSize =
            this->delay * this->throughput + this->throughput - 1;
    }
    /**
     * @brief No insertion if full. If empty, the queueFirst is set with new
     * elemToInsert. Otherwise, it is inserted in delay buffer.
     */
    int Enqueue(Input* elem);
    /**
     * @brief No removal if empty or not time to remove queueFirst. Else,
     * elemToRemove receives queueFirst and if occupation is greater than one,
     * queueFirst receives the oldest input from delay buffer.
     */
    int Dequeue(Input* elem);

  public:
    DelayQueue();
    virtual int Configure(Config config);
    virtual void PrintStatistics() {}
    virtual void Clock();
    virtual ~DelayQueue();
};

template <typename Type>
DelayQueue<Type>::DelayQueue()
    : sendTo(NULL),
      cyclesClock(0),
      occupation(0),
      delayBufferSize(0),
      throughput(0) {}

template <typename Type>
DelayQueue<Type>::~DelayQueue() {
    this->delayBuffer.Deallocate();
}

template <typename Type>
int DelayQueue<Type>::Enqueue(Input* elem) {
    if (this->IsFull()) {
        return 1;
    }
    elem->SetRemoval(this->cyclesClock + this->delay);
    if (this->IsEmpty()) {
        this->queueFirst = *elem;
    } else {
        this->delayBuffer.Enqueue(elem);
    }
    this->occupation++;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::Dequeue(Input* elem) {
    if (this->IsEmpty()) {
        return 1;
    }
    if (this->queueFirst.removeAt > this->cyclesClock) {
        return 1;  // Not yet time to remove
    }
    *elem = this->queueFirst;
    if (this->occupation > 1) {
        this->delayBuffer.Dequeue(&this->queueFirst);
    }
    this->occupation--;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::Configure(Config config) {
    long delay = 0;
    if (config.Integer("delay", &delay)) return 1;
    if (delay < 0) return config.Error("delay", "is not >= 0.");
    this->delay = delay;

    long throughput = 0;
    if (config.Integer("throughput", &throughput, true)) return 1;
    if (throughput < 0) return config.Error("throughput", "is not >= 0.");
    this->throughput = throughput;

    if (config.ComponentReference("sendTo", &this->sendTo, true)) return 1;

    this->sendToId = this->sendTo->Connect(this->throughput);

    if (this->UseDelayBuffer()) {
        this->SetDelayBufferSize();
        this->delayBuffer.Allocate(this->delayBufferSize, sizeof(Input));
    }

    return 0;
}

template <typename Type>
void DelayQueue<Type>::Clock() {
    this->cyclesClock++;

    Input input;
    if (this->UseDelayBuffer()) {
        while (this->Dequeue(&input) == 0) {
            this->sendTo->SendRequest(sendToId, &input.elem);
        }
    }
    long totalConnections = this->GetNumberOfConnections();
    for (long i = 0; i < totalConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &input.elem) == 0) {
            if (this->UseDelayBuffer()) {
                if (this->Enqueue(&input)) return;
            } else {
                if (this->sendTo->SendRequest(sendToId, &input.elem)) return;
            }
        }
    }
}

#ifndef NDEBUG
int TestDelayQueue();
#endif

#endif  // SINUCA3_DELAY_QUEUE
