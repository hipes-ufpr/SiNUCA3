#include "linkable.hpp"

sinuca::engine::Linkable::Linkable(long bufferSize, long numberOfBuffers)
    : bufferSize(bufferSize), numberOfBuffers(numberOfBuffers) {}

sinuca::engine::Linkable::~Linkable() {
    delete[] this->buffers;
}
