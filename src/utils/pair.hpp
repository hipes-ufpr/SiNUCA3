#ifndef SINUCA3_PAIR_HPP_
#define SINUCA3_PAIR_HPP_

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
void PushBackElemWithKey(std::vector<Pair<T, U> >* buffer, T key, U elem) {
    Pair<T, U> pair;
    pair.first = key;
    pair.second = elem;
    buffer->push_back(pair);
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
bool GetElemWithKey(std::vector<Pair<T, U> >* buffer, T key, U** elem) {
    for (unsigned long i = 0; i < buffer->size(); i++) {
        if (buffer->at(i).first == key) {
            *elem = &buffer->at(i).second;
            return true;
        }
    }
    return false;  // key not found
}

#endif  // SINUCA3_PAIR_HPP_