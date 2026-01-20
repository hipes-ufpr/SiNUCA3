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
 * @file boom_fetch.cpp
 * @brief Implementation of the Boom Fetch component.
 */

#include "boom_fetch.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <sinuca3.hpp>
#include <std_components/predictors/interleavedBTB.hpp>

#include "engine/default_packets.hpp"
#include "utils/map.hpp"

int BoomFetch::Configure(Config config) {
    this->btb = new BranchTargetBuffer;
    this->ras = new Ras;

    if (config.Integer("misspredictPenalty", (long*)&this->misspredictPenalty,
                       true))
        return 1;
    if (this->misspredictPenalty <= 0)
        return config.Error("misspredictPenalty", "not > 0");

    if (config.Integer("fetchInterval", (long*)&this->fetchInterval)) return 1;
    if (this->fetchInterval <= 0)
        return config.Error("fetchInterval", "not > 0");

    if (config.Integer("fetchSize", (long*)&this->fetchSize)) return 1;
    if (this->fetchSize <= 0) return config.Error("fetchSize", "not > 0");

    if (config.ComponentReference("predictor", &this->predictor, true))
        return 1;
    if (config.ComponentReference("instructionMemory", &this->instructionMemory,
                                  true))
        return 1;
    if (config.ComponentReference("fetch", &this->fetch, true)) return 1;

    Map<yaml::YamlValue>* yaml = config.RawYaml();
    yaml::YamlValue* btbConfigYaml = yaml->Get("btb");
    yaml::YamlValue* rasConfigYaml = yaml->Get("ras");

    Config btbConfig;
    Config rasConfig;
    if (config.Fork(btbConfigYaml, &btbConfig))
        return config.Error("btb", "not a mapping");
    if (config.Fork(rasConfigYaml, &rasConfig))
        return config.Error("ras", "not a mapping");
    if (btb->Configure(btbConfig)) return 1;
    if (ras->Configure(rasConfig)) return 1;

    this->fetchID = this->fetch->Connect(this->fetchSize);
    this->instructionMemoryID =
        this->instructionMemory->Connect(this->fetchSize);
    this->predictorID = this->predictor->Connect(this->fetchSize);
    this->btbID = this->btb->Connect(this->fetchSize);
    this->rasID = this->ras->Connect(this->fetchSize);

    this->fetchBuffer = new BoomFetchBufferEntry[this->fetchSize];

    this->flagsToCheck = BoomFetchBufferEntryFlagsSentToMemory |
                         BoomFetchBufferEntryFlagsPredictorCheck;

    return 0;
}

void binprintf(int v) {
    unsigned int mask = 1 << ((sizeof(int) << 3) - 1);
    while (mask) {
        printf("%d", (v & mask ? 1 : 0));
        mask >>= 1;
    }
    printf("\n");
}

bool BoomFetch::SentToRas(unsigned long i) {
    PredictorPacket rasPacket;
    if (this->fetchBuffer[i].instruction.staticInfo->branchType == BranchCall) {
        /* Fills the packet with the instruction and target info. */
        /*
         * We found a call instruction and want to insert its target into
         * the ras.
         */
        rasPacket.type = PredictorPacketTypeRequestTargetUpdate;
        rasPacket.data.targetUpdate.instruction =
            this->fetchBuffer[i].instruction;
        rasPacket.data.targetUpdate.target =
            this->fetchBuffer[i].instruction.nextInstruction;
        this->ras->SendRequest(this->rasID, &rasPacket);

        return true;
    } else if (this->fetchBuffer[i].instruction.staticInfo->branchType ==
               BranchRet) {
        /*
         * We found a return statement, so we unstacked the most recent
         * target from the ras.
         */
        rasPacket.type = PredictorPacketTypeRequestQuery;
        rasPacket.data.requestQuery = this->fetchBuffer[i].instruction;
        this->ras->SendRequest(this->rasID, &rasPacket);

        return true;
    }

    return false;
}

