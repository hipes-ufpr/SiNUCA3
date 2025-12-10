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

#include <yaml.h>

#include <cstdlib>
#include <cstring>
#include <utils/map.hpp>

#include "utils/arena.hpp"

namespace yaml {

/** @brief Types of yaml, without parsing strings. */
enum YamlValueType {
    YamlValueTypeString,
    YamlValueTypeAlias,
    YamlValueTypeArray,
    YamlValueTypeMapping,
};

struct YamlLocation {
    const char* file;
    unsigned long line;
    unsigned long column;
};

struct YamlValue;

struct YamlArray {
    YamlValue* elements;
    unsigned int size;
};

/**
 * @brief a generic YAML value. Tagged union + the anchor (C-style string,
 * deleted by the destructor).
 */
struct YamlValue {
    union {
        const char* string;
        const char* alias;
        YamlArray array;
        Map<YamlValue>* mapping;
    } value;               /** @brief The value of the tagged union. */
    YamlLocation location; /** @brief The location of the definition. */
    const char* anchor;    /** @brief The anchor. */
    YamlValueType type;    /** @brief The tag of the tagged union. */

    inline YamlValue() {}

    /**
     * @brief Constructs a value WITHOUT INITIALIZING IT, just setting it's
     * type.
     */
    inline YamlValue(YamlValueType type, YamlLocation location,
                     const char* anchor = NULL)
        : location(location), anchor(anchor), type(type) {
        memset(&this->value, 0, sizeof(this->value));
    }

    inline const char* TypeAsString() const {
        switch (this->type) {
            case YamlValueTypeString:
                return "string";
            case YamlValueTypeAlias:
                return "alias";
            case YamlValueTypeMapping:
                return "mapping";
            case YamlValueTypeArray:
                return "array";
        }
    }
};

/**
 * @brief The parser parses. A parser exists as an object instead of a function
 * to simplify memory management. All objects are alive as long as the parser
 * is.
 */
class Parser {
  private:
    Arena arena;
    yaml_parser_t parser;

    int ParseYamlValueFromEvent(yaml_event_t* event, const char* file,
                                YamlValue* ret);
    int ParseYamlValue(const char* file, YamlValue* ret);
    int ParseMapping(YamlLocation location, const char* anchor, YamlValue* ret);
    int ParseSequence(YamlLocation location, const char* anchor,
                      YamlValue* ret);

    int EnsureFileIsYamlMapping(YamlLocation* location, const char* file);
    int YamlParseAndLogError(yaml_event_t* event);

    int IncludeString(Map<YamlValue>* config, const char* string);
    int IncludeArray(Map<YamlValue>* config, YamlArray array,
                     YamlLocation location);
    int ProcessIncludeEntries(YamlValue* config);

  public:
    inline Parser() : arena(4096) {}

    /**
     * @details Same as ParseFile but deals with include entries in the
     * toplevel. Each `include` parameter in the root of each file will be
     * treated either as a file path or an array of file paths.
     * @param configFile name of the root configuration file.
     * @param ret Return. Alive until the parser is dropped.
     * @return NULL on error. If not NULL, the caller must delete the returned
     * value sometime.
     */
    int ParseFileWithIncludes(const char* const configFile,
                              YamlValue* const ret);

    /**
     * @details Opens a configuration file by name and parses it, including it's
     * include directives. The caller may safely do it's own cleanup stuff
     * without bothering in logging the error as the function already does log
     * the problems in the configuration files with the utilities provided in
     * logging.hpp. Every pointer returned via the ret tree is alive until the
     * parser is dropped.
     * @param configFile name of the root configuration file.
     * @param ret Return. Alive until the parser is dropped.
     * @return Non-zero on error.
     */
    int ParseFile(const char* const configFile, YamlValue* const ret);

    /**
     * @details Same as ParseFile but parses a string. Useful for testing.
     * @param string string to parse.
     * @param ret Return. Alive until the parser is dropped.
     * @return Non-zero on error.
     */
    int ParseString(const char* const string, YamlValue* const ret);
};

}  // namespace yaml

#endif  // SINUCA3_YAML_PARSER_HPP_
