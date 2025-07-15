#ifndef SINUCA3_CONFIG_ENGINE_BUILDER_HPP_
#define SINUCA3_CONFIG_ENGINE_BUILDER_HPP_

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
 * @file engine_builder.hpp
 * @brief Public API of the EngineBuilder, that instantiates an Engine.
 * @details The EngineBuilder has a single public method Instantiate, that
 * receives a file path and instantiates an Engine with it's configuration. If
 * there's an error, NULL is returned. For a better understanding of the way the
 * EngineBuilder transforms YAML into components, read the implementation docs
 * on engine_builder.cpp.
 */

#include <cassert>
#include <vector>

#include "../engine/engine.hpp"
#include "yaml_parser.hpp"

namespace sinuca {

namespace builder {

/** @brief Index inside the builder's instances array. */
typedef int InstanceID;
/** @brief Index inside the builder's definitions array. */
typedef int DefinitionID;

/**
 * @brief Intermediate representation of the types of parameters components may
 * receive.
 * @details Because of the idea that components may receive pointers that create
 * other components in their configuration, and pointer that only points to the
 * same component always, we need two types for representing the reference to
 * other components.
 */
enum ParameterType {
    ParameterTypeInteger =
        sinuca::config::ConfigValueTypeInteger, /** @brief Integer (long)
                                                 * parameter. */
    ParameterTypeNumber =
        sinuca::config::ConfigValueTypeNumber, /** @brief Number (double)
                                                  parameter. */
    ParameterTypeBoolean =
        sinuca::config::ConfigValueTypeBoolean, /** @brief Boolean (bool)
                                                 * parameter. */
    ParameterTypeArray =
        sinuca::config::ConfigValueTypeArray, /** @brief Array of parameters. */
    ParameterTypeInstanceReference =
        sinuca::config::ConfigValueTypeComponentReference, /** @brief A
                                                              reference to other
                                                              component. */
    ParameterTypeDefinitionReference, /** @brief A reference to a component
                                         definition, i.e., a reference to other
                                         component that must be instantiated on
                                         the reference site. */
};

// Pre-declaration for ComponentInstantiation.
struct ComponentDefinition;

/**
 * @brief Represents an instance of a component.
 * @details An instance is literally like an instance of a class in an
 * object-oriented fashion. In this case, the instance points to a definition
 * (which can be though of as the class, again) that holds the parameters.
 */
struct ComponentInstantiation {
    /**
     * @brief If the component has an alias, it's this pointer.
     * @details This is a pointer to a C-style string from the YamlParser. It
     * can be treated as an immutable value throughout it's life, as YamlParser
     * is responsible for it's deletion.
     */
    const char* alias;
    /**
     * @brief Points to the definition of the component.
     */
    DefinitionID definition;
    /**
     * @brief Points to the alloced component itself.
     * @details Sometime during the processing, the components are alloced with
     * the operator new. This points to the component, so the reference can be
     * resolved in the configuration of other components later. Thus, we cannot
     * delete this pointer here because it's literally the thing we're creating.
     */
    sinuca::engine::Linkable* component;
    /**
     * @brief Tells if the instance was already defined in the configuration
     * file or if it was previously added because another component points to
     * it.
     * @details If the builder encounters a reference to an instance not yet
     * defined, it adds a dumb instance with this parameter set to false to the
     * vector of instances. When the builder encounters the definition of the
     * instance, this is set to true. This way, multiple definitions will result
     * in a configuration error, and forward-referencing is possible.
     */
    bool alreadyDefined;

    inline ComponentInstantiation(const char* alias, DefinitionID definition,
                                  bool defined) {
        this->alias = alias;
        this->definition = definition;
        this->alreadyDefined = defined;
    }
};

// Pre-declaration for ParameterArray.
struct Parameter;

/**
 * @brief Intermediate representation of an array of parameters. This is a
 * helper struct managed by Parameter, i.e., it takes care of it's memory.
 */
struct ParameterArray {
    Parameter* parameters;
    long size;
};

/**
 * @brief Intermediate representation of a parameter value for a component.
 * Tagged union.
 * @details This is a intermediate representation (with the two reference kinds)
 * of a parameter value that'll be passed to components.
 */
struct Parameter {
    union {
        long integer;
        double number;
        bool boolean;
        ParameterArray array;
        builder::InstanceID referenceToInstance;
        builder::DefinitionID referenceToDefinition;
    } value;            /** @brief The value itself. Tagged union with type. */
    ParameterType type; /** @brief Tag for value. */