bool BoomFetch::SentToBTB(unsigned long i) {
    BTBPacket btbPacket;

    if (this->fetchBuffer[i].instruction.staticInfo->branchType != BranchNone) {
        btbPacket.type = BTBPacketTypeRequestQuery;
        btbPacket.data.requestQuery =
            this->fetchBuffer[i].instruction.staticInfo;
        this->btb->SendRequest(this->btbID, &btbPacket);
        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToBTB;

        return true;
    }

    return false;
}

void BoomFetch::ClockSendBuffered() {
    unsigned long i;
    bool instMemAvailable;
    bool predictorAvailable;
    bool btbAvailable;
    bool rasAvailable;

    PredictorPacket predictorPacket;

    /* Skip instructions we already sent. */
    i = 0;
    while (this->fetchBuffer[i].flags & BoomFetchBufferEntryFlagsSentToMemory)
        ++i;

    while (i < this->fetchBufferUsage) {
        instMemAvailable = this->instructionMemory->IsComponentAvailable(
            this->instructionMemoryID);

        predictorAvailable =
            this->predictor->IsComponentAvailable(this->predictorID);

        rasAvailable = this->ras->IsComponentAvailable(this->rasID);

        btbAvailable = this->btb->IsComponentAvailable(this->btbID);

        if (!(instMemAvailable) || !(predictorAvailable) || !(rasAvailable) ||
            !(btbAvailable)) {
            break;
        }

        this->instructionMemory->SendRequest(this->instructionMemoryID,
                                             &this->fetchBuffer[i].instruction);

        predictorPacket.type = PredictorPacketTypeRequestQuery;
        predictorPacket.data.requestQuery = this->fetchBuffer[i].instruction;
        this->predictor->SendRequest(this->predictorID, &predictorPacket);

        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToMemory;
        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToPredictor;

        if (this->SentToRas(i)) {
            this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToRas;
        }

        if (this->SentToBTB(i)) {
            this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToBTB;
        }

        i++;
    }
}

int BoomFetch::ClockCheckPredictor() {
    if (this->predictor == NULL) return 0;

    PredictorPacket response;
    unsigned long i = 0;

    while (this->fetchBuffer[i].flags & BoomFetchBufferEntryFlagsPredictorCheck)
        ++i;
    /*
     * We depend on the predictor sending the responses in order and, of course,
     * sending only what we actually asked for.
     */
    while (this->predictor->ReceiveResponse(this->predictorID, &response) ==
           0) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.targetResponse.instruction.staticInfo);
        unsigned long target =
            this->fetchBuffer[i].instruction.staticInfo->instAddress +
            this->fetchBuffer[i].instruction.staticInfo->instSize;

        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsPredictorCheck;

        /*
         * "Redirect" the fetch only if the predictor has an address, otherwise
         * expect the instruction to be at the next logical PC.
         */
        if (response.type == PredictorPacketTypeResponseTakeToAddress) {
            target = response.data.targetResponse.target;
        }

        /* If a missprediction happened. */
        if (target != this->fetchBuffer[i].instruction.nextInstruction) {
            return 1;
        }

        ++i;
    }

    return 0;
}

int BoomFetch::ClockCheckRas() {
    if (this->ras == NULL) return 0;

    unsigned long target = 0;
    PredictorPacket response;

    this->ras->Clock();
    this->ras->PosClock();

    while (this->ras->ReceiveResponse(this->rasID, &response) == 0) {
        target = response.data.targetResponse.target;

        /* The return address does not match the next address */
        if (response.data.targetResponse.instruction.nextInstruction !=
            target) {
            return 1;
        }
    }

    return 0;
}

