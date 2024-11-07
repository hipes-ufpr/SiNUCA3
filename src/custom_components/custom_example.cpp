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
 * @file custom_example.cpp
 * @brief Implements an example custom component to show how to create them.
 */

#include "custom_example.hpp"

#include "../sinuca3.hpp"

int CustomExample::SetConfigParameter(const char* parameter,
                                      sinuca::config::ConfigValue value) {
    (void)parameter;
    (void)value;

    return 1;
}

void CustomExample::Clock() {}

int CustomExample::FinishSetup() { return 0; }
