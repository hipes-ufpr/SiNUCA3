//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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
 * @file fetcher.cpp
 * @brief API of the Fetcher, a generic component for simulating logic in the
 * first stage of a pipeline, supporting integration with predictors and caches.
 */

#include "fetcher.hpp"

#include <cstring>

#include "../../sinuca3.hpp"
#include "../../utils/logging.hpp"

int Fetcher::FinishSetup() {
    if (this->fetch == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher didn't received required `fetch` parameter.\n");
        return 1;
    }

    // Default fetchSize and fetchInterval to 1, if undefined.
    if (this->fetchSize == 0) {
        this->fetchSize = 1;
    }
    if (this->fetchInterval == 0) {
        this->fetchInterval = 1;
    }

    this->fetchBuffer = new sinuca::InstructionPacket[this->fetchSize];

    this->fetchID = this->fetch->Connect(this->fetchSize);
    if (this->instructionMemory != NULL) {
        this->instructionMemoryID =
            this->instructionMemory->Connect(this->fetchSize);
    }
    if (this->predictors != NULL) {
        this->predictorsID = new int[this->numberOfPredictors];
        for (long i = 0; i < this->numberOfPredictors; ++i) {
            this->predictorsID[i] =
                this->predictors[i]->Connect(this->fetchSize);
        }
    }

    return 0;
}

int Fetcher::SetConfigParameter(const char* parameter,
                                sinuca::config::ConfigValue value) {
    if (strcmp(parameter, "fetch") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeComponentReference) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher received a parameter `fetch` that's not a reference "
                "to a Component<InstructionPacket>.\n");
            return 1;
        }

        this->fetch =
            dynamic_cast<sinuca::Component<sinuca::InstructionPacket>*>(
                value.value.componentReference);
        if (this->fetch == NULL) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher received a parameter `fetch` that's not a reference "
                "to a Component<InstructionPacket>.\n");
            return 1;
        }

        return 0;
    }

    if (strcmp(parameter, "instructionMemory") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeComponentReference) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `instructionMemory` is not "
                "Component<MemoryPacket>.\n");
            return 1;
        }

        this->instructionMemory =
            dynamic_cast<sinuca::Component<sinuca::MemoryPacket>*>(
                value.value.componentReference);
        if (this->instructionMemory == NULL) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `instructionMemory` is not "
                "Component<MemoryPacket>.\n");
            return 1;
        }

        return 0;
    }

    if (strcmp(parameter, "predictors") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeArray) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher received a `predictors` parameter that's not an array "
                "of Component<PredictorPacket>.\n");
            return 1;
        }

        if (this->predictors != NULL) {
            delete[] this->predictors;
        }

        std::vector<sinuca::config::ConfigValue>* configArray =
            value.value.array;
        this->predictors = new sinuca::Component<
            sinuca::PredictorPacket>*[configArray->size()];
        for (unsigned long i = 0; i < configArray->size(); ++i) {
            if ((*configArray)[i].type !=
                sinuca::config::ConfigValueTypeComponentReference) {
                SINUCA3_ERROR_PRINTF(
                    "A parameter of the array `predictors` passed to Fetcher "
                    "is not a Component<PredictorPacket>.\n");
                return 1;
            }

            this->predictors[i] =
                dynamic_cast<sinuca::Component<sinuca::PredictorPacket>*>(
                    (*configArray)[i].value.componentReference);
            if (this->predictors[i] == NULL) {
                SINUCA3_ERROR_PRINTF(
                    "A parameter of the array `predictors` passed to Fetcher "
                    "is not a Component<PredictorPacket>.\n");
                return 1;
            }
        }

        return 0;
    }

    if (strcmp(parameter, "fetchSize") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `fetchSize` is not an integer.\n");
            return 1;
        }
        if (value.value.integer <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `fetchSize` should be greater than 0, got "
                "%ld.\n",
                value.value.integer);
            return 1;
        }
        this->fetchSize = value.value.integer;
        return 0;
    }

    if (strcmp(parameter, "fetchInterval") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `fetchInterval` is not an integer.\n");
            return 1;
        }
        if (value.value.integer <= 0) {
            SINUCA3_ERROR_PRINTF(
                "Fetcher parameter `fetchInterval` should be greater than "
                "0.\n");
            return 1;
        }
        return 0;
    }

    SINUCA3_ERROR_PRINTF("Fetcher received unknown parameter %s.\n", parameter);
    return 1;
}

void Fetcher::Clock() {
    // TODO.

    ++this->clock;
}

void Fetcher::Flush() {}

void Fetcher::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Fetcher %p: fetched %lu instructions.\n", this,
                       this->numberOfFetchedInstructions);
}

Fetcher::~Fetcher() {
    if (this->predictors != NULL) {
        delete[] this->predictors;
    }
    if (this->fetchBuffer != NULL) {
        delete[] this->fetchBuffer;
    }
}
