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
#include <sinuca3.hpp>

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
        this->flagsToCheck |= FetchBufferEntryFlagsPredicted;
    }

    return 0;
}

int Fetcher::FetchConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->fetch = dynamic_cast<Component<FetchPacket>*>(
            value.value.componentReference);
        if (this->fetch != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetch` is not a Component<FetchPacket>.\n");
    return 1;
}

int Fetcher::InstructionMemoryConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->instructionMemory = dynamic_cast<Component<InstructionPacket>*>(
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

int Fetcher::FetchSizeConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchSize = value.value.integer;
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchSize` is not a integer > 0.\n");
    return 1;
}

int Fetcher::FetchIntervalConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchInterval = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `fetchInterval` is not a integer > 0.\n");
    return 1;
}

int Fetcher::PredictorConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->predictor = dynamic_cast<Component<PredictorPacket>*>(
            value.value.componentReference);
        if (this->predictor != NULL) return 0;
    }

    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `predictor` is not a "
        "Component<PredictorPacket>.\n");
    return 1;
}

int Fetcher::MisspredictPenaltyConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->misspredictPenalty = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Fetcher parameter `misspredictPenalty` is not a integer > 0.\n");
    return 1;
}

int Fetcher::SetConfigParameter(const char* parameter, ConfigValue value) {
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
    if (this->predictor == NULL) return;

    i = 0;

    while (i < this->fetchBufferUsage &&
           this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToPredictor)
        ++i;

    while (i < this->fetchBufferUsage) {
        PredictorPacket packet;
        packet.type = PredictorPacketTypeRequestQuery;
        packet.data.requestQuery = this->fetchBuffer[i].instruction;
        if (this->predictor->SendRequest(this->predictorID, &packet) != 0)
            break;
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToPredictor;
        ++i;
    }
}

int Fetcher::ClockCheckPredictor() {
    if (this->predictor == NULL) return 0;

    PredictorPacket response;
    unsigned long i = 0;
    // We depend on the predictor sending the responses in order and, of course,
    // sending only what we actually asked for.
    while (this->predictor->ReceiveResponse(this->predictorID, &response) ==
           0) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.targetResponse.instruction.staticInfo);
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsPredicted;
        long target =
            this->fetchBuffer[i].instruction.staticInfo->opcodeAddress +
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;
        // "Redirect" the fetch only if the predictor has an address, otherwise
        // expect the instruction to be at the next logical PC.
        if (response.type == PredictorPacketTypeResponseTakeToAddress) {
            target = response.data.targetResponse.target;
        }
        // If a missprediction happened.
        if (target != this->fetchBuffer[i].instruction.nextInstruction) {
            return 1;
        }
        ++i;
    }

    return 0;
}

void Fetcher::ClockUnbuffer() {
    unsigned long i = 0;
    while (i < this->fetchBufferUsage &&
           ((this->fetchBuffer[i].flags & this->flagsToCheck) ==
            this->flagsToCheck)) {
        ++i;
    }
    this->fetchBufferUsage -= i;
    if (this->fetchBufferUsage > 0) {
        memmove(this->fetchBuffer, &this->fetchBuffer[i],
                sizeof(*this->fetchBuffer) * this->fetchBufferUsage);
    }
}

void Fetcher::ClockRequestFetch() {
    unsigned long fetchBufferByteUsage = 0;
    for (unsigned long i = 0; i < this->fetchBufferUsage; ++i) {
        fetchBufferByteUsage +=
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;
    }

    FetchPacket request;
    request.request = fetchSize - fetchBufferByteUsage;
    this->fetch->SendRequest(this->fetchID, &request);
}

void Fetcher::ClockFetch() {
    // We're guaranteed to have space because we asked only enough bytes to fill
    // the buffer. The engine is guaranteed to send only up until that amount,
    // the cycle right after we asked.
    while (this->fetch->ReceiveResponse(
               this->fetchID,
               (FetchPacket*)&this->fetchBuffer[this->fetchBufferUsage]) == 0) {
        this->fetchBuffer[this->fetchBufferUsage].flags = 0;
        ++this->fetchBufferUsage;
        ++this->fetchedInstructions;
    }
}

void Fetcher::Clock() {
    // If paying a misspredict penalty.
    if (this->currentPenalty > 0) {
        --this->currentPenalty;
        return;
    }

    this->ClockSendBuffered();
    const int predictionResult = this->ClockCheckPredictor();
    this->ClockUnbuffer();

    // Don't fetch if a misspredict happened. The fetchClock is set to 0 so when
    // the missprediction is paid, we start fetching immediatly.
    if (predictionResult != 0) {
        ++this->misspredictions;
        this->currentPenalty = this->misspredictPenalty;
        this->fetchClock = 0;
        return;
    }

    this->ClockFetch();

    if (this->fetchClock % this->fetchInterval == 0) {
        this->fetchClock = 0;
        this->ClockRequestFetch();
    }

    ++this->fetchClock;
}

void Fetcher::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Fetcher %p: %lu fetched instructions.\n", this,
                       this->fetchedInstructions);
    SINUCA3_LOG_PRINTF("Fetcher %p: %lu misspredictions.\n", this,
                       this->misspredictions);
}

Fetcher::~Fetcher() {
    if (this->fetchBuffer != NULL) {
        delete[] this->fetchBuffer;
    }
}
