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

#include "config/config.hpp"
#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include "std_components/predictors/interleavedBTB.hpp"
#include "utils/logging.hpp"

int BoomFetch::FetchConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->fetch = dynamic_cast<Component<FetchPacket>*>(
            value.value.componentReference);
        if (this->fetch != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `fetch` is not a Component<FetchPacket>.\n");
    return 1;
}

int BoomFetch::InstructionMemoryConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->instructionMemory = dynamic_cast<Component<InstructionPacket>*>(
            value.value.componentReference);
        if (this->instructionMemory != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `instructionMemory` is not a "
        "Component<InstructionPacket>.\n");
    return 1;
}

int BoomFetch::PredictorConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->predictor = dynamic_cast<Component<PredictorPacket>*>(
            value.value.componentReference);
        if (this->predictor != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `predictor` is not a "
        "Component<PredictorPacket>.\n");

    return 1;
}

int BoomFetch::FetchSizeConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchSize = value.value.integer;
            return 0;
        }
    }

    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `fetchSize` is not a integer > 0.\n");
    return 1;
}

int BoomFetch::FetchIntervalConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->fetchInterval = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `fetchInterval` is not a integer > 0.\n");
    return 1;
}

int BoomFetch::MisspredictPenaltyConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeInteger) {
        if (value.value.integer > 0) {
            this->misspredictPenalty = value.value.integer;
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `misspredictPenalty` is not a integer > 0.\n");
    return 1;
}

int BoomFetch::SetConfigParameter(const char* parameter, ConfigValue value) {
    if (this->btb == NULL) {
        this->btb = new BranchTargetBuffer;
    }

    if (this->ras == NULL) {
        this->ras = new Ras;
    }

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
    } else if (strcmp(parameter, "interleavingFactor") == 0) {
        return this->btb->SetConfigParameter(parameter, value);
    } else if (strcmp(parameter, "numberOfEntries") == 0) {
        return this->btb->SetConfigParameter(parameter, value);
    } else if (strcmp(parameter, "size") == 0) {
        return this->ras->SetConfigParameter(parameter, value);
    }

    SINUCA3_ERROR_PRINTF("Boom Fetch received unknown parameter %s.\n",
                         parameter);

    return 1;
}

int BoomFetch::FinishSetup() {
    if (this->fetch == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Boom Fetch didn't received required parameter `fetch`.\n");
        return 1;
    }

    if (this->instructionMemory == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Boom Fetch didn't received required parameter "
            "`instructionMemory`.\n");
        return 1;
    }

    if (this->btb == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Boom Fetch didn't received required parameter `btb`.\n");
        return 1;
    }

    if (this->ras == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Boom Fetch didn't received required parameter `ras`.\n");
        return 1;
    }

    this->fetchID = this->fetch->Connect(this->fetchSize);
    this->instructionMemoryID =
        this->instructionMemory->Connect(this->fetchSize);
    this->predictorID = this->predictor->Connect(this->fetchSize);
    this->btbID = this->btb->Connect(this->fetchSize);
    this->rasID = this->ras->Connect(this->fetchSize);

    this->fetchBuffer = new FetchBufferEntry[this->fetchSize];

    if (this->btb->FinishSetup()) {
        return 1;
    }

    if (this->ras->FinishSetup()) {
        return 1;
    }

    return 0;
}

