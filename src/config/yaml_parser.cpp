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
 * @brief Implementation of YAML parsing for the SiNUCA3.
 * @details The parser works
 */

#include "yaml_parser.hpp"

#include <yaml.h>

#include <cstdio>
#include <vector>

#include "../utils/logging.hpp"

static inline sinuca::yaml::YamlValue* ParseYamlValue(yaml_parser_t* parser);
static inline sinuca::yaml::YamlValue* ParseYamlValueFromEvent(
    yaml_parser_t* parser, yaml_event_t* event);

static inline int YamlParseAndLogError(yaml_parser_t* parser,
                                       yaml_event_t* event) {
    if (!yaml_parser_parse(parser, event)) {
        SINUCA3_ERROR_PRINTF("while reading config file %s: %s\n",
                             parser->context, parser->problem);
        return 1;
    }

    return 0;
}

static inline int EnsureFileIsYamlMapping(yaml_parser_t* parser) {
    yaml_event_t event;

    // Assert toplevel is sane: stream start, then document, then mapping.
    if (YamlParseAndLogError(parser, &event)) return 1;
    if (event.type != YAML_STREAM_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file: file is not a YAML mappnig.\n");
        yaml_event_delete(&event);
        return 1;
    }
    yaml_event_delete(&event);
    if (YamlParseAndLogError(parser, &event)) return 1;
    if (event.type != YAML_DOCUMENT_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file: file is not a YAML mappnig.\n");
        yaml_event_delete(&event);
        return 1;
    }
    yaml_event_delete(&event);
    if (YamlParseAndLogError(parser, &event)) return 1;
    if (event.type != YAML_MAPPING_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file: file is not a YAML mappnig.\n");
        yaml_event_delete(&event);
        return 1;
    }

    yaml_event_delete(&event);
    return 0;
}

static inline sinuca::yaml::YamlMappingEntry* ParseMappingEntry(
    yaml_parser_t* parser, const char* name) {
    sinuca::yaml::YamlValue* value = ParseYamlValue(parser);
    if (value == NULL) return NULL;

    return new sinuca::yaml::YamlMappingEntry(new std::string(name), value);
}

static inline sinuca::yaml::YamlValue* ParseMapping(yaml_parser_t* parser) {
    sinuca::yaml::YamlValue* mapping =
        new sinuca::yaml::YamlValue(sinuca::yaml::YamlValueTypeMapping);

    yaml_event_t event;
    yaml_event_type_t eventType;
    do {
        if (YamlParseAndLogError(parser, &event)) {
            delete mapping;
            return NULL;
        }

        eventType = event.type;
        if (eventType == YAML_SCALAR_EVENT) {
            sinuca::yaml::YamlMappingEntry* entry =
                ParseMappingEntry(parser, (const char*)event.data.scalar.value);
            if (entry == NULL) {
                delete mapping;
                return NULL;
            }
            mapping->value.mapping->push_back(entry);
        }
#ifndef NDEBUG
        else if (eventType != YAML_MAPPING_END_EVENT) {
            SINUCA3_DEBUG_PRINTF(
                "%s:%d: Mapping parser got a strange event: %d\n",
                __FILE_NAME__, __LINE__, eventType);
        }
#endif
        yaml_event_delete(&event);
    } while (eventType != YAML_MAPPING_END_EVENT);
    return mapping;
}

static inline sinuca::yaml::YamlValue* ParseSequence(yaml_parser_t* parser) {
    sinuca::yaml::YamlValue* array =
        new sinuca::yaml::YamlValue(sinuca::yaml::YamlValueTypeArray);

    yaml_event_t event;

    // The loop exits by returning the array when event.type is
    // YAML_SEQUENCE_EVENT.
    for (;;) {
        if (YamlParseAndLogError(parser, &event)) {
            delete array;
            return NULL;
        }

        // Exits by returning the array.
        if (event.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&event);
            return array;
        }

        sinuca::yaml::YamlValue* entry =
            ParseYamlValueFromEvent(parser, &event);
        yaml_event_delete(&event);
        if (entry == NULL) {
            delete array;
            return NULL;
        }
        array->value.array->push_back(entry);
    }
}