    /**
     * @brief Constructor for when a dumb parameter is needed to be filled
     * later.
     */
    inline Parameter() : type(ParameterTypeInteger) {}

    inline ~Parameter() {
        if (this->type == ParameterTypeArray &&
            this->value.array.parameters != NULL)
            delete this->value.array.parameters;
    }
};

/** @brief Intermediate representation of a parameter itself (with a name). */
struct ParameterMapItem {
    /**
     * @brief Name of the parameter.
     * @details This is a pointer to a C-style string from the YamlParser. It
     * can be treated as an immutable value throughout it's life, as YamlParser
     * is responsible for it's deletion.
     */
    const char* name;
    Parameter value; /** @brief It's value. */
};

/**
 * @brief Intermediate representation of a list of parameters, i.e., the entire
 * configuration passed to a component.
 */
struct ParameterMap {
    ParameterMapItem* items; /** @brief The parameters. Vector of size size. */
    long size;               /** @brief Size of items. */

    /** @brief Ensure items is NULL on initialization so it can be used to check
     * if the map is filled or not. */
    inline ParameterMap() { this->items = NULL; }

    inline ~ParameterMap() {
        if (this->items != NULL) delete[] this->items;
    }
};

/**
 * @brief Intermediate representation of a component definition, i.e., not an
 * instance, but a list of parameters passed to a certain component, that can be
 * referenced to create instances of components.
 */
struct ComponentDefinition {
    ParameterMap parameters; /** @brief The parameters. */
    /**
     * @brief The name of the definition.
     * @details This is a pointer to a C-style string from the YamlParser. It
     * can be treated as an immutable value throughout it's life, as YamlParser
     * is responsible for it's deletion.
     */
    const char* name;

    /**
     * @brief The class of the component.
     * @details The class that will be alloced with the operator new. Named with
     * zz because `class` is a reserved word. This is a pointer to a C-style
     * string from the YamlParser. It can be treated as an immutable value
     * throughout it's life, as YamlParser is responsible for it's deletion.
     */
    const char* clazz;
    /**
     * @brief Tells if the definition was already defined in the configuration
     * file or if it was previously added because another component points to
     * it.
     * @details If the builder encounters a reference to a definition not yet
     * defined, it adds a dumb definition with this parameter set to false to
     * the vector of definitions. When the builder encounters the definition of
     * the definition, this is set to true. This way, multiple definitions will
     * result in a configuration error, and forward-referencing is possible.
     */
    bool alreadyDefined;

    /**
     * @brief Ensures clazz is initialized to NULL and the other fields have a
     * proper initialization.
     * @details The class can only be found after reading through
     * the YAML mapping, so it must be filled only after the initialization.
     */
    inline ComponentDefinition(const char* name, bool defined) {
        this->name = name;
        this->clazz = NULL;
        this->alreadyDefined = defined;
    }
};

}  // namespace builder

namespace config {

/**
 * @brief Builds an Engine from a configuration file, with the single public
 * method Instantiate.
 */
class EngineBuilder {
  private:
    /**
     * @brief What we're building.
     */
    engine::Engine* engine;

    /**
     * @brief Part of the stuff we're going to produce.
     * @details Vector of definitions that we fill throughout the reading of the
     * configuration file.
     */
    std::vector<builder::ComponentDefinition> componentDefinitions;
    /**
     * @brief Part of the stuff we're going to produce.
     * @details Vector of instances that we fill throughout the reading of the
     * configuration file. The first element is guaranteed to be the engine
     * itself.
     */
    std::vector<builder::ComponentInstantiation> components;

