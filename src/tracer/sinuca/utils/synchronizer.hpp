#ifndef SINUCA3_SINUCA_TRACER_SYNCHRONIZER_HPP_
#define SINUCA3_SINUCA_TRACER_SYNCHRONIZER_HPP_

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
 * @file
 * @brief
 * @details
 */

#include <sinuca3.hpp>
#include <tracer/sinuca/file_handler.hpp>
#include <utils/circular_buffer.hpp>

#include <search.h>

typedef int THREADID;

enum ThreadState {
    ThreadStateUndefined,
    ThreadStateSleeping,
    ThreadStateActive
};

struct ThreadLock {
    bool busy;
    CircularBuffer queue;

    inline ThreadLock() : busy(0) {
        queue.Allocate(0, sizeof(THREADID));
    }
};

struct ThreadBarrier {
    bool busy;
    int cont;

    inline ThreadBarrier() : busy(0), cont(0) {}
};

class Synchro {
  private:
    hsearch_data* hlocks;
    ThreadBarrier barrier;
    int totalThreads;

  public:
    inline Synchro() : hlocks(NULL), totalThreads(0) {}
    inline ~Synchro() {
        if (this->hlocks) {
            hdestroy_r(this->hlocks);
        }
    }

    ThreadState HandleLockRequest(int lockId);
    ThreadState HandleBarrierWait();

    inline void IncTotalThreads() {++this->totalThreads;}
};

#endif