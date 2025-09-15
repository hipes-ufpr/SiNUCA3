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

#include "gshare_predictor.hpp"

#include <cmath>

#include "config/config.hpp"
#include "engine/default_packets.hpp"
#include "utils/logging.hpp"

GsharePredictor::GsharePredictor()
    : entries(NULL),
      globalBranchHistReg(0),
      numberOfEntries(0),
      numberOfPredictions(0),
      numberOfWrongPredictions(0),
      indexQueueSize(0),
      indexBitsSize(0) {}

GsharePredictor::~GsharePredictor() { this->Deallocate(); }

int GsharePredictor::Allocate() {
    this->entries = new BimodalCounter[this->numberOfEntries];
    if (!this->entries) {
        SINUCA3_ERROR_PRINTF("Gshare failed to allocate table\n");
        return 1;
    }
    this->indexQueue.Allocate(this->indexQueueSize, sizeof(this->currentIndex));
    return 0;
}

void GsharePredictor::Deallocate() {
    if (this->entries) {
        delete this->entries;
        this->entries = NULL;
    }
    this->indexQueue.Deallocate();
}

int GsharePredictor::RoundNumberOfEntries(unsigned long requestedSize) {
    unsigned int bits = (unsigned int)floor(log2(requestedSize));
    if (bits == 0) {
        return 1;
    }
    this->numberOfEntries = 1 << bits;
    this->indexBitsSize = bits;
    return 0;
}

void GsharePredictor::PreparePacket(PredictorPacket* pkt) {
    if (this->wasPredictedToBeTaken) {
        pkt->type = PredictorPacketTypeResponseTake;
    } else {
        pkt->type = PredictorPacketTypeResponseDontTake;
    }
}

int GsharePredictor::EnqueueIndex() {
    bool ret = this->indexQueue.Enqueue(&this->currentIndex);
    SINUCA3_DEBUG_PRINTF("Gshare Enq [%ld]\n", this->currentIndex);
    return ret;
}

int GsharePredictor::DequeueIndex() {
    bool ret = this->indexQueue.Dequeue(&this->currentIndex);
    SINUCA3_DEBUG_PRINTF("Gshare Deq [%ld]\n", this->currentIndex);
    return ret;
}

void GsharePredictor::UpdateEntry() {
    bool pred = this->entries[this->currentIndex].GetPrediction();
    if (pred != this->wasBranchTaken) {
        this->numberOfWrongPredictions++;
    }
    this->entries[this->currentIndex].UpdatePrediction(this->wasBranchTaken);
}

void GsharePredictor::UpdateGlobBranchHistReg() {
    SINUCA3_DEBUG_PRINTF("Gshare gbhr bef [%ld]\n", this->globalBranchHistReg);
    this->globalBranchHistReg <<= 1;
    if (this->wasBranchTaken) {
        this->globalBranchHistReg |= 1;
    }
    SINUCA3_DEBUG_PRINTF("Gshare gbhr aft [%ld]\n", this->globalBranchHistReg);
}

void GsharePredictor::Update() {
    if (this->DequeueIndex()) {
        SINUCA3_ERROR_PRINTF("Gshare table was not updated\n");
        return;
    }
    this->UpdateEntry();
    this->UpdateGlobBranchHistReg();
}

void GsharePredictor::QueryEntry() {
    this->numberOfPredictions++;
    this->wasPredictedToBeTaken =
        this->entries[this->currentIndex].GetPrediction();
}

void GsharePredictor::Query(PredictorPacket* pkt, unsigned long addr) {
    this->CalculateIndex(addr);
    this->QueryEntry();
    this->PreparePacket(pkt);
    if (this->EnqueueIndex()) {
        SINUCA3_WARNING_PRINTF("Gshare index queue full\n");
    }
}

void GsharePredictor::CalculateIndex(unsigned long addr) {
    unsigned long mask = (1 << this->indexBitsSize) - 1;
    this->currentIndex = (this->globalBranchHistReg ^ addr) & mask;
    SINUCA3_DEBUG_PRINTF("Gshare idx [%ld]\n", this->currentIndex);
}

int GsharePredictor::SetConfigParameter(const char* parameter,
                                        ConfigValue value) {
    if (strcmp(parameter, "numberOfEntries") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "Gshare parameter numberOfEntries is an Integer");
            return 1;
        }
        unsigned long requestedNumberOfEntries = value.value.integer;
        if (requestedNumberOfEntries == 0) {
            SINUCA3_ERROR_PRINTF(
                "Gshare parameter numberOfEntries should not be zero\n");
            return 1;
        }
        if (this->RoundNumberOfEntries(requestedNumberOfEntries)) {
            SINUCA3_ERROR_PRINTF(
                "Gshare requested number of entries [%ld] is invalid\n",
                requestedNumberOfEntries);
        }

        return 0;
    }
    if (strcmp(parameter, "indexQueueSize") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "Gshare parameter indexQueueSize is an Integer\n");
            return 1;
        }
        this->indexQueueSize = value.value.integer;
        return 0;
    }
    if (strcmp(parameter, "sendTo")) {
        if (value.type != ConfigValueTypeComponentReference) {
            SINUCA3_ERROR_PRINTF(
                "Gshare parameter sendTo is a Component Reference\n");
            return 1;
        }
        Component<PredictorPacket>* comp =
            dynamic_cast<Component<PredictorPacket>*>(
                value.value.componentReference);
        if (comp == NULL) {
            SINUCA3_ERROR_PRINTF("Gshare got invalid component reference\n");
            return 1;
        }
        this->sendTo = comp;
        return 0;
    }
    SINUCA3_ERROR_PRINTF("Gshare predictor got unkown parameter\n");
    return 1;
}

