#ifndef SINUCA3_UTILS_LOGGING_HPP_
#define SINUCA3_UTILS_LOGGING_HPP_

//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
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
 * @file logging.hpp
 * @details Macros related to logging (in our case, just printing stuff to the
 * stdout or stderr).
 */

#include <cstdio>

#define SINUCA3_ERROR_PRINTF(...)     \
    {                                 \
        fprintf(stderr, "[ERROR] ");  \
        fprintf(stderr, __VA_ARGS__); \
    }

#define SINUCA3_WARNING_PRINTF(...)    \
    {                                  \
        fprintf(stderr, "[WARNING] "); \
        fprintf(stderr, __VA_ARGS__);  \
    }

#define SINUCA3_LOG_PRINTF(...) \
    { printf(__VA_ARGS__); }

#ifndef NDEBUG
#define SINUCA3_DEBUG_PRINTF(...) \
    {                             \
        printf("[DEBUG]");        \
        printf(__VA_ARGS__);      \
    }
#else
#define SINUCA3_DEBUG_PRINTF(...) \
    do {                          \
    } while (0)
#endif  // SINUCA3_DEBUG_PRINTF

#endif  // SINUCA3_UTILS_LOGGING_HPP_
