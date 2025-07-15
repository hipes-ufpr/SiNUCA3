#include "engine_builder.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "../sinuca3.hpp"
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
 *
 * This is very complex. It gets the Yaml tree from yaml_parser.hpp and
 * traverses it:
 * - When it encounters a new mapping, it creates a new, complete component
 *   definition (also creating a complete component instantiation if the mapping
 *   has any alias). Both may already exist as a "dummy". In this case, it just
 *   finishes filling the information there;
 * - When it encounters the instantiate parameter, it treats it like a reference
 *   to definition parameter;
 * - Regarding parameters: when it encounters a string, it searches for the
 *   definition. If there's none, it creates a dummy. When it encounters an
 *   alias, it does the same with instances;
 * - When all of the tree is traversed, it checks if there's no dummy
 *   definition. This would mean that a definition was referenced but not
 *   defined. Same for instances;
 * - Then, it traverses the configuration parameters, creating new instances in
 *   the way as it encounters definition references. In this stage, it calls
 *   SetConfigParameter;
 */

sinuca::builder::DefinitionID sinuca::config::EngineBuilder::AddDummyDefinition(
    const char* name) {
    builder::ComponentDefinition definition =
        builder::ComponentDefinition(name, false);
    this->componentDefinitions.push_back(definition);
    return this->componentDefinitions.size() - 1;
}

sinuca::builder::InstanceID sinuca::config::EngineBuilder::AddDummyInstance(
    const char* alias) {
    builder::ComponentInstantiation instance =
        builder::ComponentInstantiation(alias, 0, false);
    this->components.push_back(instance);
    return this->components.size() - 1;
}

int sinuca::config::EngineBuilder::GetComponentDefinition(
    const char* name, builder::DefinitionID* ret) {
    if (name == NULL) return 1;

    for (unsigned int i = 0; i < this->componentDefinitions.size(); ++i) {
        if (this->componentDefinitions[i].name != NULL &&
            (strcmp(this->componentDefinitions[i].name, name) == 0)) {
            *ret = i;
            return 0;
        }
    }

    return 1;
}

int sinuca::config::EngineBuilder::GetComponentInstantiation(
    const char* alias, builder::InstanceID* ret) {
    if (alias == NULL) return 1;

    for (unsigned int i = 0; i < this->components.size(); ++i) {
        if (this->components[i].alias == NULL) continue;
        if (strcmp(this->components[i].alias, alias) == 0) {
            *ret = i;
            return 0;
        }
    }

    return 1;
}

sinuca::builder::DefinitionID
sinuca::config::EngineBuilder::GetComponentDefinitionOrMakeDummy(
    const char* name) {
    builder::DefinitionID definitionID;
    if (this->GetComponentDefinition(name, &definitionID)) {
        return this->AddDummyDefinition(name);
    }
    return definitionID;
}

