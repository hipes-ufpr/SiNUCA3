#include "engine_builder.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "../utils/logging.hpp"
#include "yaml_parser.hpp"

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
 * @file engine_builder.cpp
 * @brief Takes a config file path and instantiates the simulation engine.
 * @details "I wrote this in a very C-like way instead of actually using the
 * class definition. But I bet it's faster this way." - Gabriel
 * Update: I decided to rewrite it in a proper software engineering fashion.
 */

sinuca::builder::ComponentDefinition*
sinuca::config::EngineBuilder::AddDummyDefinition(const char* name) {
    builder::ComponentDefinition definition =
        builder::ComponentDefinition(name, false);
    this->componentDefinitions.push_back(definition);
    return &this->componentDefinitions[this->componentDefinitions.size() - 1];
}

sinuca::builder::ComponentInstantiation*
sinuca::config::EngineBuilder::AddDummyInstance(const char* alias) {
    builder::ComponentInstantiation instance =
        builder::ComponentInstantiation(alias, NULL, false);
    this->components.push_back(instance);
    return &this->components[this->components.size() - 1];
}

sinuca::builder::ComponentDefinition*
sinuca::config::EngineBuilder::GetComponentDefinition(const char* name) {
    for (unsigned int i = 0; i < this->componentDefinitions.size(); ++i) {
        if (strcmp(this->componentDefinitions[i].name, name) == 0)
            return &this->componentDefinitions[i];
    }

    return NULL;
}

sinuca::builder::ComponentInstantiation*
sinuca::config::EngineBuilder::GetComponentInstantiation(const char* alias) {
    for (unsigned int i = 0; i < this->components.size(); ++i) {
        if (strcmp(this->components[i].alias, alias) == 0)
            return &this->components[i];
    }

    return NULL;
}

static inline void YamlNumber2Parameter(sinuca::builder::Parameter* dest,
                                        double number) {
    long integer = trunc(number);
    if (((double)integer) == number) {
        dest->value.integer = integer;
        dest->type = sinuca::builder::ParameterTypeInteger;
    } else {
        dest->value.number = number;
        dest->type = sinuca::builder::ParameterTypeNumber;
    }
}

int sinuca::config::EngineBuilder::YamlArray2Parameter(
    builder::Parameter* dest, const std::vector<yaml::YamlValue*>* array) {
    dest->value.array.parameters = new builder::Parameter[array->size()];

    for (unsigned long i = 0; i < array->size(); ++i) {
        if (this->Yaml2Parameter(NULL, &dest->value.array.parameters[i],
                                 (*array)[i]))
            return 1;
    }

    return 0;
}

int sinuca::config::EngineBuilder::Yaml2Parameter(const char* name,
                                                  builder::Parameter* dest,
                                                  const yaml::YamlValue* src) {
    switch (src->type) {
        case yaml::YamlValueTypeBoolean:
            dest->type = builder::ParameterTypeBoolean;
            dest->value.boolean = src->value.boolean;
            break;
        case yaml::YamlValueTypeNumber:
            YamlNumber2Parameter(dest, src->value.number);
            break;
        case yaml::YamlValueTypeArray:
            YamlArray2Parameter(dest, src->value.array);
            break;
        case yaml::YamlValueTypeMapping:
            dest->value.referenceToDefinition =
                this->AddComponentDefinitionFromYamlMapping(name,
                                                            src->value.mapping);
            if (dest->value.referenceToDefinition == NULL) return 1;
            break;
        case yaml::YamlValueTypeString:
            dest->value.referenceToDefinition =
                this->GetComponentDefinition(src->value.string);
            if (dest->value.referenceToDefinition == NULL)
                dest->value.referenceToDefinition =
                    this->AddDummyDefinition(src->value.string);
            break;
        case yaml::YamlValueTypeAlias:
            dest->value.referenceToInstance =
                this->GetComponentInstantiation(src->value.alias);
            if (dest->value.referenceToInstance == NULL)
                dest->value.referenceToInstance =
                    this->AddDummyInstance(src->value.alias);
            break;
    }

    return 0;
}

static inline int AddClass(sinuca::builder::ComponentDefinition* definition,
                           sinuca::yaml::YamlValue* value) {
    if (definition->clazz != NULL) {
        SINUCA3_ERROR_PRINTF(
            "While trying to define component %s: parameter `class` "
            "defined multiple times.\n",
            definition->name);
        return 1;
    }

    if (value->type != sinuca::yaml::YamlValueTypeString) {
        SINUCA3_ERROR_PRINTF(
            "While trying to define component %s: parameter `class` is "
            "not a string.\n",
            definition->name);
        return 1;
    }

    definition->clazz = value->value.string;

    return 0;
}

int sinuca::config::EngineBuilder::FillParametersAndComponent(
    builder::ComponentDefinition* definition,
    const std::vector<yaml::YamlMappingEntry*>* config) {
    definition->clazz = NULL;

    if (config->size() == 0 || strcmp((*config)[0]->name, "class") != 0) {
        SINUCA3_ERROR_PRINTF(
            "While trying to define component %s: parameter `class` not "
            "provided.\n",
            definition->name);
        return 1;
    }

    // It's -1 because we don't append the class.
    definition->parameters.size = config->size() - 1;
    definition->parameters.items =
        new builder::ParameterMapItem[definition->parameters.size];

    unsigned long itemsIndex = 0;
    for (unsigned long i = 0; i < config->size(); ++i) {
        // Get the class.
        if (strcmp((*config)[i]->name, "class") == 0) {
            if (AddClass(definition, (*config)[i]->value)) {
                delete definition->parameters.items;
                return 1;
            }

            continue;
        }

        // If we're on the last item and class was not provided, appending would
        // overflow, but we know that class was not provided!
        if (definition->clazz == NULL) {
            SINUCA3_ERROR_PRINTF(
                "While trying to define component %s: parameter `class` was "
                "not provided.\n",
                definition->name);
            delete definition->parameters.items;
            return 1;
        }

        // Finally add the parameter.
        builder::ParameterMapItem* parameter =
            &definition->parameters.items[itemsIndex];
        parameter->name = (*config)[i]->name;
        if (this->Yaml2Parameter(parameter->name, &parameter->value,
                                 (*config)[i]->value)) {
            delete definition->parameters.items;
            return 1;
        }

        ++itemsIndex;
    }

    return 0;
}

