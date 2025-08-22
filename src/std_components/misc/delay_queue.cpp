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
 * @file delay_queue.cpp
 * @brief Implementation of a test of the delay queue.
 */

#include "delay_queue.hpp"

#include "config/config.hpp"
#include "queue.hpp"

#ifndef NDEBUG

const long pipelineDelay = 2;
const long delay = 3 - pipelineDelay;
const long throughput = 4;

int TestDelayQueue() {
    DelayQueue<long> dq;
    QueueTester component;

    dq.SetConfigParameter("delay", ConfigValue((long)delay));
    dq.SetConfigParameter("throughput", ConfigValue((long)throughput));
    dq.SetConfigParameter("sendTo", ConfigValue(&component));
    dq.FinishSetup();

    long id = dq.Connect(throughput);
    long msg1 = 0xcafeefac;
    long msg2 = 0xdeaddaed;
    long msg3 = 0xb16b00b5;
    long msg4 = 0xbaaddaab;

    dq.SendRequest(id, &msg1);
    dq.SendRequest(id, &msg2);
    dq.SendRequest(id, &msg3);
    dq.SendRequest(id, &msg4);

    dq.Clock();
    component.Clock();
    dq.PosClock();
    component.PosClock();
    if (component.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d was not expecting a message.\n",
                             __FILE__, __LINE__);
        return 1;
    }
    // 1
    dq.Clock();
    component.Clock();
    dq.PosClock();
    component.PosClock();
    if (component.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d was not expecting a message.\n",
                             __FILE__, __LINE__);
        return 1;
    }
    // 2
    dq.Clock();
    component.Clock();
    dq.PosClock();
    component.PosClock();
    // 3
    dq.Clock();

    long msg = component.GetMessage();
    if (msg != msg1) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component.GetMessage();
    if (msg != msg2) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component.GetMessage();
    if (msg != msg3) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component.GetMessage();
    if (msg != msg4) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }

    dq.PosClock();
    component.PosClock();

    return 0;
}

#endif