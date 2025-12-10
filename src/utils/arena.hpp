#ifndef SINUCA3_ARENA_HPP_
#define SINUCA3_ARENA_HPP_

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
 * @file arena.hpp
 * @brief Public API and inline implementation of a growable arena for SiNUCA3.
 */

#include <cstdlib>

class Arena {
  private:
    unsigned long top;
    unsigned long size;
    unsigned char* mem;
    Arena* next;

  public:
    inline Arena(unsigned long size)
        : top(0),
          size(size + (sizeof(void*) - size % sizeof(void*))),
          mem(new unsigned char[size + (sizeof(void*) - size % sizeof(void*))]),
          next(NULL) {}

    inline void* Alloc(unsigned long size) {
        size = size + (sizeof(void*) - size % sizeof(void*));
        if (this->top + size > this->size) {
            if (this->next == NULL) {
                this->next = new Arena(this->size > size ? this->size : size);
                if (this->next == NULL) return NULL;
            }
            return this->next->Alloc(size);
        }

        void* const ptr = (void*)&this->mem[top];
        this->top += size;
        return ptr;
    }

    inline ~Arena() {
        delete[] this->mem;
        if (this->next != NULL) {
            delete this->next;
        }
    }
};

#endif  // SINUCA3_ARENA_HPP_