sinuca::builder::ComponentDefinition*
sinuca::config::EngineBuilder::AddComponentDefinitionFromYamlMapping(
    const char* name, const std::vector<yaml::YamlMappingEntry*>* config) {
    builder::ComponentDefinition definition =
        builder::ComponentDefinition(name, true);

    if (this->FillParametersAndComponent(&definition, config)) {
        this->componentDefinitions.pop_back();
        return NULL;
    }

    this->componentDefinitions.push_back(definition);

    return &this->componentDefinitions[this->componentDefinitions.size() - 1];
}

sinuca::builder::ComponentInstantiation*
sinuca::config::EngineBuilder::AddComponentInstantiationFromYamlMapping(
    const char* name, const char* alias,
    const std::vector<yaml::YamlMappingEntry*>* config) {
    builder::ComponentDefinition* definition =
        this->AddComponentDefinitionFromYamlMapping(name, config);
    if (definition == NULL) return NULL;

    builder::ComponentInstantiation instance =
        builder::ComponentInstantiation(alias, definition, true);

    this->components.push_back(instance);
    return &this->components[this->components.size() - 1];
}

int sinuca::config::EngineBuilder::AddCores(
    const std::vector<sinuca::yaml::YamlValue*>* coresConfig) {
    for (long i = 0; i < this->numberOfCores; ++i) {
        builder::ComponentInstantiation* instance =
            this->AddComponentInstantiationFromYamlMapping(
                NULL, NULL, (*coresConfig)[i]->value.mapping);
        if (instance == NULL) {
            SINUCA3_ERROR_PRINTF("Inside of the definition for the %ld core.\n",
                                 i);
            return 1;
        }
        this->cores[i] = instance;
    }

    return 0;
}

int sinuca::config::EngineBuilder::TreatCoresParameter(
    const sinuca::yaml::YamlValue* coresConfig) {
    if (this->cores != NULL) {
        SINUCA3_ERROR_PRINTF("Parameter `cores` was defined multiple times.\n");
        return 1;
    }
    if (coresConfig->type != sinuca::yaml::YamlValueTypeArray) {
        SINUCA3_ERROR_PRINTF(
            "Parameter `cores` must be an array of components. Got %s.\n",
            coresConfig->TypeAsString());
        return 1;
    }

    const std::vector<sinuca::yaml::YamlValue*>* array =
        coresConfig->value.array;
    if (array->empty()) {
        SINUCA3_ERROR_PRINTF(
            "Parameter `cores` must be an non-empty array of components.\n");
        return 1;
    }

    this->cores = new sinuca::builder::ComponentInstantiation*[array->size()];
    this->numberOfCores = array->size();

    return 0;
}

sinuca::engine::Engine* sinuca::config::EngineBuilder::Instantiate(
    const char* configFile) {
    const sinuca::yaml::YamlValue* yamlConfig =
        sinuca::yaml::ParseFile(configFile);
    if (yamlConfig == NULL) return NULL;
    assert(yamlConfig->type == sinuca::yaml::YamlValueTypeMapping);

    // We only need this to instantiate everything.
    const std::vector<sinuca::yaml::YamlMappingEntry*>* config =
        yamlConfig->value.mapping;

    for (unsigned int i = 0; i < config->size(); ++i) {
        const char* name = (*config)[i]->name;
        const yaml::YamlValue* value = (*config)[i]->value;

        // Treat the `cores` parameter. May return error.
        if (strcmp(name, "cores") == 0) {
            if (this->TreatCoresParameter(value)) {
                delete yamlConfig;
                return NULL;
            }

            // Now we add each core.
            const std::vector<sinuca::yaml::YamlValue*>* coresConfig =
                value->value.array;
            if (this->AddCores(coresConfig)) {
                delete yamlConfig;
                return NULL;
            }

            continue;
        }

        // If it's not the `cores` special parameter, it's a component
        // definition.
        if (value->type != yaml::YamlValueTypeMapping) {
            SINUCA3_ERROR_PRINTF(
                "While trying to define component %s: expected a YAML Mapping, "
                "got %s.\n",
                name, value->TypeAsString());
            delete yamlConfig;
            return NULL;
        }
        if (value->anchor != NULL)
            this->AddComponentInstantiationFromYamlMapping(
                name, value->anchor, value->value.mapping);
        else
            this->AddComponentDefinitionFromYamlMapping(name,
                                                        value->value.mapping);
    }

    sinuca::engine::Engine* engine = new sinuca::engine::Engine;

    return engine;
}

sinuca::config::EngineBuilder::EngineBuilder() {
    this->cores = NULL;
    this->numberOfCores = 0;
    // Heuristic.
    this->componentDefinitions.reserve(32);
    this->components.reserve(32);
}

sinuca::config::EngineBuilder::~EngineBuilder() {
    if (this->cores != NULL) delete[] this->cores;
}
