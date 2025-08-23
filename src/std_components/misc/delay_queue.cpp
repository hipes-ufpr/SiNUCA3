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

const long inherentDelay = 2;
const long delay = 3 - inherentDelay;

int TestDelayQueue() {
    DelayQueue<long> dq1;
    QueueTester component1;

    dq1.SetConfigParameter("delay", ConfigValue((long)delay));
    dq1.SetConfigParameter("throughput", ConfigValue((long)4));
    dq1.SetConfigParameter("sendTo", ConfigValue(&component1));
    dq1.FinishSetup();

    long id = dq1.Connect(4);
    long msg1 = 0xcafeefac;
    long msg2 = 0xdeaddaed;
    long msg3 = 0xb16b00b5;
    long msg4 = 0xbaaddaab;

    dq1.SendRequest(id, &msg1);
    dq1.SendRequest(id, &msg2);
    dq1.SendRequest(id, &msg3);
    dq1.SendRequest(id, &msg4);

    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();
    if (component1.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d was not expecting a message.\n",
                             __FILE__, __LINE__);
        return 1;
    }
    // 1
    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();
    if (component1.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d was not expecting a message.\n",
                             __FILE__, __LINE__);
        return 1;
    }
    // 2
    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();
    // 3
    dq1.Clock();

    long msg = component1.GetMessage();
    if (msg != msg1) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component1.GetMessage();
    if (msg != msg2) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg2 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component1.GetMessage();
    if (msg != msg3) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg3 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = component1.GetMessage();
    if (msg != msg4) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg4 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }

    dq1.PosClock();
    component1.PosClock();

    long msg5 = 0xfefefe;
    dq1.SendRequest(id, &msg5);

    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();

    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();

    dq1.Clock();
    component1.Clock();
    dq1.PosClock();
    component1.PosClock();

    dq1.Clock();

    msg = component1.GetMessage();
    if (msg != msg5) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg5 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }

    dq1.PosClock();
    component1.PosClock();

    DelayQueue<long> dq2;
    QueueTester component2;
    dq2.SetConfigParameter("delay", ConfigValue((long)0));  // Zero delay
    dq2.SetConfigParameter("throughput", ConfigValue((long)1));
    dq2.SetConfigParameter("sendTo", ConfigValue(&component2));
    dq2.FinishSetup();
    id = dq2.Connect(1);

    long msg8 = 0xb00b1e;
    dq2.SendRequest(id, &msg8);

    dq2.Clock();
    component2.Clock();
    dq2.PosClock();
    component2.PosClock();

    dq2.Clock();
    component2.Clock();
    dq2.PosClock();
    component2.PosClock();

    dq2.Clock();

    msg = component2.GetMessage();
    if (msg != msg8) {
        SINUCA3_ERROR_PRINTF("DelayQueue %s:%d msg8 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }

    dq2.PosClock();
    component2.PosClock();

    return 0;
}

#endif