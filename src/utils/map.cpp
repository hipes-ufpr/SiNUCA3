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
 * @file map.cpp
 * @details Implementation of the map.
 */

#include <cassert>
#include <cstring>
#include <utils/map.hpp>

#include "utils/logging.hpp"

unsigned int map::Hash(const unsigned char* const buffer, unsigned long size) {
    if (size <= 0) return 0;

    unsigned int s = buffer[0];
    unsigned int p = map::P;

    for (unsigned long i = 1; i < size; ++i) {
        s += buffer[i] * p;
        p *= map::P;
    }

    return s % map::M;
}

const char* StringMap::Insert(const char* key, const char* value) {
    unsigned long keyLen = strlen(key) * sizeof(char);
    unsigned long valueLen = strlen(value) * sizeof(char);
    unsigned int pos = map::Hash((const unsigned char* const)key, keyLen);

    map::Node<char*>** nodePtr = &this->table[pos];
    while (*nodePtr != NULL) nodePtr = &(*nodePtr)->next;

    *nodePtr = (map::Node<char*>*)this->arena->Alloc(sizeof(**nodePtr));
    (*nodePtr)->key = (char*)this->arena->Alloc(keyLen);
    (*nodePtr)->value = (char*)this->arena->Alloc(valueLen);
    (*nodePtr)->next = NULL;

    memcpy((void*)(*nodePtr)->key, key, keyLen + 1);
    memcpy((void*)(*nodePtr)->value, value, valueLen + 1);

    return (*nodePtr)->value;
}

const char* StringMap::Get(const char* const key) {
    unsigned int pos = map::Hash((const unsigned char* const)key, strlen(key));

    for (map::Node<char*>* node = this->table[pos]; node != NULL;
         node = node->next) {
        if (strcmp(node->key, key) == 0) return node->value;
    }

    return NULL;
}

const char* StringMap::Next(char** elementRet) {
    if (this->iteratorPtr != NULL) {
        if (this->iteratorPtr->next != NULL) {
            this->iteratorPtr = this->iteratorPtr->next;
            *elementRet = this->iteratorPtr->value;
            return this->iteratorPtr->key;
        } else {
            this->iteratorIdx += 1;
            if (this->iteratorIdx >= map::M) {
                this->ResetIterator();
                return NULL;
            }
        }
    }

    for (; this->iteratorIdx < map::M; ++this->iteratorIdx) {
        if (this->table[this->iteratorIdx] != NULL) {
            this->iteratorPtr = this->table[this->iteratorIdx];
            *elementRet = this->iteratorPtr->value;
            return this->iteratorPtr->key;
        }
    }

    this->ResetIterator();
    return NULL;
}

#ifndef NDEBUG

int TestHashMap() {
    Map<unsigned int> intMap;

    intMap.Insert("foo", 0xcafebabe);
    intMap.Insert("bar", 0xb15b00b5);

    if (*intMap.Get("foo") != 0xcafebabe) {
        SINUCA3_ERROR_PRINTF("HashMap test failed at %s:%u\n", __FILE__,
                             __LINE__);
        return 1;
    }
    if (*intMap.Get("bar") != 0xb15b00b5) {
        SINUCA3_ERROR_PRINTF("HashMap test failed at %s:%u\n", __FILE__,
                             __LINE__);
        return 1;
    }

    StringMap stringMap;

    stringMap.Insert("hello", "world");
    stringMap.Insert("foo", "bar");

    if (strcmp(stringMap.Get("hello"), "world") != 0) {
        SINUCA3_ERROR_PRINTF("HashMap test failed at %s:%u\n", __FILE__,
                             __LINE__);
        return 1;
    }
    if (strcmp(stringMap.Get("foo"), "bar") != 0) {
        SINUCA3_ERROR_PRINTF("HashMap test failed at %s:%u\n", __FILE__,
                             __LINE__);
        return 1;
    }

    return 0;
}

#endif  // NDEBUG
