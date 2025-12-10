
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
 * @file queue.cpp
 * @details Implementation of the template Queue, a generic fetching queue that
 * can be used to enqueue results from caches, predictors, etc. You need to
 * create a proper component that instantiates this template class to use it.
 */

#include "queue.hpp"

#include <config/config.hpp>
#include <sinuca3.hpp>

#ifndef NDEBUG

/** @brief Test for the Queue template class. */
int TestQueue() {
    Queue<long> queue;
    QueueTester tester;

    Map<Linkable*> aliases;
    yaml::Parser parser;
    aliases.Insert("tester", &tester);

    queue.Configure(CreateFakeConfig(&parser,
                                     "sendTo: *tester\n"
                                     "throughput: 3\n",
                                     &aliases));

    int id = queue.Connect(3);

    queue.Clock();
    queue.PosClock();
    tester.PosClock();

    long msg1 = 0xcafebabe;
    long msg2 = 0xdeadbeef;
    long msg3 = 0xb16b00b5;
    long msg4 = 0xbaadf00d;

    queue.SendRequest(id, &msg1);

    queue.Clock();
    queue.PosClock();
    tester.PosClock();

    queue.Clock();
    queue.PosClock();
    tester.PosClock();

    queue.Clock();

    // What would be tester.Clock().
    long msg = tester.GetMessage();
    if (msg != msg1) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    if (tester.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d got more than one message.\n",
                             __FILE__, __LINE__);
        return 1;
    }

    queue.PosClock();
    tester.PosClock();

    queue.SendRequest(id, &msg1);
    queue.SendRequest(id, &msg2);
    queue.SendRequest(id, &msg3);
    if (queue.SendRequest(id, &msg4) == 0) {
        SINUCA3_ERROR_PRINTF(
            "TestQueue %s:%d sucessfully send more messages than it should.\n",
            __FILE__, __LINE__);
        return 1;
    }

    queue.Clock();
    queue.PosClock();
    tester.PosClock();

    queue.Clock();
    queue.PosClock();
    tester.PosClock();

    queue.Clock();

    // What would be tester.Clock().
    msg = tester.GetMessage();
    if (msg != msg1) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d msg1 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = tester.GetMessage();
    if (msg != msg2) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d msg2 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    msg = tester.GetMessage();
    if (msg != msg3) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d msg2 is %ld\n", __FILE__,
                             __LINE__, msg);
        return 1;
    }
    if (tester.GetMessage() != 0) {
        SINUCA3_ERROR_PRINTF("TestQueue %s:%d got more than three messages.\n",
                             __FILE__, __LINE__);
        return 1;
    }

    return 0;
}

#endif  // NDEBUG
