#ifndef SINUCA3_YAML_PARSER_HPP_
#define SINUCA3_YAML_PARSER_HPP_

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
 * @file yaml_parser.cpp
 * @brief Public API of YAML parsing for the SiNUCA3.
 * @details This contains some types and a single function: Parse. For the
 * implementation details, check yaml_parser.cpp.
 */

#include <cassert>
#include <cstdlib>
#include <vector>

namespace sinuca {
namespace yaml {

/**
 * @brief Types of YAML.
 */
enum YamlValueType {
    YamlValueTypeNumber,
    YamlValueTypeBoolean,
    YamlValueTypeString,
    YamlValueTypeAlias,
    YamlValueTypeMapping,
    YamlValueTypeArray,
};

// Pre-declaration for YamlMappingEntry.
struct YamlValue;

/**
 * @brief This is an entry in a YAML mapping.
 */
struct YamlMappingEntry {
    const char* name; /** @brief The key in the mapping. */
    YamlValue* value; /** @brief It's value. */

    // The constructors and destructors of this struct are defined after
    // YamlValue because "deleting incomplete type may cause undefined
    // behaviour".
    YamlMappingEntry(const char* name, YamlValue* value);
    ~YamlMappingEntry();
};

/**
 * @brief a generic YAML value. Tagged union.
 */
struct YamlValue {
    union {
        double number;
        bool boolean;
        const char* string;
        const char* alias;
        std::vector<YamlValue*>* array;
        std::vector<YamlMappingEntry*>* mapping;
    } value;            /** @brief The value of the tagged union. */
    YamlValueType type; /** @brief The tag of the tagged union. */

    /** @brief Constructs a value from a double. */
    inline YamlValue(double value) : type(YamlValueTypeNumber) {
        this->value.number = value;
    }

    /** @brief Constructs a value from a boolean. */
    inline YamlValue(bool value) : type(YamlValueTypeBoolean) {
        this->value.boolean = value;
    }

    /**
     * @brief Constructs a value WITHOUT INITIALIZING IT, just setting it's
     * type.
     */
    inline YamlValue(YamlValueType type) : type(type) {
        switch (this->type) {
            case YamlValueTypeAlias:
                this->value.alias = NULL;
                break;
            case YamlValueTypeString:
                this->value.string = NULL;
                break;
            // 8 is a good heuristic.
            case YamlValueTypeArray:
                this->value.array = new std::vector<YamlValue*>;
                this->value.array->reserve(8);
                break;
            case YamlValueTypeMapping:
                this->value.mapping = new std::vector<YamlMappingEntry*>;
                this->value.mapping->reserve(8);
                break;
            default:
                break;
        }
    }

    inline ~YamlValue() {
        switch (this->type) {
            case YamlValueTypeAlias:
                delete[] this->value.alias;
                break;
            case YamlValueTypeString:
                delete[] this->value.string;
                break;
            case YamlValueTypeArray:
                for (unsigned int i = 0; i < this->value.array->size(); ++i)
                    delete (*this->value.array)[i];
                delete this->value.array;
                break;
            case YamlValueTypeMapping:
                for (unsigned int i = 0; i < this->value.mapping->size(); ++i)
                    delete (*this->value.mapping)[i];
                delete this->value.mapping;
                break;
            default:
                break;
        }
    }
};

// See the comment on the struct definition.
inline YamlMappingEntry::YamlMappingEntry(const char* name, YamlValue* value)
    : name(name), value(value) {}
inline YamlMappingEntry::~YamlMappingEntry() {
    delete[] this->name;
    delete this->value;
}

/**
 * @details Opens a configuration file by name and parses it, including it's
 * include directives. If a NULL is returned, an error occurred. The caller may
 * safely do it's own cleanup stuff without bothering in logging the error as
 * the function already does log the problems in the configuration files with
 * the utilities provided in logging.hpp.
 * @param configFile name of the root configuration file.
 * @return NULL on error. If not NULL, the caller must delete the returned value
 * sometime.
 */
YamlValue* ParseFile(const char* configFile);

#ifndef NDEBUG
/**
 * @brief Debug purpouses.
 */
void PrintYaml(YamlValue* value);
#endif  // NDEBUG

}  // namespace yaml
}  // namespace sinuca

#endif  // SINUCA3_YAML_PARSER_HPP_
