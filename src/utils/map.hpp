#ifndef SINUCA3_MAP_HPP_
#define SINUCA3_MAP_HPP_

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
 * @file map.hpp
 * @details Implementation of a generic write-only map with strings as keys.
 */

#include <cstring>
#include <utils/arena.hpp>

namespace map {

/**
 * @brief P for hashing. Must be a prime roughly the size of the input
 * alphabet. Heuristically we decided 71 is rhoughly the size of our expected
 * alphabet.
 */
const int P = 71;

/**
 * @brief M for hashing (table size). Size of a page.
 */
const int M = 4096;

/**
 * @brief Polynomial rolling hash function, implemented in hash.cpp.
 */
unsigned int Hash(const unsigned char* const buffer, unsigned long size);

template <typename Element>
struct Node {
    Node* next;
    const char* key;
    Element value;
};

}  // namespace map

/**
 * @brief A generic write-only map with char* as keys.
 */
template <typename Element>
class Map {
  private:
    Arena arena;
    map::Node<Element>* table[map::M];

  public:
    Map() : arena(4096) {
        memset(this->table, 0, map::M * sizeof(*this->table));
    };

    void Insert(const char* const key, const Element value) {
        unsigned long keyLen = strlen(key) * sizeof(char);
        unsigned int pos = map::Hash((const unsigned char* const)key, keyLen);

        map::Node<Element>** nodePtr = &this->table[pos];
        while (*nodePtr != NULL) nodePtr = &(*nodePtr)->next;

        *nodePtr = (map::Node<Element>*)this->arena.Alloc(sizeof(**nodePtr));
        (*nodePtr)->key = (char*)this->arena.Alloc(keyLen + 1);
        (*nodePtr)->next = NULL;

        memcpy((void*)(*nodePtr)->key, key, keyLen + 1);
        (*nodePtr)->value = value;
    }

    Element* Get(const char* const key) {
        unsigned int pos =
            map::Hash((const unsigned char* const)key, strlen(key));

        for (map::Node<Element>* node = this->table[pos]; node != NULL;
             node = node->next) {
            if (strcmp(node->key, key) == 0) return &node->value;
        }

        return NULL;
    }
};

/**
 * @brief a char* -> char* map.
 */
class StringMap {
  private:
    Arena arena;
    map::Node<char*>* table[map::M];

  public:
    inline StringMap() : arena(4096) {
        memset(this->table, 0, map::M * sizeof(*this->table));
    };

    const char* Insert(const char* key, const char* value);
    const char* Get(const char* const key);
};

#ifndef NDEBUG

int TestHashMap();

#endif  // NDEBUG

#endif  // SINUCA3_MAP_HPP_
