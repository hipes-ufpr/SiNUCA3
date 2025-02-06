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
 * @file main.cpp
 * @details Entry point. User interaction should go here. Besides, should mostly
 * just consume other public APIs.
 */

#include <getopt.h>

// #include <cassert>
#include <cstdlib>

#include "config/engine_builder.hpp"
#include "utils/logging.hpp"

/**
 * @brief Prints licensing information.
 */
void license() {
    SINUCA3_LOG_PRINTF(
        "SiNUCA 3 - Simulator of Non-Uniform Cache Architectures, Third "
        "iteration.\n"
        "\n"
        " Copyright (C) 2024  HiPES - Universidade Federal do Paraná\n"
        "\n"
        " This program is free software: you can redistribute it and/or "
        "modify\n"
        " it under the terms of the GNU General Public License as published "
        "by\n"
        " the Free Software Foundation, either version 3 of the License, or\n"
        " (at your option) any later version.\n"
        "\n"
        " This program is distributed in the hope that it will be useful,\n"
        " but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        " GNU General Public License for more details.\n"
        "\n"
        " You should have received a copy of the GNU General Public License\n"
        " along with this program.  If not, see "
        "<https://www.gnu.org/licenses/>.\n"
        "\n");
}

/**
 * @brief Prints the usage of the program.
 */
void usage() {
    license();
    SINUCA3_LOG_PRINTF("\n");
    // TODO: Make this pretty.
    SINUCA3_LOG_PRINTF(
        "Use -h to see this text, -c to set a configuration file (required for "
        "simulation) and -l to see license information.\n");
}

/**
 * @brief Entry point.
 * @returns Non-zero on error.
 */
int main(int argc, char* const argv[]) {
    const char* rootConfigFile = NULL;
    char nextOpt;

    while ((nextOpt = getopt(argc, argv, "lc:")) != -1) {
        switch (nextOpt) {
            case 'c':
                rootConfigFile = optarg;
                break;
            case 'l':
                license();
                return 0;
            case 'h':
                usage();
                return 0;
        }
    }

    if (rootConfigFile == NULL) {
        usage();
        return 1;
    }

    sinuca::config::EngineBuilder builder;
    sinuca::engine::Engine* engine = builder.Instantiate(rootConfigFile);
    if (engine == NULL) return 1;

#ifdef NDEBUG
    engine->Simulate();
#endif

    return 0;
}
