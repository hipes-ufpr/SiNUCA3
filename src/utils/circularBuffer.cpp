/**
 * @file circularBuffer.cpp
 * @brief Implementation of CircularBuffer class
 */

#include "circularBuffer.hpp"

inline bool CircularBuffer::IsAllocated() const {
    return (this->buffer != NULL);
};

inline int CircularBuffer::GetSize() const { return (this->bufferSize); };

inline int CircularBuffer::GetOccupation() const { return (this->occupation); };

inline bool CircularBuffer::IsFull() const {
    return (this->occupation == this->bufferSize);
};

inline bool CircularBuffer::IsEmpty() const { return (this->occupation == 0); };

void CircularBuffer::Allocate(int bufferSize, int messageSize) {
    this->occupation = 0;
    this->startOfBuffer = 0;
    this->endOfBuffer = 0;
    this->bufferSize = bufferSize;
    this->messageSize = messageSize;
    this->buffer = (void*)new char[bufferSize * messageSize];

    if (!(this->buffer)) {
        this->buffer = NULL;
    }
};

int CircularBuffer::Enqueue(void* element) {
    if (!(this->IsFull())) {
        /*
         * Target stores the memory address where the element should be
         * inserted, based on pointer arithmetic. After its definition, memcpy
         * stores the element in the most recent position in the buffer.
         */
        void* target = static_cast<char*>(buffer) + (endOfBuffer * messageSize);
        memcpy(target, element, messageSize);
        ++occupation;
        ++endOfBuffer;

        if (endOfBuffer == bufferSize) {
            endOfBuffer = 0;
        }

        return 1;
    }

    return 0;
};

void* CircularBuffer::Dequeue() {
    if (!(this->IsEmpty())) {
        /*
         * Element stores the memory address of the oldest element in the Buffer
         * (the one that should be removed). Although there is no need to clear
         * the space of this element, the buffer limits are readjusted to avoid
         * unauthorized access.
         */
        void* element =
            static_cast<char*>(buffer) + (startOfBuffer * messageSize);

        --occupation;
        ++startOfBuffer;

        if (startOfBuffer == bufferSize) {
            startOfBuffer = 0;
        }

        return element;
    }

    return NULL;
};