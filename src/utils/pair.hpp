#ifndef SINUCA3_PAIR_HPP_
#define SINUCA3_PAIR_HPP_

//
// Copyright (C) 2026  HiPES - Universidade Federal do Paraná
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
 * @file pair.hpp
 * @brief Pair implementation.
 * @details This file contains the implementation of a simple pair structure
 * and utility functions for managing pairs in a vector.
 */

#include <cstddef>
#include <vector>

template <typename T, typename U>
struct Pair {
    T first;
    U second;
};

template <typename T, typename U>
int ErasePairWithKey(std::vector<Pair<T, U> >* buffer, T key) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) {
            buffer->at(i) = buffer->back();
            buffer->pop_back();
            return 0;
        }
    }
    return 1;
}

template <typename T, typename U>
bool ContainsKey(std::vector<Pair<T, U> >* buffer, T key) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) return true;
    }
    return false;
}

template <typename T, typename U>
U* PushBackElemWithKey(std::vector<Pair<T, U> >* buffer, T key, U elem) {
    Pair<T, U> pair;
    pair.first = key;
    pair.second = elem;
    buffer->push_back(pair);
    return &buffer->back().second;
}

template <typename T, typename U>
void ChangeKeyOfElem(std::vector<Pair<T, U> >* buffer, T key, T newKey) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) {
            buffer->at(i).first = newKey;
            return;
        }
    }
}

template <typename T, typename U>
void UpdateElemWithKey(std::vector<Pair<T, U> >* buffer, T key, U newElem) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) {
            buffer->at(i).second = newElem;
            return;
        }
    }
}

template <typename T, typename U>
int GetElemWithKey(std::vector<Pair<T, U> >* buffer, T key, U** elem) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) {
            *elem = &buffer->at(i).second;
            return 0;
        }
    }
    *elem = NULL;
    return 1;  // key not found
}

#endif  // SINUCA3_PAIR_HPP_