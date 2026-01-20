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

int Fetcher::Configure(Config config) {
    if (config.ComponentReference("fetch", &this->fetch, true)) return 1;
    if (config.ComponentReference("instructionMemory", &this->instructionMemory,
                                  true))
        return 1;
    if (config.ComponentReference("predictor", &this->predictor)) return 1;

    long fetchSize = 1;
    if (config.Integer("fetchSize", &fetchSize)) return 1;
    if (fetchSize <= 0) return config.Error("fetchSize", "is not > 0.");
    this->fetchSize = fetchSize;

    long fetchInterval = 1;
    if (config.Integer("fetchInterval", &fetchInterval)) return 1;
    if (fetchInterval <= 0) return config.Error("fetchInterval", "is not > 0.");
    this->fetchInterval = fetchInterval;

    long misspredictPenalty = 0;
    if (config.Integer("misspredictPenalty", &misspredictPenalty)) return 1;
    if (misspredictPenalty <= 0)
        return config.Error("misspredictPenalty", "is not > 0.");
    this->misspredictPenalty = misspredictPenalty;

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

void Fetcher::ClockSendBuffered() {
    unsigned long i = 0;

    // Skip instructions we already sent.
    while (i < this->fetchBufferUsage &&
           (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToMemory))
        ++i;

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
    int ret = 0;

    bool cont =
        this->predictor->ReceiveResponse(this->predictorID, &response) == 0;
    if (!cont) return 1;
    // We depend on the predictor sending the responses in order and, of course,
    // sending only what we actually asked for.
    while (cont) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.targetResponse.instruction.staticInfo);
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsPredicted;
        unsigned long target =
            this->fetchBuffer[i].instruction.staticInfo->instAddress +
            this->fetchBuffer[i].instruction.staticInfo->instSize;
        // "Redirect" the fetch only if the predictor has an address, otherwise
        // expect the instruction to be at the next logical PC.
        if (response.type == PredictorPacketTypeResponseTakeToAddress) {
            target = response.data.targetResponse.target;
        }
        // If a missprediction happened.
        if (target != this->fetchBuffer[i].instruction.nextInstruction) {
            ret = 1;
        }
        ++i;
        cont =
            this->predictor->ReceiveResponse(this->predictorID, &response) == 0;
    }

    return ret;
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
            this->fetchBuffer[i].instruction.staticInfo->instSize;
    }

    FetchPacket request;
    request.request = this->fetchSize - fetchBufferByteUsage;
    this->fetch->SendRequest(this->fetchID, &request);
}

void Fetcher::ClockFetch() {
    // We're guaranteed to have space because we asked only enough bytes to fill
    // the buffer. The engine is guaranteed to send only up until that amount,
    // the cycle right after we asked.
    while (this->fetch->ReceiveResponse(
               this->fetchID,
               (FetchPacket*)&this->fetchBuffer[this->fetchBufferUsage]
                   .instruction) == 0) {
        this->fetchBuffer[this->fetchBufferUsage].flags = 0;
        ++this->fetchBufferUsage;
        ++this->fetchedInstructions;
    }
}

void Fetcher::Clock() {
    this->ClockSendBuffered();
    const int predictionResult = this->ClockCheckPredictor();
    this->ClockUnbuffer();

    bool forceFetch = false;
    // If paying a misspredict penalty.
    if (this->currentPenalty > 0) {
        --this->currentPenalty;
        // In the last three cycles of paying the prediction, we need to force
        // fetching new instructions, so they arrive in the last one and we can
        // buffer them.
        if (this->currentPenalty > 2) return;
        forceFetch = true;
    }

    // Don't fetch if a misspredict happened. The fetchClock is set to 0 so when
    // the missprediction is paid, we start fetching immediatly.
    if (!forceFetch && predictionResult != 0) {
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
