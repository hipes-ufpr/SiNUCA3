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

/* Métodos a comentar */
bool sinuca::engine::Buffer::isAllocated() const {
    return (this->buffer != NULL);
};

void sinuca::engine::Buffer::allocate(int bufferSize, int messageSize) {
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

int sinuca::engine::Buffer::enqueue(void* element) {
    if (occupation < bufferSize) {
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

void* sinuca::engine::Buffer::dequeue() {
    if (occupation > 0) {
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

int sinuca::engine::Buffer::getStart() { return startOfBuffer; };

int sinuca::engine::Buffer::getEnd() { return endOfBuffer; };

sinuca::engine::Linkable::Linkable(long bufferSize, long numberOfBuffers)
    : bufferSize(bufferSize), numberOfBuffers(numberOfBuffers) {}

sinuca::engine::Linkable::~Linkable() { delete[] this->buffers; }

void sinuca::engine::Linkable::PreClock() {}
void sinuca::engine::Linkable::PosClock() {}
