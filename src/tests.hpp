#ifndef SINUCA3_TESTS_HPP_
#define SINUCA3_TESTS_HPP_

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
 * @file tests.hpp
 * @brief Infrastructure to the tests the simulator supports.
 */

/** @brief An example test. */
int TestExample();
/** @brief Runs a test by it's name. */
int Test(const char* test);

#define TEST(func) \
    if (!strcmp(test, #func)) return func()

#endif  // SINUCA3_TESTS_HPP_