sinuca::builder::InstanceID
sinuca::config::EngineBuilder::GetComponentInstantiationOrMakeDummy(
    const char* alias) {
    builder::InstanceID instanceID;
    if (this->GetComponentInstantiation(alias, &instanceID)) {
        return this->AddDummyInstance(alias);
    }
    return instanceID;
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
    int err;

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
            dest->type = builder::ParameterTypeDefinitionReference;
            err = this->AddComponentDefinitionFromYamlMapping(
                name, src->value.mapping, &dest->value.referenceToDefinition);
            if (err != 0) return 1;
            break;
        case yaml::YamlValueTypeString:
            dest->value.referenceToDefinition =
                this->GetComponentDefinitionOrMakeDummy(src->value.string);
            dest->type = builder::ParameterTypeDefinitionReference;
            break;
        case yaml::YamlValueTypeAlias:
            if (strcmp(src->value.alias, "ENGINE") == 0) {
                dest->value.referenceToInstance = 0;
            } else {
                dest->value.referenceToInstance =
                    this->GetComponentInstantiationOrMakeDummy(
                        src->value.alias);
            }
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
    builder::DefinitionID id,
    const std::vector<yaml::YamlMappingEntry*>* config) {
    SINUCA3_DEBUG_PRINTF("    Filling parameters and class for definition.\n");

    builder::ComponentDefinition* definition = &this->componentDefinitions[id];

    definition->clazz = NULL;

    if (config->size() == 0) {
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
        if (i == config->size() && definition->clazz == NULL) {
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

int sinuca::config::EngineBuilder::AddComponentDefinitionFromYamlMapping(
    const char* name, const std::vector<yaml::YamlMappingEntry*>* config,
    builder::DefinitionID* ret) {
    builder::DefinitionID definitionID;
    bool mustPop = false;

    if (this->GetComponentDefinition(name, &definitionID)) {
        builder::ComponentDefinition definition =
            builder::ComponentDefinition(name, true);
        this->componentDefinitions.push_back(definition);

        definitionID = this->componentDefinitions.size() - 1;
        mustPop = true;
    } else if (this->componentDefinitions[definitionID].alreadyDefined) {
        SINUCA3_ERROR_PRINTF("Multiple definitions of component %s.", name);
        return 1;
    }

    if (this->FillParametersAndClass(definitionID, config)) {
        if (mustPop) this->componentDefinitions.pop_back();
        return 1;
    }

    *ret = definitionID;
    return 0;
}

int sinuca::config::EngineBuilder::AddComponentInstantiationFromYamlMapping(
    const char* name, const char* alias,
    const std::vector<yaml::YamlMappingEntry*>* config,
    builder::InstanceID* ret) {
    builder::DefinitionID definition;
    if (this->AddComponentDefinitionFromYamlMapping(name, config,
                                                    &definition)) {
        return 1;
    }

    builder::InstanceID instance;
    if (!this->GetComponentInstantiation(alias, &instance)) {
        builder::ComponentInstantiation* instancePtr =
            &this->components[instance];
        if (instancePtr->alreadyDefined) {
            SINUCA3_ERROR_PRINTF("Multiple components with alias %s.", alias);
            return 1;
        }
        instancePtr->alreadyDefined = true;
        instancePtr->definition = definition;
        *ret = instance;
        return 0;
    }

    builder::ComponentInstantiation newInstance =
        builder::ComponentInstantiation(alias, definition, true);

    this->components.push_back(newInstance);
    *ret = this->components.size() - 1;

    return 0;
}

int sinuca::config::EngineBuilder::TreatInstantiateParameter(
    const yaml::YamlValue* value) {
    builder::InstanceID dummy;
    switch (value->type) {
        case yaml::YamlValueTypeString:
            SINUCA3_DEBUG_PRINTF(
                "Instantiating anonymous component from reference %s.\n",
                value->value.string);
            this->NewComponentFromDefinitionReference(
                this->GetComponentDefinitionOrMakeDummy(value->value.string));
            break;
        case yaml::YamlValueTypeMapping:
            SINUCA3_DEBUG_PRINTF(
                "Instantiating anonymous component from mapping.\n");
            this->AddComponentInstantiationFromYamlMapping(
                NULL, NULL, value->value.mapping, &dummy);
            break;
        default:
            SINUCA3_ERROR_PRINTF(
                "Argument to \"instantiate\" parameter is not a component "
                "definition.\n");
            return 1;
    }

    return 0;
}

int sinuca::config::EngineBuilder::TreatParameter(
    const char* name, const yaml::YamlValue* value) {
    SINUCA3_DEBUG_PRINTF("Treating parameter %s.\n", name);

    if (strcmp(name, "instantiate") == 0) {
        return this->TreatInstantiateParameter(value);
    }

    // If the parameter is not an "instantiate" special parameter, it's a
    // component definition.
    SINUCA3_DEBUG_PRINTF("Parameter is a component definition.\n");
    if (value->type != yaml::YamlValueTypeMapping) {
        SINUCA3_ERROR_PRINTF(
            "While trying to define component %s: expected a YAML Mapping, "
            "got %s.\n",
            name, value->TypeAsString());
        return 1;
    }

    SINUCA3_DEBUG_PRINTF("  With anchor %s.\n", value->anchor);
    builder::InstanceID dummy;
    if (value->anchor != NULL) {
        if (this->AddComponentInstantiationFromYamlMapping(
                name, value->anchor, value->value.mapping, &dummy) != 0)
            return 1;

        SINUCA3_DEBUG_PRINTF("  Successfully added component instantiation.\n");
        return 0;
    }

    if (this->AddComponentDefinitionFromYamlMapping(name, value->value.mapping,
                                                    &dummy) != 0)
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
    delete this->engine;

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
    builder::DefinitionID reference) {
    engine::Linkable* component =
        CreateComponent(this->componentDefinitions[reference].clazz);
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
            *value = ConfigValue(
                this->components[parameter->value.referenceToInstance]
                    .component);
            break;
        case builder::ParameterTypeArray:
            return this->ArrayParameter2ConfigValue(&parameter->value.array,
                                                    value);
            break;
        case builder::ParameterTypeDefinitionReference:
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
    builder::InstanceID instance) {
    sinuca::engine::Linkable* component = this->components[instance].component;
    builder::DefinitionID definition = this->components[instance].definition;

    // We need to perform the access to map each iteration as
    // Parameter2ConfigValue may append to the array, thus changing the location
    // of the map.
    for (long i = 0; i < this->componentDefinitions[definition].parameters.size;
         ++i) {
        builder::ParameterMap* map =
            &this->componentDefinitions[definition].parameters;
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
    //
    // We skip the engine of course.
    for (unsigned long i = 1; i < this->components.size(); ++i) {
        builder::ComponentInstantiation* component = &this->components[i];
        builder::ComponentDefinition* definition =
            &this->componentDefinitions[component->definition];
        component->component = CreateComponent(definition->clazz);

        // No such class.
        if (component->component == NULL) {
            SINUCA3_ERROR_PRINTF("No such component class: %s.\n",
                                 definition->clazz);
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
        }
    }

    // Pass the parameters to the components. This also resolves pointers to
    // definitions, thus allocating more components. As we already ensured all
    // definitions are satisfied, this cannot fail.
    //
    // We skip the engine of course.
    for (unsigned long i = 1; i < this->components.size(); ++i) {
        if (this->SetupComponentConfig(i))
            return this->FreeSelfOnInstantiationFailure(yamlConfig);
    }

    engine::Engine* engine = this->BuildEngine();
    delete yamlConfig;
    return engine;
}

sinuca::engine::Engine* sinuca::config::EngineBuilder::BuildEngine() {
    long numberOfComponents = this->components.size();
    engine::Linkable** components = new engine::Linkable*[numberOfComponents];

    assert(components != NULL);

    for (long i = 0; i < numberOfComponents; ++i)
        components[i] = this->components[i].component;

    this->engine->Instantiate(components, this->components.size());

    return this->engine;
}
