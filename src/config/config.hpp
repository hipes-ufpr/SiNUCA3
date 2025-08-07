#ifndef SINUCA3_CONFIG_HPP
#define SINUCA3_CONFIG_HPP

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
 * @file config.hpp
 * @brief Configuration public API for SiNUCA3.
 */

#include <vector>

// Pre-declaration for ConfigValue as Linkable should not be included here as it
// already includes us.
class Linkable;

/**
 * @brief Each configuration parameter type supported for components.
 */
enum ConfigValueType {
    ConfigValueTypeInteger,
    ConfigValueTypeNumber,
    ConfigValueTypeBoolean,
    ConfigValueTypeArray,
    ConfigValueTypeComponentReference,
};

struct ConfigValue;
struct ConfigArray {
    ConfigValue* items;
    long size;
};

/**
 * @brief A single configuration parameter.
 */
struct ConfigValue {
    union {
        long integer;
        double number;
        bool boolean;
        std::vector<ConfigValue>* array;
        Linkable* componentReference;
    } value;
    ConfigValueType type;

    inline ConfigValue() {}

    inline ConfigValue(long integer) : type(ConfigValueTypeInteger) {
        this->value.integer = integer;
    }
    inline ConfigValue(double number) : type(ConfigValueTypeNumber) {
        this->value.number = number;
    }
    inline ConfigValue(bool boolean) : type(ConfigValueTypeBoolean) {
        this->value.boolean = boolean;
    }
    inline ConfigValue(std::vector<ConfigValue>* array)
        : type(ConfigValueTypeArray) {
        this->value.array = array;
    }
    inline ConfigValue(Linkable* componentReference)
        : type(ConfigValueTypeComponentReference) {
        this->value.componentReference = componentReference;
    }
};

#endif  // SINUCA3_CONFIG_HPP
