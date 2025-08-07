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

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../utils/logging.hpp"

// This functions are declared up here because they're codependencies with other
// parsing functions.
static inline yaml::YamlValue* ParseYamlValue(yaml_parser_t* parser);
static inline yaml::YamlValue* ParseYamlValueFromEvent(yaml_parser_t* parser,
                                                       yaml_event_t* event);

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

static inline yaml::YamlMappingEntry* ParseMappingEntry(yaml_parser_t* parser,
                                                        const char* name) {
    yaml::YamlValue* value = ParseYamlValue(parser);
    if (value == NULL) return NULL;

    long sizeOfName = strlen(name);
    char* newStrName = new char[sizeOfName + 1];
    memcpy(newStrName, name, sizeOfName + 1);
    return new yaml::YamlMappingEntry(newStrName, value);
}

static inline yaml::YamlValue* ParseMapping(yaml_parser_t* parser,
                                            const char* anchor) {
    if (anchor != NULL) {
        long anchorSize = strlen(anchor);
        char* newAnchor = new char[anchorSize + 1];
        memcpy((void*)newAnchor, (const void*)anchor, anchorSize + 1);
        anchor = newAnchor;
    }

    yaml::YamlValue* mapping =
        new yaml::YamlValue(yaml::YamlValueTypeMapping, anchor);

    yaml_event_t event;
    yaml_event_type_t eventType;
    do {
        if (YamlParseAndLogError(parser, &event)) {
            delete mapping;
            return NULL;
        }

        eventType = event.type;
        if (eventType == YAML_SCALAR_EVENT) {
            yaml::YamlMappingEntry* entry =
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

static inline yaml::YamlValue* ParseSequence(yaml_parser_t* parser,
                                             const char* anchor) {
    if (anchor != NULL) {
        long anchorSize = strlen(anchor);
        char* newAnchor = new char[anchorSize + 1];
        memcpy((void*)newAnchor, (const void*)anchor, anchorSize + 1);
        anchor = newAnchor;
    }

    yaml::YamlValue* array =
        new yaml::YamlValue(yaml::YamlValueTypeArray, anchor);

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

        yaml::YamlValue* entry = ParseYamlValueFromEvent(parser, &event);
        yaml_event_delete(&event);
        if (entry == NULL) {
            delete array;
            return NULL;
        }
        array->value.array->push_back(entry);
    }
}

static inline yaml::YamlValue* ParseScalar(const char* scalar, long scalarSize,
                                           const char* anchor) {
    if (anchor != NULL) {
        long anchorSize = strlen(anchor);
        char* newAnchor = new char[anchorSize + 1];
        memcpy((void*)newAnchor, (const void*)anchor, anchorSize + 1);
        anchor = newAnchor;
    }

    double number;
    if (sscanf(scalar, "%lf", &number) == 1)
        return new yaml::YamlValue(number, anchor);
    if (!strcmp(scalar, "true") || !strcmp(scalar, "yes"))
        return new yaml::YamlValue(true, anchor);
    if (!strcmp(scalar, "false") || !strcmp(scalar, "no"))
        return new yaml::YamlValue(false, anchor);

    yaml::YamlValue* value =
        new yaml::YamlValue(yaml::YamlValueTypeString, anchor);
    value->value.string = new char[scalarSize + 1];
    memcpy((void*)value->value.string, (const void*)scalar, scalarSize + 1);
    return value;
}

static inline yaml::YamlValue* ParseYamlValueFromEvent(yaml_parser_t* parser,
                                                       yaml_event_t* event) {
    yaml::YamlValue* value = NULL;
    long strSize;
    switch (event->type) {
        case YAML_ALIAS_EVENT:
            value = new yaml::YamlValue(yaml::YamlValueTypeAlias);
            strSize = strlen((const char*)event->data.alias.anchor);
            value->value.alias = new char[strSize + 1];
            memcpy((void*)value->value.alias,
                   (const void*)event->data.alias.anchor, strSize + 1);
            break;
        case YAML_SCALAR_EVENT:
            value = ParseScalar((const char*)event->data.scalar.value,
                                event->data.scalar.length,
                                (const char*)event->data.scalar.anchor);
            break;
        case YAML_MAPPING_START_EVENT:
            value = ParseMapping(parser,
                                 (const char*)event->data.mapping_start.anchor);
            break;
        case YAML_SEQUENCE_START_EVENT:
            value = ParseSequence(
                parser, (const char*)event->data.sequence_start.anchor);
            break;
        default:
            SINUCA3_DEBUG_PRINTF(
                "%s:%d: YamlValue parser got a strange event: %d\n",
                __FILE_NAME__, __LINE__, event->type);
            break;
    }

    return value;
}

static inline yaml::YamlValue* ParseYamlValue(yaml_parser_t* parser) {
    yaml_event_t event;
    if (YamlParseAndLogError(parser, &event)) return NULL;

    yaml::YamlValue* value = ParseYamlValueFromEvent(parser, &event);
    yaml_event_delete(&event);
    return value;
}

yaml::YamlValue* yaml::ParseFile(const char* configFile) {
    FILE* fp = fopen(configFile, "r");
    if (fp == NULL) {
        SINUCA3_ERROR_PRINTF("No such config file: %s.\n", configFile);
        return NULL;
    }

    // This only fails on allocation failure, and we don't care about them lmao.
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    // We need to make sure the top level is a mapping because another thing
    // would make no sense.
    yaml::YamlValue* yaml = NULL;
    if (!EnsureFileIsYamlMapping(&parser)) yaml = ParseMapping(&parser, NULL);

    yaml_parser_delete(&parser);

    fclose(fp);
    return yaml;
}

// Pre-declaration for IncludeString as they're mutually recursive.
int ProcessIncludeEntries(yaml::YamlValue* config, const char* configFile);

int IncludeString(std::vector<yaml::YamlMappingEntry*>* config,
                  const char* string) {
    yaml::YamlValue* newFileValue = yaml::ParseFile(string);
    if (newFileValue == NULL) return 1;

    assert(newFileValue->type == yaml::YamlValueTypeMapping);
    std::vector<yaml::YamlMappingEntry*>* newValues =
        newFileValue->value.mapping;

    if (ProcessIncludeEntries(newFileValue, string)) {
        delete newFileValue;
        return 1;
    }

    config->reserve(config->size() + newValues->size());
    for (unsigned int i = 0; i < newValues->size(); ++i) {
        config->push_back((*newValues)[i]);
    }

    // We should call clear before destroying the value so it does not destroy
    // the values we copied to the other vector.
    newValues->clear();
    delete newFileValue;

    return 0;
}

int IncludeArray(std::vector<yaml::YamlMappingEntry*>* config,
                 std::vector<yaml::YamlValue*>* array, const char* configFile) {
    for (unsigned int i = 0; i < array->size(); ++i) {
        if ((*array)[i]->type != yaml::YamlValueTypeString) {
            SINUCA3_ERROR_PRINTF(
                "while reading configuration file %s: include array members "
                "should all be string.",
                configFile);
            return 1;
        }

        if (IncludeString(config, (*array)[i]->value.string)) return 1;
    }

    return 0;
}

int ProcessIncludeEntries(yaml::YamlValue* config, const char* configFile) {
    assert(config->type == yaml::YamlValueTypeMapping);

    std::vector<yaml::YamlMappingEntry*>* configMapping = config->value.mapping;

    // We got two iterators because we must iterate every original entry, but we
    // remove include entries while iterating.
    unsigned int origSize = configMapping->size();
    unsigned int index = 0;
    for (unsigned int i = 0; i < origSize; ++i) {
        if (strcmp((*configMapping)[index]->name, "include") == 0) {
            yaml::YamlMappingEntry* entry = (*configMapping)[index];
            switch (entry->value->type) {
                case yaml::YamlValueTypeString:
                    if (IncludeString(configMapping,
                                      entry->value->value.string)) {
                        SINUCA3_ERROR_PRINTF(
                            "while reading configuration file %s.\n",
                            configFile);
                        return 1;
                    }
                    break;
                case yaml::YamlValueTypeArray:
                    if (IncludeArray(configMapping, entry->value->value.array,
                                     configFile)) {
                        SINUCA3_ERROR_PRINTF(
                            "while reading configuration file %s.\n",
                            configFile);
                        return 1;
                    }
                    break;
                default:
                    SINUCA3_ERROR_PRINTF(
                        "while reading configuration file %s: include should "
                        "be a string or an array of strings.\n",
                        configFile);
                    return 1;
            }

            delete (*configMapping)[index];
            configMapping->erase(configMapping->begin() + index);
        } else {
            ++index;
        }
    }

    return 0;
}

yaml::YamlValue* yaml::ParseFileWithIncludes(const char* configFile) {
    yaml::YamlValue* config = ParseFile(configFile);
    if (config == NULL) return NULL;

    if (ProcessIncludeEntries(config, configFile)) {
        delete config;
        return NULL;
    }

    return config;
}

#ifndef NDEBUG

void yaml::PrintYaml(yaml::YamlValue* value) {
    switch (value->type) {
        case yaml::YamlValueTypeBoolean:
            printf("%s\n", value->value.boolean ? "true" : "false");
            break;
        case yaml::YamlValueTypeNumber:
            printf("%f\n", value->value.number);
            break;
        case yaml::YamlValueTypeString:
            printf("%s\n", value->value.string);
            break;
        case yaml::YamlValueTypeAlias:
            printf("*%s\n", value->value.alias);
            break;
        case yaml::YamlValueTypeArray:
            printf("[\n");
            for (unsigned int i = 0; i < value->value.array->size(); ++i)
                yaml::PrintYaml((*value->value.array)[i]);
            printf("]\n");
            break;
        case yaml::YamlValueTypeMapping:
            printf("{\n");
            for (unsigned int i = 0; i < value->value.mapping->size(); ++i) {
                printf("%s: ", ((*value->value.mapping)[i])->name);
                yaml::PrintYaml(((*value->value.mapping)[i])->value);
            }
            printf("}\n");
            break;
    }
}

#endif  // NDEBUG
