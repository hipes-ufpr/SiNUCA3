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

void binprintf(int v) {
    unsigned int mask = 1 << ((sizeof(int) << 3) - 1);
    while (mask) {
        printf("%d", (v & mask ? 1 : 0));
        mask >>= 1;
    }
    printf("\n");
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

    this->fetchBuffer = new BoomFetchBufferEntry[this->fetchSize];

    this->flagsToCheck = BoomFetchBufferEntryFlagsSentToMemory;
    this->flagsToCheck |= BoomFetchBufferEntryFlagsPredictorCheck;

    if (this->btb->FinishSetup()) {
        return 1;
    }

    if (this->ras->FinishSetup()) {
        return 1;
    }

    return 0;
}

bool BoomFetch::SendToRas(unsigned long i) {
    PredictorPacket rasPacket;
    if (this->fetchBuffer[i].instruction.staticInfo->branchType == BranchCall) {
        /* Fills the packet with the instruction and target info. */
        /*
         * We found a call instruction and want to insert its target into
         * the ras.
         */
        rasPacket.type = PredictorPacketTypeRequestUpdate;
        rasPacket.data.requestUpdate.instruction =
            this->fetchBuffer[i].instruction;
        rasPacket.data.requestUpdate.target =
            this->fetchBuffer[i].instruction.nextInstruction;
        this->ras->SendRequest(this->rasID, &rasPacket);

        return true;
    } else if (this->fetchBuffer[i].instruction.staticInfo->branchType ==
               BranchReturn) {
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

void BoomFetch::ClockSendBuffered() {
    unsigned long i;
    bool instMemAvailable;
    bool predictorAvailable;
    bool btbAvailable;
    bool rasAvailable;

    // BTBPacket btbPacket;
    PredictorPacket predictorPacket;

    /* Skip instructions we already sent. */
    i = 0;
    while (this->fetchBuffer[i].flags & BoomFetchBufferEntryFlagsSentToMemory)
        ++i;

    btbAvailable = this->btb->IsComponentAvailable(this->btbID);

    if (!(btbAvailable)) return;

    //    if (i < this->fetchBufferUsage) {
    //        btbPacket.type = BTBPacketTypeRequestQuery;
    //        btbPacket.data.requestQuery =
    //            this->fetchBuffer[i].instruction.staticInfo;
    //        this->btb->SendRequest(this->btbID, &btbPacket);
    //        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSendToBTB;
    //    }

    while (i < this->fetchBufferUsage) {
        instMemAvailable = this->instructionMemory->IsComponentAvailable(
            this->instructionMemoryID);

        predictorAvailable =
            this->predictor->IsComponentAvailable(this->predictorID);

        rasAvailable = this->ras->IsComponentAvailable(this->rasID);

        if (!(instMemAvailable) || !(predictorAvailable) || !(rasAvailable)) {
            break;
        }

        this->instructionMemory->SendRequest(this->instructionMemoryID,
                                             &this->fetchBuffer[i].instruction);

        predictorPacket.type = PredictorPacketTypeRequestQuery;
        predictorPacket.data.requestQuery = this->fetchBuffer[i].instruction;
        this->predictor->SendRequest(this->predictorID, &predictorPacket);

        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToMemory;
        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToPredictor;

        if (this->SendToRas(i)) {
            this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsSentToRas;
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
               response.data.response.instruction.staticInfo);
        unsigned long target =
            this->fetchBuffer[i].instruction.staticInfo->opcodeAddress +
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;

        this->fetchBuffer[i].flags |= BoomFetchBufferEntryFlagsPredictorCheck;

        /*
         * "Redirect" the fetch only if the predictor has an address, otherwise
         * expect the instruction to be at the next logical PC.
         */
        if (response.type == PredictorPacketTypeResponseTakeToAddress) {
            target = response.data.response.target;
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

    unsigned long target;
    PredictorPacket response;

    this->ras->Clock();
    this->ras->PosClock();

    while (this->ras->ReceiveResponse(this->rasID, &response) == 0) {
        target = response.data.response.target;

        /* The return address does not match the next address */
        if (response.data.response.instruction.nextInstruction != target) {
            return 1;
        }
    }

    return 0;
}

int BoomFetch::ClockCheckBTB() {
    if (this->btb == NULL) return 0;

    BTBPacket response;
    bool validResponse, isNext;
    unsigned int i, index, nextInstruction, targetBlock, miss;

    this->btb->Clock();

    miss = 0;
    if (this->btb->ReceiveResponse(this->btbID, &response) == 0) {
        i = 0;
        while (this->fetchBuffer[i].flags &
               BoomFetchBufferEntryFlagsSentToMemory)
            ++i;
        assert(this->fetchBuffer[i].instruction.staticInfo ==
               response.data.response.instruction);

        index = i;

        /* Checks whether the instructions actually executed as intended */
        for (i = 1; i < response.data.response.numberOfInstructions; ++i) {
            validResponse = response.data.response.validBits[i];

            nextInstruction = this->fetchBuffer[index + (i - 1)]
                                  .instruction.staticInfo->opcodeAddress +
                              this->fetchBuffer[index + (i - 1)]
                                  .instruction.staticInfo->opcodeSize;

            isNext =
                nextInstruction == this->fetchBuffer[index + i]
                                       .instruction.staticInfo->opcodeAddress;

            if (validResponse != isNext) {
                /* In this case, the prediction was wrong. */
                miss = 1;
                if (validResponse) {
                    /*
                     * If it was valid, the previous instruction (branch) was
                     * executed and the current instruction contains the jump
                     * destination address.
                     */
                    targetBlock = this->fetchBuffer[index + i]
                                      .instruction.staticInfo->opcodeAddress;
                } else {
                    /*
                     * If it was not valid, the previous instruction (branch)
                     * was considered to have been taken, and the address of the
                     * next block is given by the base address + the number of
                     * instructions per block.
                     */
                    targetBlock =
                        ((this->fetchBuffer[index]
                              .instruction.staticInfo->opcodeAddress >>
                          response.data.response.interleavingBits)
                         << response.data.response.interleavingBits) +
                        response.data.response.numberOfInstructions;
                }

                BTBPacket updateRequest;

                if (response.data.response.isInBTB) {
                    updateRequest.data.requestUpdate.instruction =
                        this->fetchBuffer[index + (i - 1)]
                            .instruction.staticInfo;

                    updateRequest.data.requestUpdate.branchState =
                        validResponse;

                    updateRequest.type = BTBPacketTypeRequestUpdate;
                } else {
                    updateRequest.data.requestAddEntry.instruction =
                        this->fetchBuffer[index + (i - 1)]
                            .instruction.staticInfo;

                    updateRequest.data.requestAddEntry.target = targetBlock;

                    updateRequest.type = BTBPacketTypeRequestAddEntry;
                }

                if (this->btb->SendRequest(this->btbID, &updateRequest) != 0) {
                    break;
                }
            } else {
                /* The prediction was correct. */
                BTBPacket updateRequest;

                updateRequest.data.requestUpdate.instruction =
                    this->fetchBuffer[index + (i - 1)].instruction.staticInfo;
                /*
                 * If it was taken, the validity bit of the next sequential
                 * instruction is 0. If it was not taken, the validity bit of
                 * the next sequential instruction is 1.
                 */
                updateRequest.data.requestUpdate.branchState = (!validResponse);
                updateRequest.type = BTBPacketTypeRequestUpdate;

                if (this->btb->SendRequest(this->btbID, &updateRequest) != 0) {
                    break;
                }
            }
        }
    }

    return miss;
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

void BoomFetch::ClockRequestFetch() {
    unsigned long fetchBufferByteUsage = 0;
    for (unsigned long i = 0; i < this->fetchBufferUsage; ++i) {
        fetchBufferByteUsage +=
            this->fetchBuffer[i].instruction.staticInfo->opcodeSize;
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
    if (this->currentPenalty > 0) {
        --this->currentPenalty;
        return;
    }

    this->ClockSendBuffered();
    const int predictionResult = this->ClockCheckPredictor();
    const int rasResult = this->ClockCheckRas();
    // const int btbResult = this->ClockCheckBTB();
    this->ClockUnbuffer();

    if ((predictionResult != 0) || (rasResult != 0))
    //|| (btbResult != 0))
    {
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