void BoomFetch::ClockSendBuffered() {
    unsigned int i;

    // Skip instructions we already sent.
    i = 0;
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToMemory) ++i;

    while ((i < this->fetchBufferUsage) &&
           (this->instructionMemory->SendRequest(
                this->instructionMemoryID, &this->fetchBuffer[i].instruction) ==
            0)) {
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToMemory;
        ++i;
    }

    // Skip instructions we already sent.
    i = 0;
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToPredictor)
        ++i;

    while (i < this->fetchBufferUsage) {
        PredictorPacket predictorPacket;
        predictorPacket.type = PredictorPacketTypeRequestQuery;
        predictorPacket.data.requestQuery = this->fetchBuffer[i].instruction;
        if (this->predictor->SendRequest(this->predictorID, &predictorPacket) !=
            0) {
            break;
        }
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToPredictor;
        ++i;
    }

    // Skip instructions we already sent.
    i = 0;
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToBTB) {
        ++i;
    }

    if (i < this->fetchBufferUsage) {
        BTBPacket btbPacket;
        btbPacket.type = BTBPacketTypeRequestQuery;
        btbPacket.data.requestQuery =
            this->fetchBuffer[i].instruction.staticInfo;

        if (this->btb->SendRequest(this->btbID, &btbPacket) == 0) {
            this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToBTB;
        }
    }

    // Skip instructions we already sent.
    i = 0;
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToRAS) {
        ++i;
    }

    while (i < this->fetchBufferUsage) {
        if (this->fetchBuffer[i].instruction.staticInfo->branchType ==
            BranchCall) {
            PredictorPacket predictorPacket;

            /* Fills the packet with the instruction and target info. */
            /* We found a call instruction and want to insert its target into
             * the ras */
            predictorPacket.data.requestUpdate.instruction =
                this->fetchBuffer[i].instruction;
            predictorPacket.data.requestUpdate.target =
                this->fetchBuffer[i].instruction.nextInstruction;
            predictorPacket.type = PredictorPacketTypeRequestUpdate;

            if (this->ras->SendRequest(this->rasID, &predictorPacket) != 0) {
                break;
            }
            this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToRAS;

        } else if (this->fetchBuffer[i].instruction.staticInfo->branchType ==
                   BranchReturn) {
            PredictorPacket predictorPacket;

            /* We found a return statement, so we unstacked the most recent
             * target from the ras. */
            predictorPacket.data.requestQuery =
                this->fetchBuffer[i].instruction;
            predictorPacket.type = PredictorPacketTypeRequestQuery;

            if (this->ras->SendRequest(this->rasID, &predictorPacket) != 0) {
                break;
            }
            this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToRAS;
        }
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToRAS;
        i++;
    }
}

int BoomFetch::ClockCheckPredictor() {
    if (this->predictor == NULL) return 0;

    PredictorPacket response;
    unsigned long i = 0;

    /*We depend on the predictor sending the responses in order and, of course,
     * sending only what we actually asked for. */
    while (this->predictor->ReceiveResponse(this->predictorID, &response) ==
           0) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.response.instruction.staticInfo);
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToPredictor;
        long target =
            this->fetchBuffer[i].instruction.staticInfo->opcodeAddress +
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;

        // "Redirect" the fetch only if the predictor has an address, otherwise
        // expect the instruction to be at the next logical PC.
        if (response.type == PredictorPacketTypeResponseTakeToAddress) {
            target = response.data.response.target;
        }

        // If a missprediction happened.
        if (target != this->fetchBuffer[i].instruction.nextInstruction) {
            return 1;
        }
        ++i;
    }

    return 0;
}

int BoomFetch::ClockCheckBTB() {
    int i = 0;
    if (this->btb == NULL) return 0;

    BTBPacket response;

    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToBTB) {
        ++i;
    }

    if (this->btb->ReceiveResponse(this->btbID, &response) == 0) {
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.instruction);
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsBTBPredicted;
        long target =
            this->fetchBuffer[i].instruction.staticInfo->opcodeAddress +
            this->fetchSize;

        if (response.type == BTBPacketTypeResponseBTBHit) {
            target = response.data.response.target;
        }

        i = 0;
        while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToMemory)
            ++i;

        if (target != this->fetchBuffer[i].instruction.nextInstruction) {
            return 1;
        }
    }

    return 0;
}

void BoomFetch::ClockUnbuffer() {
    unsigned long i = 0;
    while (i < this->fetchBufferUsage &&
           ((this->fetchBuffer[i].flags & this->flagsToCheck) ==
            this->flagsToCheck)) {
        ++i;
    }

    /* Moves the instructions to start */
    this->fetchBufferUsage -= i;
    if (this->fetchBufferUsage > 0) {
        memmove(this->fetchBuffer, &this->fetchBuffer[i],
                sizeof(*this->fetchBuffer) * this->fetchBufferUsage);
    }
}

void BoomFetch::Clock() {
    // If paying a misspredict penalty
    if (this->currentPenalty > 0) {
        --this->currentPenalty;
        return;
    }

    this->ClockSendBuffered();
    const int predictionResult = this->ClockCheckPredictor();

    if (predictionResult != 0) {
        ++this->misspredictions;
        this->currentPenalty = this->misspredictPenalty;
        this->fetchClock = 0;
    }
}

void BoomFetch::Flush() {
    // Implementation of Flush
}

void BoomFetch::PrintStatistics() {
    // Implementation of PrintStatistics
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