int BoomFetch::ClockCheckBTB() {
    if (this->btb == NULL) return 0;

    BTBPacket response;
    BTBPacket updateRequest;

    bool btbAvailable, taken;
    unsigned int i, next;

    this->btb->Clock();
    this->btb->PosClock();

    next = 0;
    while (this->btb->ReceiveResponse(this->btbID, &response) == 0) {
        i = 0;
        while (
            ((this->fetchBuffer[i].flags &
              BoomFetchBufferEntryFlagsSentToBTB) !=
             BoomFetchBufferEntryFlagsSentToBTB) ||
            (this->fetchBuffer[i].flags & BoomFetchBufferEntryFlagsBTBCheck) ==
                BoomFetchBufferEntryFlagsBTBCheck) {
            ++i;
        }

        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsBTBCheck;

        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.response.instruction);

        btbAvailable = this->btb->IsComponentAvailable(this->btbID);

        next = this->fetchBuffer[i].instruction.nextInstruction;

        if (response.type == BTBPacketTypeResponseBTBHit) {
            updateRequest.type = BTBPacketTypeRequestUpdate;
            updateRequest.data.requestUpdate.instruction =
                this->fetchBuffer[i].instruction.staticInfo;

            taken = (next !=
                     this->fetchBuffer[i].instruction.staticInfo->instAddress +
                         this->fetchBuffer[i].instruction.staticInfo->instSize);

            updateRequest.data.requestUpdate.branchState = taken;

            if (btbAvailable)
                this->btb->SendRequest(this->btbID, &updateRequest);

            if (next != response.data.response.target) {
                return 1;
            }
        } else {
            updateRequest.type = BTBPacketTypeRequestAddEntry;
            updateRequest.data.requestAddEntry.instruction =
                this->fetchBuffer[i].instruction.staticInfo;
            updateRequest.data.requestAddEntry.target =
                this->fetchBuffer[i].instruction.nextInstruction;

            if (btbAvailable)
                this->btb->SendRequest(this->btbID, &updateRequest);

            if (next != response.data.response.target) {
                return 1;
            }
        }
    }

    return 0;
}

void BoomFetch::ClockUnbuffer() {
    unsigned long i = 0;

    while (i < this->fetchBufferUsage &&
           ((this->fetchBuffer[i].flags & this->flagsToCheck) ==
            this->flagsToCheck)) {
        if (((this->fetchBuffer[i].flags &
              BoomFetchBufferEntryFlagsSentToBTB) ==
             BoomFetchBufferEntryFlagsSentToBTB) &&
            ((this->fetchBuffer[i].flags & BoomFetchBufferEntryFlagsBTBCheck) !=
             BoomFetchBufferEntryFlagsBTBCheck))
            break;
        ++i;
    }

    /* Moves the instructions to start */
    this->fetchBufferUsage -= i;
    if (this->fetchBufferUsage > 0) {
        memmove(this->fetchBuffer, &this->fetchBuffer[i],
                sizeof(*this->fetchBuffer) * this->fetchBufferUsage);
    }
}

void BoomFetch::ClockRequestFetch() {
    unsigned long fetchBufferByteUsage = 0;
    for (unsigned long i = 0; i < this->fetchBufferUsage; ++i) {
        fetchBufferByteUsage +=
            this->fetchBuffer[i].instruction.staticInfo->instSize;
    }

    FetchPacket request;
    request.request = this->fetchSize - fetchBufferByteUsage;
    this->fetch->SendRequest(this->fetchID, &request);
}

void BoomFetch::ClockFetch() {
    while (this->fetch->ReceiveResponse(
               this->fetchID,
               (FetchPacket*)&this->fetchBuffer[this->fetchBufferUsage]) == 0) {
        this->fetchBuffer[this->fetchBufferUsage].flags = 0;
        ++this->fetchBufferUsage;
        ++this->fetchedInstructions;
    }
}

void BoomFetch::Clock() {
    this->ClockSendBuffered();
    const int predictionResult = this->ClockCheckPredictor();
    const int rasResult = this->ClockCheckRas();
    const int btbResult = this->ClockCheckBTB();
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
    if (!forceFetch && predictionResult == 0 && rasResult == 0 &&
        btbResult == 0) {
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

void BoomFetch::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Boom Fetch [%p]\n", this);
    this->btb->PrintStatistics();
    this->ras->PrintStatistics();
}

BoomFetch::~BoomFetch() {
    if (this->fetchBuffer != NULL) {
        delete[] this->fetchBuffer;
    }

    if (this->btb != NULL) {
        delete this->btb;
    }

    if (this->ras != NULL) {
        delete this->ras;
    }
}
