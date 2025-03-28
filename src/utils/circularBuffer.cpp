/**
 * @file circularBuffer.cpp
 * @brief Implementation of CircularBuffer class
 */

#include "circularBuffer.hpp"
#include <cstdio>

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
    if ((bufferSize == 0) || (messageSize == 0)) return;

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

void CircularBuffer::Deallocate() {
    if (this->buffer) {
        delete[] (char*)this->buffer;
        this->buffer = NULL;
    }
};

bool CircularBuffer::Enqueue(void* elementInput) {
    if (!(this->IsFull())) {
        /*
         * Target stores the memory address where the element should be
         * inserted, based on pointer arithmetic. After its definition, memcpy
         * stores the element in the most recent position in the buffer.
         */
        void* memoryAddress = static_cast<char*>(this->buffer) +
                              (this->endOfBuffer * this->messageSize);

        memcpy(memoryAddress, elementInput, this->messageSize);
        ++this->occupation;
        ++this->endOfBuffer;

        if (this->endOfBuffer == this->bufferSize) {
            this->endOfBuffer = 0;
        }

        return 1;
    }

    return 0;
};

bool CircularBuffer::Dequeue(void* elementOutput) {
    if (!(this->IsEmpty())) {
        /*
         * Element stores the memory address of the oldest element in the Buffer
         * (the one that should be removed). Although there is no need to clear
         * the space of this element, the buffer limits are readjusted to avoid
         * unauthorized access.
         */
        void* memoryAddress = static_cast<char*>(this->buffer) +
                              (this->startOfBuffer * this->messageSize);

        memcpy(elementOutput, memoryAddress, this->messageSize);
        --this->occupation;
        ++this->startOfBuffer;

        if (this->startOfBuffer == this->bufferSize) {
            this->startOfBuffer = 0;
        }

        return 1;
    }

    memset(elementOutput, 0, this->messageSize);

    return 0;
};