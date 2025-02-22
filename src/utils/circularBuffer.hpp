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
 * @file circularBuffer.hpp
 * @brief Circular Buffer Class
 * @details This class implements a Circular Buffer, useful for several other
 * classes within the simulator.
 */

#include <cstddef>
#include <cstring>

class CircularBuffer {
  private:
    void* buffer;      /**<The Buffer. */
    int occupation;    /**<Buffer's current occupancy */
    int bufferSize;    /**<The maximum buffer capacity. */
    int messageSize;   /**<The message size supported by the buffer. */
    int startOfBuffer; /**<Sentinel to the start of the buffer. */
    int endOfBuffer;   /*<Sentinel for the end of the buffer. */

  public:
    CircularBuffer()
        : buffer(NULL),
          occupation(0),
          bufferSize(0),
          messageSize(0),
          startOfBuffer(0),
          endOfBuffer(0){};

    /**
     * @brief Returns a boolean indicating whether the Buffer is allocated.
     */
    inline bool IsAllocated() const;

    /**
     * @brief Returns the size of Buffer.
     */
    inline int GetSize() const;

    /**
     * @brief Returns the occupation of Buffer.
     */
    inline int GetOccupation() const;

    /**
     * @brief Returns a boolean indicating whether the Buffer is full.
     */
    inline bool IsFull() const;

    /**
     * @brief Returns a boolean indicating whether the Buffer is empty.
     */
    inline bool IsEmpty() const;

    /**
     * @brief Allocates the structure of a Circular Buffer.
     * @param bufferSize self-explanatory.
     * @param messageSize self-explanatory.
     */
    void Allocate(int bufferSize, int messageSize);

    /**
     * @brief Deallocates the Circular Buffer.
     * @details This method is called by the class destructor or if buffer
     * deallocation is necessary.
     */
    void Deallocate();

    /**
     * @brief Inserts the element at the "top" of the buffer.
     * @param elementInput A pointer to the element to be inserted.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool Enqueue(void* elementInput);

    /**
     * @brief Removes and returns the element contained in the "base" of the
     * Buffer.
     * @param elementOutput A pointer to the memory region where the element
     * will be returned.
     * @return 1 if successfuly, 0 otherwise.
     */
    bool Dequeue(void* elementOutput);

    ~CircularBuffer() { Deallocate(); };
};

#endif