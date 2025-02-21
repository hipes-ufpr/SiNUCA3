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

#include <cstddef>

inline bool sinuca::engine::CircularBuffer::IsAllocated() const {
    return (this->buffer != NULL);
};

inline int sinuca::engine::CircularBuffer::GetSize() const {
    return (this->bufferSize);
};

inline int sinuca::engine::CircularBuffer::GetOccupation() const {
    return (this->occupation);
};

inline bool sinuca::engine::CircularBuffer::IsFull() const {
    return (this->occupation == this->bufferSize);
};

inline bool sinuca::engine::CircularBuffer::IsEmpty() const {
    return (this->occupation == 0);
};

void sinuca::engine::CircularBuffer::Allocate(int bufferSize, int messageSize) {
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

int sinuca::engine::CircularBuffer::Enqueue(void* element) {
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

void* sinuca::engine::CircularBuffer::Dequeue() {
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

sinuca::engine::Linkable::Linkable(long bufferSize, long numberOfBuffers)
    : bufferSize(bufferSize), numberOfBuffers(numberOfBuffers) {}

sinuca::engine::Linkable::~Linkable() { delete[] this->buffers; }

void sinuca::engine::Linkable::PreClock() {}
void sinuca::engine::Linkable::PosClock() {}
