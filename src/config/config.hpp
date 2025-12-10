#ifndef SINUCA3_CONFIG_HPP
#define SINUCA3_CONFIG_HPP

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
 * @file config.hpp
 * @brief Configuration public API for SiNUCA3.
 */

#include <cassert>
#include <engine/build_definitions.hpp>
#include <engine/component.hpp>
#include <utils/logging.hpp>
#include <utils/map.hpp>
#include <vector>
#include <yaml/yaml_parser.hpp>

/**
 * @brief Don't call, used by the engine when building itself.
 * @details Allocates a default component by it's class name.
 */
Linkable* CreateDefaultComponentByClass(const char* name);
/**
 * @brief Don't call, used by the engine when building itself.
 * @details Allocates a custom component by it's class name.
 */
Linkable* CreateCustomComponentByClass(const char* name);
/**
 * @brief Don't call, used by the engine when building itself.
 * @details Allocates a component by it's class name.
 */
Linkable* CreateComponentByClass(const char* clazz);

class Config {
  private:
    Map<yaml::YamlValue>* config;
    std::vector<Linkable*>* components;
    Map<Linkable*>* aliases;
    Map<Definition>* definitions;
    yaml::YamlLocation location;

    inline void RequiredParameterNotPassed(const char* const parameter) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Required parameter not passed: %s.\n",
                             this->location.file, this->location.line,
                             this->location.column, parameter);
    }

    Linkable* GetComponentByMapping(Map<yaml::YamlValue>* mapping,
                                    yaml::YamlLocation location);
    Linkable* GetComponentByAlias(const char* alias,
                                  yaml::YamlLocation location);
    Linkable* GetComponentByString(const char* string,
                                   yaml::YamlLocation location);
    Linkable* GetComponentFromYaml(yaml::YamlValue* yaml);

  public:
    inline Config(std::vector<Linkable*>* components, Map<Linkable*>* aliases,
                  Map<Definition>* definitions, Map<yaml::YamlValue>* config,
                  yaml::YamlLocation location)
        : config(config),
          components(components),
          aliases(aliases),
          definitions(definitions),
          location(location) {}

    template <typename ComponentType>
    int ComponentReference(const char* parameter, ComponentType** ret,
                           bool required = false) {
        yaml::YamlValue* value = this->config->Get(parameter);
        if (value == NULL) {
            if (!required) return 0;
            this->RequiredParameterNotPassed(parameter);
            return 1;
        }
        *ret = dynamic_cast<ComponentType*>(this->GetComponentFromYaml(value));
        return *ret == NULL;
    }

    int Bool(const char* parameter, bool* ret, bool required = false);
    int Integer(const char* parameter, long* ret, bool required = false);
    int Floating(const char* parameter, double* ret, bool required = false);
    int String(const char* parameter, const char** ret, bool required = false);

    int Error(const char* parameter, const char* reason);

    /** @brief Only use if you know what you're doing. */
    Map<yaml::YamlValue>* RawYaml() { return this->config; }

    /** @brief Only use if you know what you're doing. Probably never except
     * when coding the Engine. */
    std::vector<Linkable*>* Components() { return this->components; }

    /** @brief Only use if you know what you're doing. Probably never except
     * when coding the Engine. */
    Map<Linkable*>* Aliases() { return this->aliases; }

    /** @brief Only use if you know what you're doing. Probably never except
     * when coding the Engine. */
    Map<Definition>* Definitions() { return this->definitions; }
};

/**
 * @brief Creates a "fake" configuration for testing a component.
 * @param parser It's lifetime is the lifetime of the configuration itself, so
 * the caller should pass one.
 * @param content The content.
 * @param aliases An aliases mapping.
 */
Config CreateFakeConfig(yaml::Parser* parser, const char* content,
                        Map<Linkable*>* aliases);

#endif  // SINUCA3_CONFIG_HPP
