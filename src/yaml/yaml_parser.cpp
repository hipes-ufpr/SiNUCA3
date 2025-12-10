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
 * @file yaml_parser.cpp
 * @brief Implementation of YAML parsing for the SiNUCA3.
 * @details It's a "recursive descent"-ish parser.
 */

#include "yaml_parser.hpp"

#include <yaml.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <utils/logging.hpp>
#include <vector>

static inline yaml::YamlLocation LocationFromMark(const yaml_mark_t mark,
                                                  const char* file) {
    yaml::YamlLocation location;
    location.column = mark.column;
    location.line = mark.line;
    location.file = file;
    return location;
}

int yaml::Parser::YamlParseAndLogError(yaml_event_t* event) {
    if (!yaml_parser_parse(&this->parser, event)) {
        SINUCA3_ERROR_PRINTF("while reading config file %s: %s\n",
                             this->parser.context, this->parser.problem);
        return 1;
    }

    return 0;
}

int yaml::Parser::EnsureFileIsYamlMapping(YamlLocation* location,
                                          const char* file) {
    yaml_event_t event;

    // Assert toplevel is sane: stream start, then document, then mapping.
    if (this->YamlParseAndLogError(&event)) return 1;
    if (event.type != YAML_STREAM_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file %s: file is not a YAML mappnig.\n",
            file);
        yaml_event_delete(&event);
        return 1;
    }
    yaml_event_delete(&event);
    if (this->YamlParseAndLogError(&event)) return 1;
    if (event.type != YAML_DOCUMENT_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file %s: file is not a YAML mappnig.\n",
            file);
        yaml_event_delete(&event);
        return 1;
    }
    yaml_event_delete(&event);
    if (this->YamlParseAndLogError(&event)) return 1;
    if (event.type != YAML_MAPPING_START_EVENT) {
        SINUCA3_ERROR_PRINTF(
            "while reading config file %s: file is not a YAML mappnig.\n",
            file);
        yaml_event_delete(&event);
        return 1;
    }

    // Write the location.
    *location = LocationFromMark(event.start_mark, file);

    yaml_event_delete(&event);
    return 0;
}