static inline sinuca::yaml::YamlValue* ParseScalar(const char* scalar) {
    double number;
    if (sscanf(scalar, "%lf", &number) == 1)
        return new sinuca::yaml::YamlValue(number);
    if (!strcmp(scalar, "true") || !strcmp(scalar, "yes"))
        return new sinuca::yaml::YamlValue(true);
    if (!strcmp(scalar, "false") || !strcmp(scalar, "no"))
        return new sinuca::yaml::YamlValue(false);

    sinuca::yaml::YamlValue* value =
        new sinuca::yaml::YamlValue(sinuca::yaml::YamlValueTypeString);
    value->value.string = new std::string(scalar);
    return value;
}

static inline sinuca::yaml::YamlValue* ParseYamlValueFromEvent(
    yaml_parser_t* parser, yaml_event_t* event) {
    sinuca::yaml::YamlValue* value = NULL;
    switch (event->type) {
        case YAML_ALIAS_EVENT:
            value =
                new sinuca::yaml::YamlValue(sinuca::yaml::YamlValueTypeAlias);
            value->value.alias =
                new std::string((const char*)event->data.alias.anchor);
            break;
        case YAML_SCALAR_EVENT:
            value = ParseScalar((const char*)event->data.scalar.value);
            break;
        case YAML_MAPPING_START_EVENT:
            value = ParseMapping(parser);
            break;
        case YAML_SEQUENCE_START_EVENT:
            value = ParseSequence(parser);
            break;
        default:
            SINUCA3_DEBUG_PRINTF(
                "%s:%d: YamlValue parser got a strange event: %d\n",
                __FILE_NAME__, __LINE__, event->type);
            break;
    }
    return value;
}

static inline sinuca::yaml::YamlValue* ParseYamlValue(yaml_parser_t* parser) {
    yaml_event_t event;
    if (YamlParseAndLogError(parser, &event)) return NULL;

    sinuca::yaml::YamlValue* value = ParseYamlValueFromEvent(parser, &event);
    yaml_event_delete(&event);
    return value;
}

sinuca::yaml::YamlValue* sinuca::yaml::ParseFile(
    const std::string* configFile) {
    FILE* fp = fopen(configFile->c_str(), "r");
    if (fp == NULL) return NULL;

    // This only fails on allocation failure, and we don't care about them lmao.
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    // We need to make sure the top level is a mapping because another thing
    // would make no sense.
    YamlValue* yaml = NULL;
    if (!EnsureFileIsYamlMapping(&parser)) yaml = ParseMapping(&parser);

    yaml_parser_delete(&parser);

    fclose(fp);
    return yaml;
}

#ifndef NDEBUG

void sinuca::yaml::PrintYaml(YamlValue* value) {
    switch (value->type) {
        case sinuca::yaml::YamlValueTypeBoolean:
            printf("%s\n", value->value.boolean ? "true" : "false");
            break;
        case sinuca::yaml::YamlValueTypeNumber:
            printf("%f\n", value->value.number);
            break;
        case sinuca::yaml::YamlValueTypeString:
            printf("%s\n", value->value.string->c_str());
            break;
        case sinuca::yaml::YamlValueTypeAlias:
            printf("%s\n", value->value.alias->c_str());
            break;
        case sinuca::yaml::YamlValueTypeArray:
            printf("[\n");
            for (unsigned int i = 0; i < value->value.array->size(); ++i)
                PrintYaml((*value->value.array)[i]);
            printf("]\n");
            break;
        case sinuca::yaml::YamlValueTypeMapping:
            printf("{\n");
            for (unsigned int i = 0; i < value->value.mapping->size(); ++i) {
                printf("%s: ", ((*value->value.mapping)[i])->name->c_str());
                PrintYaml(((*value->value.mapping)[i])->value);
            }
            printf("}\n");
            break;
    }
}

#endif  // NDEBUG