    /** @brief Simply adds an instantiation and a definition. Calls
     * AddComponentDefinitionFromYamlMapping first, then creates an instance.
     * Returns non-zero on error. */
    int AddComponentInstantiationFromYamlMapping(
        const char* name, const char* alias,
        const std::vector<yaml::YamlMappingEntry*>* config,
        builder::InstanceID* ret);
    /** @brief Simply adds a definition, recursively. Returns non-zero on error.
     */
    builder::DefinitionID AddComponentDefinitionFromYamlMapping(
        const char* name, const std::vector<yaml::YamlMappingEntry*>* config,
        builder::DefinitionID* ret);
    /** @brief Adds a definition not yet defined. */
    builder::DefinitionID AddDummyDefinition(const char* name);
    /** @brief Adds an instance not yet defined. */
    builder::InstanceID AddDummyInstance(const char* alias);
    /** @brief Makes an array of parameters from a YAML array. */
    int YamlArray2Parameter(builder::Parameter* dest,
                            const std::vector<yaml::YamlValue*>* array);

    /** @brief Fills the parameters and the class of a component. Parameter are
     * translated with Yaml2Parameter. */
    int FillParametersAndClass(
        builder::DefinitionID id,
        const std::vector<yaml::YamlMappingEntry*>* config);

    /** @brief If there's a component definition with the name passed, returns a
     * pointer to it. Otherwise creates a dummy one and returns it's pointer. */
    builder::DefinitionID GetComponentDefinitionOrMakeDummy(const char* name);
    /** @brief If there's a component instance with the name passed, returns a
     * pointer to it. Otherwise creates a dummy one and returns it's pointer. */
    builder::InstanceID GetComponentInstantiationOrMakeDummy(const char* alias);

    /** @brief Returns a definition with the name passed by reference. If it
     * does not already exists, returns 1. */
    int GetComponentDefinition(const char* name, builder::DefinitionID* ret);
    /** @brief Returns an instance with the name passed by reference. If it does
     * not already exists, returns 1. */
    int GetComponentInstantiation(const char* alias, builder::InstanceID* ret);

    /** @brief Translates YAML to a parameter. This may recursively call
     * AddComponentDefinitionFromYamlMapping */
    int Yaml2Parameter(const char* name, builder::Parameter* dest,
                       const yaml::YamlValue* src);

    /** @brief Returns 0 if all components have the alreadyDefined parameter set
     * to true. */
    int EnsureAllComponentsAreDefined();

    /** @brief Takes care of parameters in the configuration toplevel, i.e.,
     * calls the functions to create definitions, instances and deal with the
     * cores parameter. */
    int TreatParameter(const char* name, const yaml::YamlValue* value);

    /**
     * @brief Pass the parameters to a component, with Parameter2ConfigValue.
     * This method also resolves pointers to definitions, thus allocating more
     * components.
     */
    int SetupComponentConfig(const builder::InstanceID instance);
    /** @brief Translates the intermediate representation of the parameter to
     * the config API representation. Note that this function adds more
     * instances when it encounters references to definitions. */
    int Parameter2ConfigValue(const builder::Parameter* parameter,
                              ConfigValue* value);
    /** @brief Called by Parameter2ConfigValue to translate array values. */
    int ArrayParameter2ConfigValue(const builder::ParameterArray* array,
                                   ConfigValue* value);

    /** @brief Called by Parameter2ConfigValue to create new instances from
     * references to definitions. */
    engine::Linkable* NewComponentFromDefinitionReference(
        builder::DefinitionID reference);

    /** @brief Creates an anonymous instance defined by the instantiate
     * parameter. */
    int TreatInstantiateParameter(const yaml::YamlValue* value);

    /** @brief Helper that frees all memory used by the builder and returns NULL
     * (to be used as `return this->FreeSelf...`). */
    sinuca::engine::Engine* FreeSelfOnInstantiationFailure(
        const yaml::YamlValue* yamlConfig);

    /** @brief After everything is done, constructs the engine. */
    sinuca::engine::Engine* BuildEngine();

  public:
    /** @brief Instantiates an Engine from a configuration file, returning NULL
     * on error. */
    engine::Engine* Instantiate(const char* configFile);
    inline EngineBuilder() : engine(new engine::Engine) {
        builder::ComponentInstantiation engineInstantiation =
            builder::ComponentInstantiation(NULL, 0, true);
        engineInstantiation.component = this->engine;
        // Heuristic.
        this->componentDefinitions.reserve(32);
        this->components.reserve(32);
        this->components.push_back(engineInstantiation);
    }
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_ENGINE_BUILDER_HPP_
