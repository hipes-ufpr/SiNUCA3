#include "config.hpp"

#include <cstdio>
#include <cstring>
#include <engine/build_definitions.hpp>
#include <utils/logging.hpp>
#include <vector>
#include <yaml/yaml_parser.hpp>

Linkable* CreateComponentByClass(const char* clazz) {
    Linkable* component = CreateDefaultComponentByClass(clazz);
    if (component != NULL) return component;
    return CreateCustomComponentByClass(clazz);
}

int Config::Error(const char* parameter, const char* reason) {
    yaml::YamlLocation location = this->location;
    yaml::YamlValue* value = this->config->Get(parameter);
    if (value != NULL) location = value->location;
    SINUCA3_ERROR_PRINTF("%s:%lu:%lu %s: %s.\n", location.file, location.line,
                         location.column, parameter, reason);
    return 1;
}

int Config::GetValue(const char* parameter, bool required,
                     yaml::YamlValue** ret) {
    if (this->config != NULL) {
        yaml::YamlValue* value = this->config->Get(parameter);
        *ret = value;
        if (value != NULL) return 0;
    }

    if (!required) return 0;
    this->RequiredParameterNotPassed(parameter);
    return 1;
}

int Config::Bool(const char* parameter, bool* ret, bool required) {
    yaml::YamlValue* value;
    if (this->GetValue(parameter, required, &value)) return 1;
    if (value == NULL) return 0;

    if (value->type == yaml::YamlValueTypeString) {
        if (!strcmp(value->value.string, "true") ||
            !strcmp(value->value.string, "yes") ||
            !strcmp(value->value.string, "1")) {
            *ret = true;
            return 0;
        } else if (!strcmp(value->value.string, "false") ||
                   !strcmp(value->value.string, "no") ||
                   !strcmp(value->value.string, "0")) {
            *ret = false;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF("%s:%lu:%lu Parameter is not a boolean: %s.\n",
                         this->location.file, this->location.line,
                         this->location.column, parameter);
    return 1;
}

int Config::Integer(const char* parameter, long* ret, bool required) {
    yaml::YamlValue* value;
    if (this->GetValue(parameter, required, &value)) return 1;
    if (value == NULL) return 0;

    if (value->type == yaml::YamlValueTypeString &&
        sscanf(value->value.string, "%ld", ret) == 1)
        return 0;

    SINUCA3_ERROR_PRINTF("%s:%lu:%lu Parameter is not an integer: %s.\n",
                         this->location.file, this->location.line,
                         this->location.column, parameter);

    return 1;
}

int Config::Floating(const char* parameter, double* ret, bool required) {
    yaml::YamlValue* value;
    if (this->GetValue(parameter, required, &value)) return 1;
    if (value == NULL) return 0;

    if (value->type == yaml::YamlValueTypeString &&
        sscanf(value->value.string, "%lf", ret) == 1)
        return 0;

    SINUCA3_ERROR_PRINTF("%s:%lu:%lu Parameter is not a floating: %s.\n",
                         this->location.file, this->location.line,
                         this->location.column, parameter);
    return 1;
}

int Config::String(const char* parameter, const char** ret, bool required) {
    yaml::YamlValue* value;
    if (this->GetValue(parameter, required, &value)) return 1;
    if (value == NULL) return 0;

    if (value->type == yaml::YamlValueTypeString) {
        *ret = value->value.string;
        return 0;
    }

    SINUCA3_ERROR_PRINTF("%s:%lu:%lu Parameter is not a string: %s.\n",
                         this->location.file, this->location.line,
                         this->location.column, parameter);
    return 1;
}

Linkable* Config::GetComponentFromYaml(yaml::YamlValue* yaml) {
    switch (yaml->type) {
        case yaml::YamlValueTypeString:
            return this->GetComponentByString(yaml->value.string,
                                              yaml->location);
        case yaml::YamlValueTypeAlias:
            return this->GetComponentByAlias(yaml->value.alias, yaml->location);

        case yaml::YamlValueTypeMapping:
            return this->GetComponentByMapping(yaml->value.mapping,
                                               yaml->location);

        default:
            SINUCA3_ERROR_PRINTF("%s:%lu:%lu Is not a component reference.\n",
                                 yaml->location.file, yaml->location.line,
                                 yaml->location.column);
            return NULL;
    }
}

Linkable* Config::GetComponentByAlias(const char* alias,
                                      yaml::YamlLocation location) {
    Linkable** component = this->aliases->Get(alias);
    if (component == NULL) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu No such component alias: %s.\n",
                             location.file, location.line, location.column,
                             alias);
        return NULL;
    }
    return *component;
}

Linkable* Config::GetComponentByMapping(Map<yaml::YamlValue>* config,
                                        yaml::YamlLocation location) {
    yaml::YamlValue* clazzYaml = config->Get("class");

    if (clazzYaml == NULL) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component class not passed.\n",
                             location.file, location.line, location.column);
        return NULL;
    }
    if (clazzYaml->type != yaml::YamlValueTypeString) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component class is not a string.\n",
                             clazzYaml->location.file, clazzYaml->location.line,
                             clazzYaml->location.column);
        return NULL;
    }
    const char* clazz = clazzYaml->value.string;

    Linkable* component = CreateComponentByClass(clazz);
    if (component == NULL) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component class %s doesn't exists.\n",
                             clazzYaml->location.file, clazzYaml->location.line,
                             clazzYaml->location.column, clazz);
        return NULL;
    }

    this->components->push_back(component);
    if (component->Configure(Config(this->components, this->aliases,
                                    this->definitions, config, location)))
        return NULL;

    return component;
}

Linkable* Config::GetComponentByString(const char* string,
                                       yaml::YamlLocation location) {
    Definition* definition = this->definitions->Get(string);
    if (definition == NULL) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component does not exists: %s.\n",
                             location.file, location.line, location.column,
                             string);
        return NULL;
    }

    return this->GetComponentByMapping(definition->config,
                                       definition->location);
}

Config CreateFakeConfig(yaml::Parser* parser, const char* content,
                        Map<Linkable*>* aliases) {
    std::vector<Linkable*> components;
    Map<Definition> definitions;

    yaml::YamlValue yaml;
    parser->ParseString(content, &yaml);

    return Config(&components, aliases, &definitions, yaml.value.mapping,
                  yaml.location);
}

int Config::Fork(yaml::YamlValue* value, Config* ret) {
    if (value->type != yaml::YamlValueTypeMapping) return 1;
    *ret = Config(this->Components(), this->Aliases(), this->Definitions(),
                  value->value.mapping, value->location);
    return 0;
}