void GsharePredictor::PrintStatistics() {
    unsigned long percentage =
        this->numberOfWrongPredictions / this->numberOfPredictions;
    SINUCA3_LOG_PRINTF("Gshare table size [%lu] & number of index bits [%u]\n",
                       this->numberOfEntries, this->indexBitsSize);
    SINUCA3_LOG_PRINTF("Gshare number of predictions [%lu]\n",
                       this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("Gshare rate of wrong predictions [%ld]%%\n",
                       percentage);
}

int GsharePredictor::FinishSetup() {
    if (this->numberOfEntries == 0) {
        SINUCA3_ERROR_PRINTF("Gshare has invalid number of entries\n");
        return 1;
    }
    if (this->Allocate()) {
        return 1;
    }
    if (sendTo != NULL) {
        this->sendToId = this->sendTo->Connect(0);
    }
    return 0;
}

void GsharePredictor::Clock() {
    PredictorPacket packet;
    unsigned long addr;
    long totalConnections = this->GetNumberOfConnections();
    for (long i = 0; i < totalConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            if (packet.type == PredictorPacketTypeRequestQuery) {
                addr = packet.data.requestQuery.staticInfo->opcodeAddress;
                this->Query(&packet, addr);
                if (this->sendTo == NULL) {
                    this->SendResponseToConnection(i, &packet);
                } else {
                    this->sendTo->SendRequest(sendToId, &packet);
                }
            }
            if (packet.type == PredictorPacketTypeRequestUpdate) {
                this->wasBranchTaken = packet.data.directionUpdate.taken;
                this->Update();
            }
        }
    }
}

#ifndef NDEBUG
int TestGshare() {
    GsharePredictor predictor;
    PredictorPacket packet[6];
    StaticInstructionInfo ins[2];
    const long addrs[] = {0x1, 0x2};
    const bool taken[] = {false, true};

    ins[0].opcodeAddress = addrs[0];
    ins[1].opcodeAddress = addrs[1];
    packet[0].data.requestQuery.staticInfo = &ins[0];
    packet[1].data.requestQuery.staticInfo = &ins[1];
    packet[0].type = PredictorPacketTypeRequestQuery;
    packet[1].type = PredictorPacketTypeRequestQuery;
    packet[4].type = PredictorPacketTypeRequestUpdate;
    packet[5].type = PredictorPacketTypeRequestUpdate;

    predictor.SetConfigParameter("numberOfEntries", ConfigValue((long)2));
    int id = predictor.Connect(2);

    // clock 1
    predictor.SendRequest(id, &packet[0]);
    predictor.SendRequest(id, &packet[1]);
    predictor.Clock();
    predictor.PosClock();

    // clock 2
    if (predictor.ReceiveResponse(id, &packet[2])) {
        SINUCA3_ERROR_PRINTF("Gshare got unexpected response [1]\n");
        return 1;
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 3
    predictor.ReceiveResponse(id, &packet[2]);
    predictor.ReceiveResponse(id, &packet[3]);
    SINUCA3_DEBUG_PRINTF("Gshare predicted [1 / %d]\n", packet[2].type);
    SINUCA3_DEBUG_PRINTF("Gshare predicted [2 / %d]\n", packet[3].type);
    packet[4].data.directionUpdate.taken = taken[0];
    packet[5].data.directionUpdate.taken = taken[1];
    predictor.SendRequest(id, &packet[4]);
    predictor.SendRequest(id, &packet[5]);
    predictor.Clock();
    predictor.PosClock();

    // clock 4
    predictor.SendRequest(id, &packet[0]);
    predictor.SendRequest(id, &packet[1]);
    predictor.Clock();
    predictor.PosClock();

    // clock 5
    if (predictor.ReceiveResponse(id, &packet[2])) {
        SINUCA3_ERROR_PRINTF("Gshare got unexpected response [2]\n");
        return 1;
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 6
    predictor.ReceiveResponse(id, &packet[2]);
    SINUCA3_DEBUG_PRINTF("Gshare predicted [%d] for [%ld] addr\n",
                         packet[2].type, addrs[0]);
    predictor.ReceiveResponse(id, &packet[3]);
    SINUCA3_DEBUG_PRINTF("Gshare predicted [%d] for [%ld] addr\n",
                         packet[3].type, addrs[1]);

    predictor.PrintStatistics();

    return 0;
}
#endif