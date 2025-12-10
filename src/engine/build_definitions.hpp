#ifndef SINUCA3_ENGINE_BUILD_DEFINITIONS_HPP_
#define SINUCA3_ENGINE_BUILD_DEFINITIONS_HPP_

#include <yaml/yaml_parser.hpp>

// Pre-declaration because they include us.
class Linkable;

struct Definition {
    Map<yaml::YamlValue>* config;
    yaml::YamlLocation location;
};

struct InstanceWithDefinition {
    Linkable* component;
    const char* definition;
};

#endif  // SINUCA3_ENGINE_BUILD_DEFINITIONS_HPP_
