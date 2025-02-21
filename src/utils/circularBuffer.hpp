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
    void* buffer;
    int occupation;
    int bufferSize;
    int messageSize;
    int startOfBuffer;
    int endOfBuffer;

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
     * @brief Inserts the element at the "top" of the buffer.
     */
    int Enqueue(void* element);

    /**
     * @brief Removes and returns the element contained in the "base" of the
     * Buffer.
     */
    void* Dequeue();

    ~CircularBuffer() {
        if (this->buffer) {
            delete[] (char*)this->buffer;
        }
    };
};