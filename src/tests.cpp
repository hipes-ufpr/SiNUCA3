#ifndef NDEBUG

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
 * @file tests.cpp
 * @details Defines the tests the simulator supports. A test is a function with
 * the signature `int()` that returns 0 if the test succeeds and a number
 * greater than zero otherwise. To add a test to the infrastructure, go to the
 * file `tests.cpp` and declare your test inside the function `Test` with the
 * `TEST()` macro.
 */

#include "tests.hpp"

#include <sinuca3.hpp>
#include <std_components/misc/queue.hpp>
#include <std_components/misc/delay_queue.hpp>
#include <std_components/predictors/ras.hpp>

int TestExample() {
    SINUCA3_LOG_PRINTF("Hello, World!\n");

    return 0;
}

/**
 * @brief Runs a test by name.
 */
int Test(const char* test) {
    TEST(TestExample);
    TEST(TestRas);
    TEST(TestQueue);
    TEST(TestDelayQueue);

    return -1;
}

#endif