int yaml::Parser::ParseMapping(const YamlLocation location, const char* anchor,
                               YamlValue* ret) {
    if (anchor != NULL) {
        long anchorSize = strlen(anchor);
        const char* newAnchor = (const char*)this->arena.Alloc(anchorSize + 1);
        memcpy((void*)newAnchor, (const void*)anchor, anchorSize + 1);
        anchor = newAnchor;
    }

    *ret = yaml::YamlValue(yaml::YamlValueTypeMapping, location, anchor);
    ret->value.mapping =
        (Map<YamlValue>*)this->arena.Alloc(sizeof(*ret->value.mapping));
    *ret->value.mapping = Map<YamlValue>(&this->arena);

    yaml_event_t event;
    yaml_event_type_t eventType;
    do {
        if (this->YamlParseAndLogError(&event)) return 1;

        eventType = event.type;
        if (eventType == YAML_SCALAR_EVENT) {
            const char* key = (const char*)event.data.scalar.value;
            yaml::YamlValue value;
            if (this->ParseYamlValue(location.file, &value)) return 1;
            ret->value.mapping->Insert(key, value);
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

    return 0;
}

int yaml::Parser::ParseSequence(YamlLocation location, const char* anchor,
                                YamlValue* ret) {
    if (anchor != NULL) {
        long anchorSize = strlen(anchor);
        char* newAnchor = (char*)this->arena.Alloc(anchorSize + 1);
        memcpy((void*)newAnchor, (const void*)anchor, anchorSize + 1);
        anchor = newAnchor;
    }

    *ret = yaml::YamlValue(yaml::YamlValueTypeArray, location, anchor);

    yaml_event_t event;
    std::vector<yaml::YamlValue> array;

    // The loop breaks when event.type is YAML_SEQUENCE_EVENT.
    for (;;) {
        if (this->YamlParseAndLogError(&event)) return 1;

        // Exits by returning the array.
        if (event.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        yaml::YamlValue value;
        int result =
            this->ParseYamlValueFromEvent(&event, location.file, &value);
        yaml_event_delete(&event);
        if (result) return 1;
        array.push_back(value);
    }

    ret->value.array.size = array.size();
    ret->value.array.elements = (YamlValue*)this->arena.Alloc(
        ret->value.array.size * sizeof(YamlValue));
    memcpy(ret->value.array.elements, array.data(),
           array.size() * sizeof(*ret->value.array.elements));

    return 0;
}

int yaml::Parser::ParseYamlValueFromEvent(yaml_event_t* event, const char* file,
                                          YamlValue* ret) {
    long strSize;
    int result = 0;
    yaml::YamlLocation location = LocationFromMark(event->start_mark, file);
    switch (event->type) {
        case YAML_ALIAS_EVENT:
            *ret = yaml::YamlValue(yaml::YamlValueTypeAlias, location);
            strSize = strlen((const char*)event->data.alias.anchor);
            ret->value.alias = (const char*)this->arena.Alloc(strSize + 1);
            memcpy((void*)ret->value.alias,
                   (const void*)event->data.alias.anchor, strSize + 1);
            break;
        case YAML_SCALAR_EVENT:
            *ret = yaml::YamlValue(yaml::YamlValueTypeString, location,
                                   (const char*)event->data.scalar.anchor);
            strSize = strlen((char*)event->data.scalar.value);
            ret->value.string = (const char*)this->arena.Alloc(strSize + 1);
            memcpy((void*)ret->value.string, event->data.scalar.value,
                   strSize + 1);
            break;
        case YAML_MAPPING_START_EVENT:
            result = this->ParseMapping(
                location, (const char*)event->data.mapping_start.anchor, ret);
            break;
        case YAML_SEQUENCE_START_EVENT:
            result = this->ParseSequence(
                location, (const char*)event->data.mapping_start.anchor, ret);
            break;
        default:
            SINUCA3_DEBUG_PRINTF(
                "%s:%d: YamlValue parser got a strange event: %d\n",
                __FILE_NAME__, __LINE__, event->type);
            break;
    }

    return result;
}

int yaml::Parser::ParseYamlValue(const char* file, YamlValue* ret) {
    yaml_event_t event;
    if (this->YamlParseAndLogError(&event)) return 1;

    int result = this->ParseYamlValueFromEvent(&event, file, ret);
    yaml_event_delete(&event);

    return result;
}

int yaml::Parser::ParseFile(const char* configFile, YamlValue* const ret) {
    FILE* fp = fopen(configFile, "r");
    if (fp == NULL) {
        SINUCA3_ERROR_PRINTF("No such config file: %s.\n", configFile);
        return 1;
    }

    // This only fails on allocation failure, and we don't care about them lmao.
    yaml_parser_initialize(&this->parser);
    yaml_parser_set_input_file(&this->parser, fp);

    // We need to make sure the top level is a mapping because another thing
    // would make no sense.
    yaml::YamlLocation location;
    int result = 1;
    if (!this->EnsureFileIsYamlMapping(&location, configFile))
        result = this->ParseMapping(location, NULL, ret);

    yaml_parser_delete(&this->parser);

    fclose(fp);
    return result;
}

int yaml::Parser::ParseString(const char* const string, YamlValue* const ret) {
    yaml_parser_initialize(&this->parser);
    yaml_parser_set_input_string(&this->parser, (const unsigned char*)string,
                                 strlen(string));

    yaml::YamlLocation location;
    int result = 1;
    if (!this->EnsureFileIsYamlMapping(&location, "<input string>"))
        result = this->ParseMapping(location, NULL, ret);

    yaml_parser_delete(&this->parser);
    return result;
}

int yaml::Parser::IncludeString(Map<YamlValue>* config, const char* string) {
    Parser newParser;
    yaml::YamlValue newFileValue;
    if (newParser.ParseFile(string, &newFileValue)) return 1;

    assert(newFileValue.type == yaml::YamlValueTypeMapping);
    Map<YamlValue>* newValues = newFileValue.value.mapping;

    if (this->ProcessIncludeEntries(&newFileValue)) return 1;

    newValues->ResetIterator();
    YamlValue newValue;
    for (const char* key = newValues->Next(&newValue); key != NULL;
         key = newValues->Next(&newValue)) {
        config->Insert(key, newValue);
    }

    return 0;
}

int yaml::Parser::IncludeArray(Map<YamlValue>* config, YamlArray array,
                               YamlLocation location) {
    for (unsigned int i = 0; i < array.size; ++i) {
        const yaml::YamlValue* const value = &array.elements[i];
        if (value->type != yaml::YamlValueTypeString) {
            SINUCA3_ERROR_PRINTF(
                "%s:%lu:%lu: include array members "
                "should all be string. Got %s at %s:%lu:%lu.\n",
                location.file, location.line, location.column,
                value->TypeAsString(), value->location.file,
                value->location.line, value->location.column);
            return 1;
        }

        if (this->IncludeString(config, value->value.string)) return 1;
    }

    return 0;
}

int yaml::Parser::ProcessIncludeEntries(yaml::YamlValue* config) {
    assert(config->type == yaml::YamlValueTypeMapping);

    Map<YamlValue>* configMapping = config->value.mapping;
    YamlValue* entry = configMapping->Get("include");
    if (entry == NULL) return 0;

    switch (entry->type) {
        case yaml::YamlValueTypeString:
            if (this->IncludeString(configMapping, entry->value.string)) {
                SINUCA3_ERROR_PRINTF("%s:%lu:%lu.\n", config->location.file,
                                     config->location.line,
                                     config->location.column);
                return 1;
            }
            break;
        case yaml::YamlValueTypeArray:
            if (this->IncludeArray(configMapping, entry->value.array,
                                   config->location)) {
                SINUCA3_ERROR_PRINTF("%s:%lu:%lu.\n", config->location.file,
                                     config->location.line,
                                     config->location.column);
                return 1;
            }
            break;
        default:
            SINUCA3_ERROR_PRINTF(
                "%s:%lu:%lu: include should "
                "be a string or an array of strings.\n",
                config->location.file, config->location.line,
                config->location.column);
            return 1;
    }

    return 0;
}

int yaml::Parser::ParseFileWithIncludes(const char* configFile,
                                        YamlValue* ret) {
    if (this->ParseFile(configFile, ret)) return 1;
    if (this->ProcessIncludeEntries(ret)) return 1;

    return 0;
}
