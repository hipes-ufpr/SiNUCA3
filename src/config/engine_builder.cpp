#include "engine_builder.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "../utils/logging.hpp"
#include "config.hpp"
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
    if (name == NULL) return NULL;

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

sinuca::builder::ComponentDefinition*
sinuca::config::EngineBuilder::GetComponentDefinitionOrMakeDummy(
    const char* name) {
    builder::ComponentDefinition* definition =
        this->GetComponentDefinition(name);
    if (definition == NULL) definition = this->AddDummyDefinition(name);
    return definition;
}

sinuca::builder::ComponentInstantiation*
sinuca::config::EngineBuilder::GetComponentInstantiationOrMakeDummy(
    const char* alias) {
    builder::ComponentInstantiation* instance =
        this->GetComponentInstantiation(alias);
    if (instance == NULL) instance = this->AddDummyInstance(alias);
    return instance;
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
                                                            dest->type = builder::ParameterTypeDefinitionReference;
            if (dest->value.referenceToDefinition == NULL) return 1;
            break;
        case yaml::YamlValueTypeString:
            dest->value.referenceToDefinition =
                this->GetComponentDefinitionOrMakeDummy(src->value.string);
                dest->type = builder::ParameterTypeDefinitionReference;
            break;
        case yaml::YamlValueTypeAlias:
            dest->value.referenceToInstance =
                this->GetComponentInstantiationOrMakeDummy(src->value.alias);
                dest->type = builder::ParameterTypeInstanceReference;
            break;
    }

    return 0;
}

static inline int AddClass(sinuca::builder::ComponentDefinition* definition,
                           const sinuca::yaml::YamlValue* value) {
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

int sinuca::config::EngineBuilder::FillParametersAndClass(
    builder::ComponentDefinition* definition,
    const std::vector<yaml::YamlMappingEntry*>* config) {
    SINUCA3_DEBUG_PRINTF("    Filling parameters and class for definition.\n");

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
            if (AddClass(definition, (*config)[i]->value)) return 1;

            continue;
        }

        // If we're on the last item and class was not provided, appending would
        // overflow, but we know that class was not provided!
        if (definition->clazz == NULL) {
            SINUCA3_ERROR_PRINTF(
                "While trying to define component %s: parameter `class` was "
                "not provided.\n",
                definition->name);
            return 1;
        }

        // Finally add the parameter.
        builder::ParameterMapItem* parameter =
            &definition->parameters.items[itemsIndex];
        parameter->name = (*config)[i]->name;
        if (this->Yaml2Parameter(parameter->name, &parameter->value,
                                 (*config)[i]->value))
            return 1;

        ++itemsIndex;
    }

    return 0;
}

sinuca::builder::ComponentDefinition*
sinuca::config::EngineBuilder::AddComponentDefinitionFromYamlMapping(
    const char* name, const std::vector<yaml::YamlMappingEntry*>* config) {
    builder::ComponentDefinition* addr;
    bool mustPop = false;

    addr = this->GetComponentDefinition(name);
    if (addr == NULL) {
        builder::ComponentDefinition definition =
            builder::ComponentDefinition(name, true);
        this->componentDefinitions.push_back(definition);

        addr =
            &this->componentDefinitions[this->componentDefinitions.size() - 1];
        mustPop = true;
    } else if (addr->alreadyDefined) {
        SINUCA3_ERROR_PRINTF("Multiple definitions of component %s.", name);
        return NULL;
    }

    if (this->FillParametersAndClass(addr, config)) {
        if (mustPop) this->componentDefinitions.pop_back();
        return NULL;
    }

    return addr;
}

sinuca::builder::ComponentInstantiation*
sinuca::config::EngineBuilder::AddComponentInstantiationFromYamlMapping(
    const char* name, const char* alias,
    const std::vector<yaml::YamlMappingEntry*>* config) {
    builder::ComponentDefinition* definition =
        this->AddComponentDefinitionFromYamlMapping(name, config);
    if (definition == NULL) return NULL;

    builder::ComponentInstantiation* instancePtr =
        this->GetComponentInstantiation(alias);
    if (instancePtr != NULL) {
        if (instancePtr->alreadyDefined) {
            SINUCA3_ERROR_PRINTF("Multiple components with alias %s.", alias);
            return NULL;
        }
        instancePtr->alreadyDefined = true;
        instancePtr->definition = definition;
        return instancePtr;
    }

    builder::ComponentInstantiation instance =
        builder::ComponentInstantiation(alias, definition, true);

    this->components.push_back(instance);
    return &this->components[this->components.size() - 1];
}

