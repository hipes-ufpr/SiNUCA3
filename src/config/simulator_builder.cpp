#include "simulator_builder.hpp"

#include <cstdio>
#include <yaml.h>

sinuca::engine::Engine* sinuca::config::SimulatorBuilder::InstantiateSimulationEngine(const std::string *configFile) {
    FILE* fp = fopen(configFile->c_str(), "r");
    if (fp == NULL) return NULL;

    yaml_parser_t parser;

    // This only fails on allocation failures so we don't check.
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fp);

    fclose(fp);
    return NULL;
}
