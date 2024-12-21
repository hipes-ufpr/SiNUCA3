#ifndef SINUCA3_CONFIG_ENGINE_BUILDER_HPP_
#define SINUCA3_CONFIG_ENGINE_BUILDER_HPP_

#include <cassert>
#include <vector>

#include "../engine/engine.hpp"
#include "yaml_parser.hpp"

namespace sinuca {

namespace builder {

enum ParameterType {
    ParameterTypeInteger = sinuca::config::ConfigValueTypeInteger,
    ParameterTypeNumber = sinuca::config::ConfigValueTypeNumber,
    ParameterTypeBoolean = sinuca::config::ConfigValueTypeBoolean,
    ParameterTypeArray = sinuca::config::ConfigValueTypeArray,
    ParameterTypeInstanceReference =
        sinuca::config::ConfigValueTypeComponentReference,
    ParameterTypeDefinitionReference,
};

struct ComponentDefinition;
struct ComponentInstantiation {
    const char* alias;
    ComponentDefinition* definition;
    sinuca::engine::Linkable* component;
    bool alreadyDefined;

    inline ComponentInstantiation(const char* alias,
                                  ComponentDefinition* definition,
                                  bool defined) {
        this->alias = alias;
        this->definition = definition;
        this->alreadyDefined = defined;
    }
};

struct Parameter;
struct ParameterArray {
    Parameter* parameters;
    long size;
};

struct Parameter {
    union {
        long integer;
        double number;
        bool boolean;
        ParameterArray array;
        ComponentInstantiation* referenceToInstance;
        ComponentDefinition* referenceToDefinition;
    } value;
    const char* name;
    sinuca::builder::ParameterType type;

    inline ~Parameter() {
        if (this->type == ParameterTypeArray)
            delete this->value.array.parameters;
    }
};

struct ParameterMapItem {
    const char* name;
    Parameter value;
};

struct ParameterMap {
    ParameterMapItem* items;
    long size;

    inline ~ParameterMap() {
        assert(this->items != NULL);
        delete[] items;
    }
};

struct ComponentDefinition {
    ParameterMap parameters;
    const char* name;
    const char* clazz; /** @brief `class` is a reserved word. */
    bool alreadyDefined;

    inline ComponentDefinition(const char* name, bool defined) {
        this->name = name;
        this->clazz = NULL;
        this->alreadyDefined = defined;
    }
};

}  // namespace builder

namespace config {

class EngineBuilder {
  private:
    // These are the stuff we're going to produce.
    std::vector<builder::ComponentDefinition> componentDefinitions;
    std::vector<builder::ComponentInstantiation> components;
    // Ok, here's a very unreadable and tricky way of doing things. Both cores
    // and numberOfCores will be zero-initialized so when we reach the `cores`
    // parameter, we know that it was never reached before. If we reach it and
    // these are not zeroed, we know that the parameter was defined multiple
    // times. For pratical reasons, what this means is that these two variables
    // are "actually initialized" after we reach the `cores` parameter.
    builder::ComponentInstantiation** cores;
    long numberOfCores;

    /** @brief Initializes the cores and numberOfCores passed by reference, and
     * may return error. */
    int TreatCoresParameter(const sinuca::yaml::YamlValue* coresConfig);
    /** @brief Add the cores. */
    int AddCores(const std::vector<sinuca::yaml::YamlValue*>* coresConfig);
    /** @brief Simply adds an instantiation. Adds a definition also when
     * config->type == sinuca::yaml::YamlValueTypeMapping. */
    builder::ComponentInstantiation* AddComponentInstantiationFromYamlMapping(
        const char* name, const char* alias,
        const std::vector<yaml::YamlMappingEntry*>* config);
    /** @brief Simply adds a definition, recursively. */
    builder::ComponentDefinition* AddComponentDefinitionFromYamlMapping(
        const char* name, const std::vector<yaml::YamlMappingEntry*>* config);
    /** @brief Adds a definition not yet defined. */
    builder::ComponentDefinition* AddDummyDefinition(const char* name);
    /** @brief Adds a instance not yet defined. */
    builder::ComponentInstantiation* AddDummyInstance(const char* alias);
    /** @brief Makes an array of parameters from a YAML array. */
    int YamlArray2Parameter(builder::Parameter* dest,
                            const std::vector<yaml::YamlValue*>* array);

    int FillParametersAndComponent(
        builder::ComponentDefinition* definition,
        const std::vector<yaml::YamlMappingEntry*>* config);

    builder::ComponentDefinition* GetComponentDefinition(const char* name);
    builder::ComponentInstantiation* GetComponentInstantiation(
        const char* alias);

    int Yaml2Parameter(const char* name, builder::Parameter* dest,
                       const yaml::YamlValue* src);

  public:
    engine::Engine* Instantiate(const char* configFile);
    EngineBuilder();
    ~EngineBuilder();
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_ENGINE_BUILDER_HPP_
