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

int BoomFetch::BTBConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->btb =
            dynamic_cast<BranchTargetBuffer*>(value.value.componentReference);
        if (this->btb != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF(
        "Boom Fetch parameter `btb` is not a "
        "BranchTargetBuffer.\n");
    return 1;
}

int BoomFetch::RASConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->ras = dynamic_cast<Ras*>(value.value.componentReference);
        if (this->ras != NULL) {
            return 0;
        }
    }
    SINUCA3_ERROR_PRINTF("Boom Fetch parameter `ras` is not a Ras.\n");
    return 1;
}

int BoomFetch::PredictorConfigParameter(ConfigValue value) {
    if (value.type == ConfigValueTypeComponentReference) {
        this->preditor = dynamic_cast<Component<PredictorPacket>*>(
            value.value.componentReference);
        if (this->preditor != NULL) {
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
    if (strcmp(parameter, "fetch") == 0) {
        return this->FetchConfigParameter(value);
    } else if (strcmp(parameter, "instructionMemory") == 0) {
        return this->InstructionMemoryConfigParameter(value);
    } else if (strcmp(parameter, "fetchSize") == 0) {
        return this->FetchSizeConfigParameter(value);
    } else if (strcmp(parameter, "fetchInterval") == 0) {
        return this->FetchIntervalConfigParameter(value);
    } else if (strcmp(parameter, "BranchTargetBuffer") == 0) {
        return this->BTBConfigParameter(value);
    } else if (strcmp(parameter, "Ras") == 0) {
        return this->RASConfigParameter(value);
    } else if (strcmp(parameter, "predictor") == 0) {
        return this->PredictorConfigParameter(value);
    } else if (strcmp(parameter, "misspredictPenalty") == 0) {
        return this->MisspredictPenaltyConfigParameter(value);
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
    this->predictorID = this->preditor->Connect(this->fetchSize);
    this->btbID = this->btb->Connect(this->fetchSize);
    this->rasID = this->ras->Connect(this->fetchSize);

    this->fetchBuffer = new FetchBufferEntry[this->fetchSize];

    return 0;
}

void BoomFetch::ClockSendBuffered() {
    unsigned int i = 0;

    // Skip instructions we already sent.
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToMemory) ++i;

    while ((i < this->fetchBufferUsage) &&
           (this->instructionMemory->SendRequest(
                this->instructionMemoryID, &this->fetchBuffer[i].instruction) ==
            0)) {
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToMemory;
        ++i;
    }

    i = 0;
    while (this->fetchBuffer[i].flags & FetchBufferEntryFlagsSentToPredictor)
        ++i;

    while (i < this->fetchBufferUsage) {
        PredictorPacket predictorPacket;
        predictorPacket.type = PredictorPacketTypeRequestQuery;
        predictorPacket.data.requestQuery = this->fetchBuffer[i].instruction;
        if (this->preditor->SendRequest(this->predictorID, &predictorPacket) !=
            0) {
            break;
        }
        this->fetchBuffer[i].flags |= FetchBufferEntryFlagsSentToPredictor;
        ++i;
    }
}

void BoomFetch::Clock() {
    // If paying a misspredict penalty
    if (this->currentPenalty > 0) {
        --this->currentPenalty;
        return;
    }

    this->ClockSendBuffered();
}

void BoomFetch::Flush() {
    // Implementation of Flush
}

void BoomFetch::PrintStatistics() {
    // Implementation of PrintStatistics
}

BoomFetch::~BoomFetch() {
    // Implementation of destructor
}
