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

#include <config/engine_builder.hpp>
#include <cstdlib>
#include <cstring>
#include <sinuca3.hpp>
#include <trace_reader/sinuca3_trace_reader.hpp>
#include <trace_reader/trace_reader.hpp>

// Include our testing facilities in debug mode.
#ifndef NDEBUG
#include <tests.hpp>
#endif

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
        "simulation), -t to set a trace (also required for simulation) and -l "
        "to see license information.\n"
        "\n"
        "Other simulation options:\n"
        "   -T <string> sets the trace reader to use (orcs, foo, bar, "
        "TODO...)\n");
}

/**
 * @brief Returns a TraceReader from it's name. TODO: add default trace reader.
 */
TraceReader* AllocTraceReader(const char* traceReader) {
    if (strcmp(traceReader, "sinuca3") == 0)
        return new sinuca3TraceReader::SinucaTraceReader;
    else
        return NULL;
}

/**
 * @brief Entry point.
 * @returns Non-zero on error.
 */
int main(int argc, char* const argv[]) {
    const char* traceReaderName = "sinuca3";
    const char* rootConfigFile = NULL;
    const char* traceDir = ".";
    const char* traceFileName = NULL;
    char nextOpt;

    // When compiling debug mode, enable our testing facilities.
#ifdef NDEBUG
#define SINUCA3_SWITCHES "lc:t:d:T:"
#else
#define SINUCA3_SWITCHES "r:lc:t:d:T:"
    const char* testToRun = NULL;
#endif

    while ((nextOpt = getopt(argc, argv, SINUCA3_SWITCHES)) != -1) {
        switch (nextOpt) {
            // When compiling debug mode, enable our testing facilities.
#ifndef NDEBUG
            case 'r':
                testToRun = optarg;
                break;
#endif
            case 'c':
                rootConfigFile = optarg;
                break;
            case 't':
                traceFileName = optarg;
                break;
            case 'd':
                traceDir = optarg;
                break;
            case 'T':
                traceReaderName = optarg;
                break;
            case 'l':
                license();
                return 0;
            case 'h':
                usage();
                return 0;
        }
    }

    // When compiling debug mode and there's a test to run, run it.
#ifndef NDEBUG
    if (testToRun != NULL) {
        int ret = Test(testToRun);
        if (ret < 0) {
            SINUCA3_LOG_PRINTF("No such test: %s\n", testToRun);
        } else if (ret > 0) {
            SINUCA3_LOG_PRINTF("Test failed with code %d.\n", ret);
        }
        return ret;
    }
#endif

    if (rootConfigFile == NULL) {
        usage();
        return 1;
    }
    if (traceFileName == NULL) {
        usage();
        return 1;
    }

    EngineBuilder builder;
    Engine* engine = builder.Instantiate(rootConfigFile);
    if (engine == NULL) return 1;

    TraceReader* traceReader = AllocTraceReader(traceReaderName);
    if (traceReader == NULL) {
        SINUCA3_ERROR_PRINTF("The trace reader %s does not exist.",
                             traceReaderName);
        return 1;
    }
    if (traceReader->OpenTrace(traceFileName, traceDir)) return 1;

    engine->Simulate(traceReader);
    delete engine;
    delete traceReader;

    return 0;
}
