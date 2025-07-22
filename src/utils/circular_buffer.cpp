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
 * @file circular_buffer.cpp
 * @brief Implementation of the CircularBuffer class.
 */

#include "circular_buffer.hpp"

#include <cstdlib>

const int defaultBufferSize = 8;

void CircularBuffer::Allocate(int bufferSize, int elementSize) {
    if (elementSize == 0) return;

    this->occupation = 0;
    this->startOfBuffer = 0;
    this->endOfBuffer = 0;
    this->elementSize = elementSize;
    this->bufferSize = bufferSize;

    this->maxBufferSize = bufferSize;

    if (bufferSize == 0) {
        this->buffer = (void*)malloc(defaultBufferSize * elementSize);
        this->bufferSize = defaultBufferSize;
    } else {
        this->buffer = (void*)malloc(bufferSize * elementSize);
    }

    if (!this->buffer) {
        this->buffer = NULL;
    }
}

void CircularBuffer::Deallocate() {
    if (this->buffer) {
        free(this->buffer);
        this->buffer = NULL;
    }
}

bool CircularBuffer::Enqueue(void* elementInput) {
    if (this->IsFull()) {
        if (this->maxBufferSize != 0) {
            return 1;
        }

        void* newBuffer =
            (void*)malloc(this->bufferSize * 2 * this->elementSize);
        if (this->startOfBuffer >= this->endOfBuffer) {
            long offset = this->bufferSize - this->startOfBuffer;
            memcpy(newBuffer, &((char*)this->buffer)[this->startOfBuffer],
                   offset);
            memcpy(&((char*)newBuffer)[offset], this->buffer,
                   this->bufferSize - offset);
            this->startOfBuffer = 0;
            this->endOfBuffer = this->bufferSize - 1;
        } else {
            memcpy(newBuffer, this->buffer, this->bufferSize * elementSize);
        }

        this->bufferSize *= 2;
        free(this->buffer);
        this->buffer = newBuffer;
    }

    /*
     * Target stores the memory address where the element should be
     * inserted, based on pointer arithmetic. After its definition, memcpy
     * stores the element in the most recent position in the buffer.
     */
    void* memoryAddress = static_cast<char*>(this->buffer) +
                          (this->endOfBuffer * this->elementSize);

    memcpy(memoryAddress, elementInput, this->elementSize);
    ++this->occupation;
    ++this->endOfBuffer;

    if (this->endOfBuffer == this->bufferSize) {
        this->endOfBuffer = 0;
    }

    return 0;
}

bool CircularBuffer::Dequeue(void* elementOutput) {
    if (!this->IsEmpty()) {
        /*
         * Element stores the memory address of the oldest element in the Buffer
         * (the one that should be removed). Although there is no need to clear
         * the space of this element, the buffer limits are readjusted to avoid
         * unauthorized access.
         */
        void* memoryAddress = static_cast<char*>(this->buffer) +
                              (this->startOfBuffer * this->elementSize);

        memcpy(elementOutput, memoryAddress, this->elementSize);
        --this->occupation;
        ++this->startOfBuffer;

        if (this->startOfBuffer == this->bufferSize) {
            this->startOfBuffer = 0;
        }

        return 0;
    }

    memset(elementOutput, 0, this->elementSize);

    return 1;
}

void CircularBuffer::Flush() {
    this->occupation = 0;
    this->startOfBuffer = 0;
    this->endOfBuffer = 0;
}
