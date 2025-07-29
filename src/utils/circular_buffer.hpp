#ifndef SINUCA3_UTILS_CIRCULAR_BUFFER_HPP_
#define SINUCA3_UTILS_CIRCULAR_BUFFER_HPP_

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
 * @file circular_buffer.hpp
 * @brief Circular Buffer Class.
 * @details This class implements a Circular Buffer, useful for several other
 * classes within the simulator.
 */

#include <cstring>

/**
 * @brief A performant circular buffer.
 * @details You may use this as a queue. Use Allocate() to init it.
 */
class CircularBuffer {
  private:
    void* buffer;      /**<The Buffer. */
    int occupation;    /**<Buffer's current occupancy */
    int maxBufferSize; /**<The maximum buffer capacity. Zero if it can grow
                       undefinitely. */
    int bufferSize;    /**<Actual alloced buffer size. */
    int elementSize;   /**<The element size supported by the buffer. */
    int startOfBuffer; /**<Sentinel to the start of the buffer. */
    int endOfBuffer;   /**<Sentinel for the end of the buffer. */

  public:
    CircularBuffer()
        : buffer(NULL),
          occupation(0),
          maxBufferSize(0),
          bufferSize(0),
          elementSize(0),
          startOfBuffer(0),
          endOfBuffer(0) {};

    /**
     * @brief Returns a boolean indicating whether the Buffer is allocated.
     */
    inline bool IsAllocated() { return this->buffer != NULL; };

    /**
     * @brief Returns the size of Buffer.
     */
    inline int GetSize() { return this->bufferSize; };

    /**
     * @brief Returns the occupation of Buffer.
     */
    inline int GetOccupation() { return this->occupation; };

    /**
     * @brief Returns a boolean indicating whether the Buffer is full.
     */
    inline bool IsFull() { return this->occupation == this->bufferSize; };

    /**
     * @brief Returns a boolean indicating whether the Buffer is empty.
     */
    inline bool IsEmpty() { return this->occupation == 0; };

    /**
     * @brief Allocates the structure of a Circular Buffer.
     * @param bufferSize When > 0, sets a limit size, and trying to enqueue more
     * elements will result in an error. When 0, the buffer grows as needed.
     * @param elementSize Sets the element buffer. For instance, elementSize = 4
     * and bufferSize = 4 will allocate a circular buffer of 16 bytes.
     */
    void Allocate(int bufferSize, int elementSize);

    /**
     * @brief Deallocates the Circular Buffer.
     * @details This method is called by the class destructor or if buffer
     * deallocation is necessary.
     */
    void Deallocate();

    /**
     * @brief Inserts the element at the "top" of the buffer.
     * @param elementInput A pointer to the element to be inserted.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool Enqueue(void* elementInput);

    /**
     * @brief Removes and returns the element contained in the "base" of the
     * Buffer.
     * @param elementOutput A pointer to the memory region where the element
     * will be returned.
     * @return 0 if successfuly, 1 otherwise.
     */
    bool Dequeue(void* elementOutput);

    /**
     * @brief Flushes the buffer.
     * @details This method is called to clear the buffer.
     */
    void Flush();

    ~CircularBuffer() { this->Deallocate(); };
};

#endif
