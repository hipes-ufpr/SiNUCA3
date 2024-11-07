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

namespace sinuca {

// Pre-declaration for ConfigValue as Linkable should not be included here as it
// already includes us.
namespace engine {
class Linkable;
}

namespace config {

/**
 * @brief Each configuration parameter type supported for components.
 */
enum ConfigValueType {
    ConfigValueTypeInteger,
    ConfigValueTypeNumber,
    ConfigValueTypeComponentReference,
};

/**
 * @brief A single configuration parameter.
 */
struct ConfigValue {
    ConfigValueType type;
    union {
        long integer;
        double number;
        engine::Linkable* componentReference;
    } value;
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_HPP