int sinuca::config::EngineBuilder::AddCores(
    const std::vector<sinuca::yaml::YamlValue*>* coresConfig) {
    for (long i = 0; i < this->numberOfCores; ++i) {
        const sinuca::yaml::YamlValue* entry = (*coresConfig)[i];
        builder::ComponentDefinition* definition = NULL;

        switch (entry->type) {
            case sinuca::yaml::YamlValueTypeMapping:
                definition = this->AddComponentDefinitionFromYamlMapping(
                    NULL, entry->value.mapping);
                break;
            case sinuca::yaml::YamlValueTypeString:
                definition = this->GetComponentDefinitionOrMakeDummy(
                    entry->value.string);
                break;
            default:
                SINUCA3_ERROR_PRINTF(
                    "Core configuration must be a component definition. Got "
                    "%s.\n",
                    entry->TypeAsString());
                return 1;
        }
        if (definition == NULL) {
            SINUCA3_ERROR_PRINTF("Inside of the definition for the %ld core.\n",
                                 i);
            return 1;
        }
        this->cores[i] = definition;
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

    this->cores = new sinuca::builder::ComponentDefinition*[array->size()];
    this->numberOfCores = array->size();

    return 0;
}

int sinuca::config::EngineBuilder::TreatParameter(
    const char* name, const yaml::YamlValue* value) {
    SINUCA3_DEBUG_PRINTF("Treating parameter %s.\n", name);

    // Treat the `cores` parameter. May return error.
    if (strcmp(name, "cores") == 0) {
        if (this->TreatCoresParameter(value)) return 1;

        SINUCA3_DEBUG_PRINTF("Successfully treated parameter cores.\n");

        // Now we add each core.
        const std::vector<sinuca::yaml::YamlValue*>* coresConfig =
            value->value.array;
        if (this->AddCores(coresConfig)) return 1;

        SINUCA3_DEBUG_PRINTF("Successfully added each core.\n");

        return 0;
    }

    // If it's not the `cores` special parameter, it's a component
    // definition.
    SINUCA3_DEBUG_PRINTF("Parameter is a component definition.\n");
    if (value->type != yaml::YamlValueTypeMapping) {
        SINUCA3_ERROR_PRINTF(
            "While trying to define component %s: expected a YAML Mapping, "
            "got %s.\n",
            name, value->TypeAsString());
        return 1;
    }

    SINUCA3_DEBUG_PRINTF("  With anchor %s.\n", value->anchor);
    if (value->anchor != NULL) {
        if (this->AddComponentInstantiationFromYamlMapping(
                name, value->anchor, value->value.mapping) == NULL)
            return 1;

        SINUCA3_DEBUG_PRINTF("  Successfully added component instantiation.\n");
        return 0;
    }

    if (this->AddComponentDefinitionFromYamlMapping(
            name, value->value.mapping) == NULL)
        return 1;

    SINUCA3_DEBUG_PRINTF("  Successfully added component definition.\n");
    return 0;
}

int sinuca::config::EngineBuilder::EnsureAllComponentsAreDefined() {
    for (unsigned long i = 0; i < this->components.size(); ++i) {
        if (!components[i].alreadyDefined) {
            SINUCA3_ERROR_PRINTF("Component with alias %s was never defined.",
                                 components[i].alias);
            return 1;
        }
    }

    for (unsigned long i = 0; i < this->componentDefinitions.size(); ++i) {
        if (!componentDefinitions[i].alreadyDefined) {
            SINUCA3_ERROR_PRINTF("Component definition %s was never defined.",
                                 componentDefinitions[i].name);
            return 1;
        }
    }

    return 0;
}

static inline sinuca::engine::Linkable* CreateComponent(const char* clazz) {
    sinuca::engine::Linkable* component =
        sinuca::CreateDefaultComponentByClass(clazz);
    if (component != NULL) return component;

    component = sinuca::CreateCustomComponentByClass(clazz);
    return component;
}

sinuca::engine::Engine*
sinuca::config::EngineBuilder::FreeSelfOnInstantiationFailure(
    const sinuca::yaml::YamlValue* yamlConfig) {
    delete yamlConfig;

    // Go back deallocating everything. We need to do this because
    // ComponentInstantiation doesn't delete it's own component pointer.
    for (unsigned long j = 0;
         j < this->components.size() && this->components[j].component != NULL;
         ++j)
        delete this->components[j].component;
    // The definitions array doesn't need one of this.

    return NULL;
}

int sinuca::config::EngineBuilder::ArrayParameter2ConfigValue(
    const builder::ParameterArray* parameter, ConfigValue* value) {
    std::vector<ConfigValue>* array = new std::vector<ConfigValue>;
    array->reserve(parameter->size);
    for (long i = 0; i < parameter->size; ++i) {
        ConfigValue itemValue;
        if (this->Parameter2ConfigValue(&parameter->parameters[i],
                                        &itemValue)) {
            delete array;
            return 1;
        }
        array->push_back(itemValue);
    }

    *value = ConfigValue(array);

    return 0;
}

sinuca::engine::Linkable*
sinuca::config::EngineBuilder::NewComponentFromDefinitionReference(
    builder::ComponentDefinition* reference) {
    engine::Linkable* component = CreateComponent(reference->clazz);
    if (component == NULL) return NULL;

    builder::ComponentInstantiation instance =
        builder::ComponentInstantiation(NULL, reference, true);
    instance.component = component;

    this->components.push_back(instance);

    return component;
}

int sinuca::config::EngineBuilder::Parameter2ConfigValue(
    const builder::Parameter* parameter, ConfigValue* value) {
    switch (parameter->type) {
        case builder::ParameterTypeInteger:
            *value = ConfigValue(parameter->value.integer);
            break;
        case builder::ParameterTypeNumber:
            *value = ConfigValue(parameter->value.number);
            break;
        case builder::ParameterTypeBoolean:
            *value = ConfigValue(parameter->value.boolean);
            break;
        case builder::ParameterTypeInstanceReference:
            *value =
                ConfigValue(parameter->value.referenceToInstance->component);
            break;
        case builder::ParameterTypeArray:
            return this->ArrayParameter2ConfigValue(&parameter->value.array,
                                                    value);
            break;
        case builder::ParameterTypeDefinitionReference:
            // TODO.
            *value = ConfigValue(this->NewComponentFromDefinitionReference(
                parameter->value.referenceToDefinition));
            if (value->value.componentReference == NULL) return 1;
            break;
        default:
            assert(0 && "unreachable");
    }

    return 0;
}

int sinuca::config::EngineBuilder::SetupComponentConfig(
    const sinuca::builder::ComponentInstantiation* instance) {
    sinuca::engine::Linkable* component = instance->component;
    builder::ParameterMap* map = &instance->definition->parameters;

    for (long i = 0; i < map->size; ++i) {
        const char* parameterName = map->items[i].name;
        const builder::Parameter* parameterValue = &map->items[i].value;
        ConfigValue value;
        if (this->Parameter2ConfigValue(parameterValue, &value)) return 1;
        if (component->SetConfigParameter(parameterName, value)) return 1;
    }

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
        if (this->TreatParameter((*config)[i]->name, (*config)[i]->value))
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
    }

    // Error.
    if (this->EnsureAllComponentsAreDefined())
        return this->FreeSelfOnInstantiationFailure(yamlConfig);

    // We instantiate all components first so the pointers to aliased stuff will
    // always work. We only have to solve definitions references now.
    for (unsigned long i = 0; i < this->components.size(); ++i) {
        builder::ComponentInstantiation* component = &this->components[i];
        component->component = CreateComponent(component->definition->clazz);

        // No such class.
        if (component->component == NULL) {
            SINUCA3_ERROR_PRINTF("No such component class: %s.",
                                 component->definition->clazz);
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
        }
    }

    // Pass the parameters to the components. This also resolves pointers to
    // definitions, thus allocating more components. As we already ensured all
    // definitions are satisfied, this cannot fail.
    for (unsigned long i = 0; i < this->components.size(); ++i) {
        if (this->SetupComponentConfig(&this->components[i]))
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
    }

    // Now we do the same for the cores.
    std::vector<builder::ComponentInstantiation> coresInstantiations;
    coresInstantiations.reserve(this->numberOfCores);
    for (long i = 0; i < this->numberOfCores; ++i) {
        coresInstantiations.push_back(
            builder::ComponentInstantiation(NULL, this->cores[i], true));
        if (this->SetupComponentConfig(&coresInstantiations[i]))
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
    }
    for (unsigned long i = 0; i < coresInstantiations.size(); ++i) {
        coresInstantiations[i].component =
            CreateComponent(coresInstantiations[i].definition->clazz);

        // No such class.
        if (coresInstantiations[i].component == NULL) {
            SINUCA3_ERROR_PRINTF("No such component class: %s.",
                                 coresInstantiations[i].definition->clazz);
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
        }
    }

    engine::Engine* engine = this->BuildEngine(&coresInstantiations);
    delete yamlConfig;
    return engine;
}

sinuca::engine::Engine* sinuca::config::EngineBuilder::BuildEngine(
    const std::vector<builder::ComponentInstantiation>* coresInstances) {
    long numberOfComponents = this->components.size();
    engine::Linkable** components = new engine::Linkable*[numberOfComponents];
    engine::Linkable** cores = new engine::Linkable*[this->numberOfCores];

    assert(components != NULL);
    assert(cores != NULL);

    for (long i = 0; i < numberOfComponents; ++i)
        components[i] = this->components[i].component;
    for (long i = 0; i < this->numberOfCores; ++i)
        cores[i] = (*coresInstances)[i].component;

    engine::Engine* engine = new engine::Engine(components, numberOfComponents);
    if (engine->AddCPUs(cores, this->numberOfCores)) {
        delete[] cores;
        delete engine;
        return NULL;
    }

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
