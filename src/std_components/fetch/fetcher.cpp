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

#include <cassert>

#include "../../sinuca3.hpp"
#include "../../utils/logging.hpp"

int Fetcher::FinishSetup() {
    if (this->fetch == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher didn't received required parameter `fetch`.\n");
        return 1;
    }
    if (this->instructionMemory == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher didn't received required parameter "
            "`instructionMemory`.\n");
        return 1;
    }

    this->fetchID = this->fetch->Connect(this->fetchSize);
    this->instructionMemoryID =
        this->instructionMemory->Connect(this->fetchSize);

    this->fetchBuffer = new FetchBufferEntry[this->fetchSize];

    // Maybe connect to a predictor.
    if (this->predictor != NULL) {
        this->predictorID = this->predictor->Connect(this->fetchSize);
    }

    return 0;
}

int Fetcher::FetchConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeComponentReference) {
        this->fetch = dynamic_cast<sinuca::Component<sinuca::FetchPacket>*>(
            value.value.componentReference);
        if (this->fetch != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetch` is not a Component<FetchPacket>.\n");
    return 1;
}

int Fetcher::InstructionMemoryConfigParameter(
    sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeComponentReference) {
        this->instructionMemory =
            dynamic_cast<sinuca::Component<sinuca::InstructionPacket>*>(
                value.value.componentReference);
        if (this->instructionMemory != NULL) {
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `instructionMemory` is not a "
        "Component<InstructionPacket>.\n");
    return 1;
}

int Fetcher::FetchSizeConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchSize = value.value.integer;
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchSize` is not a integer > 0.\n");
    return 1;
}

int Fetcher::FetchIntervalConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchInterval = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchInterval` is not a integer > 0.\n");
    return 1;
}

int Fetcher::PredictorConfigParameter(sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeComponentReference) {
        this->predictor =
            dynamic_cast<sinuca::Component<sinuca::PredictorPacket>*>(
                value.value.componentReference);
        if (this->predictor != NULL) return 0;
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `predictor` is not a "
        "Component<PredictorPacket>.\n");
    return 1;
}

int Fetcher::MisspredictPenaltyConfigParameter(
    sinuca::config::ConfigValue value) {
    if (value.type == sinuca::config::ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->misspredictPenalty = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `misspredictPenalty` is not a integer > 0.\n");
    return 1;
}

int Fetcher::SetConfigParameter(const char* parameter,
                                sinuca::config::ConfigValue value) {
    if (strcmp(parameter, "fetch") == 0) {
        return this->FetchConfigParameter(value);
    } else if (strcmp(parameter, "instructionMemory") == 0) {
        return this->InstructionMemoryConfigParameter(value);
    } else if (strcmp(parameter, "fetchSize") == 0) {
        return this->FetchSizeConfigParameter(value);
    } else if (strcmp(parameter, "fetchInterval") == 0) {
        return this->FetchIntervalConfigParameter(value);
    } else if (strcmp(parameter, "predictor") == 0) {
        return this->PredictorConfigParameter(value);
    } else if (strcmp(parameter, "misspredictPenalty") == 0) {
        return this->MisspredictPenaltyConfigParameter(value);
    }

    SINUCA3_ERROR_PRINTF("Fetcher received unknown parameter %s.\n", parameter);

    return 1;
}

void Fetcher::ClockSendBuffered() {
    unsigned long i = 0;

    // Skip instructions we already sent.
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToMemory) ++i;

    while (i < this->fetchBufferUsage &&
           (this->instructionMemory->SendRequest(
                this->instructionMemoryID, &this->fetchBuffer[i].instruction) ==
            0)) {
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToMemory;
        ++i;
    }

    // Same thing for the predictor.
    i = 0;

    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToPredictor)
        ++i;

    while (i < this->fetchBufferUsage) {
        sinuca::PredictorPacket packet;
        packet.type = sinuca::PredictorPacketTypeRequestQuery;
        packet.data.requestQuery = this->fetchBuffer[i].instruction.staticInfo;
        if (this->predictor->SendRequest(this->predictorID, &packet) != 0)
            break;
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToPredictor;
        ++i;
    }
}

// TODO: this makes the fetcher send one instruction more after a misspredict...
// if we could put the target address in the dynamicInfo it would be so nice.
void Fetcher::ClockCheckPredictor() {
    sinuca::PredictorPacket response;
    unsigned long i = 0;
    // We depend on the predictor sending the responses in order and, of course,
    // sending only what we actually asked for.
    while (this->predictor->ReceiveResponse(this->predictorID, &response) ==
           0) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.response.instruction);
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsPredicted;

        long last = this->lastPrediction;
        this->lastPrediction = response.data.response.target;

        // We check wether the last prediction was successful here because it
        // would be just too complex otherwise. We literally stop the world,
        // letting the remaining of the buffer to be treated when the penalty
        // is paid.
        if (last != 0 &&
            last !=
                this->fetchBuffer[i].instruction.staticInfo->opcodeAddress) {
            this->currentPenalty = this->misspredictPenalty;
            break;
        }
    }
}

void Fetcher::ClockUnbuffer() {}

void Fetcher::ClockRequestFetch() {
    unsigned long fetchBufferByteUsage = 0;
    for (unsigned long i = 0; i < this->fetchBufferUsage; ++i) {
        fetchBufferByteUsage +=
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;
    }

    sinuca::FetchPacket request;
    request.request = fetchSize - fetchBufferByteUsage;
    this->fetch->SendRequest(this->fetchID, &request);
}

void Fetcher::ClockFetch() {
    // We're guaranteed to have space because we asked only enough bytes to fill
    // the buffer. The engine is guaranteed to send only up until that amount,
    // the cycle right after we asked.
    while (
        this->fetch->ReceiveResponse(
            this->fetchID,
            (sinuca::FetchPacket*)&this->fetchBuffer[this->fetchBufferUsage]) ==
        0) {
        ++this->fetchBufferUsage;
    }
}

void Fetcher::Clock() {
    this->ClockSendBuffered();
    this->ClockCheckPredictor();
    this->ClockUnbuffer();
    this->ClockFetch();

    if (this->fetchClock % this->fetchInterval == 0) {
        this->fetchClock = 0;
        this->ClockRequestFetch();
    }

    ++this->fetchClock;
}

void Fetcher::Flush() {}

void Fetcher::PrintStatistics() {}

Fetcher::~Fetcher() {
    if (this->fetchBuffer != NULL) {
        delete[] this->fetchBuffer;
    }
}
