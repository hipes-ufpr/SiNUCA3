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

#include <cassert>
#include <cmath>
#include <sinuca3.hpp>

GsharePredictor::GsharePredictor()
    : entries(NULL),
      globalBranchHistReg(0),
      numberOfEntries(0),
      numberOfPredictions(0),
      numberOfWrongPredictions(0),
      indexQueueSize(0),
      indexBitsSize(0),
      sendTo(NULL) {}

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
        delete[] this->entries;
        this->entries = NULL;
    }
    if (!this->indexQueue.IsEmpty()) {
        SINUCA3_WARNING_PRINTF(
            "Gshare index queue not empty when it was expected to be\n");
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
    SINUCA3_DEBUG_PRINTF("Gshare Gbhr Bef [%ld]\n", this->globalBranchHistReg);
    this->globalBranchHistReg <<= 1;
    if (this->wasBranchTaken) {
        this->globalBranchHistReg |= 1;
    }
    SINUCA3_DEBUG_PRINTF("Gshare Gbhr Aft [%ld]\n", this->globalBranchHistReg);
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
    SINUCA3_DEBUG_PRINTF("Gshare Idx [%ld]\n", this->currentIndex);
}

int GsharePredictor::Configure(Config config) {
    long numberOfEntries = 0;
    if (config.Integer("numberOfEntries", &numberOfEntries, true)) return 1;
    if (numberOfEntries <= 0)
        return config.Error("numberOfEntries", "is not > 0.");
    this->numberOfEntries = numberOfEntries;

    long indexQueueSize = 1;
    if (config.Integer("indexQueueSize", &indexQueueSize)) return 1;
    if (indexQueueSize <= 0)
        return config.Error("indexQueueSize", "is not > 0.");
    this->indexQueueSize = indexQueueSize;

    if (config.ComponentReference("sendTo", &this->sendTo)) return 1;
    if (this->sendTo != NULL) this->sendToId = this->sendTo->Connect(0);

    return this->Allocate();
}

void GsharePredictor::PrintStatistics() {
    double fraction =
        ((double)this->numberOfWrongPredictions / this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("Gshare table size [%lu] & number of index bits [%u]\n",
                       this->numberOfEntries, this->indexBitsSize);
    SINUCA3_LOG_PRINTF("Gshare number of predictions [%lu]\n",
                       this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("Gshare number of wrong predictions [%lu]\n",
                       this->numberOfWrongPredictions);
    SINUCA3_LOG_PRINTF("Gshare rate of wrong predictions [%.0lf]%%\n",
                       fraction * 100);
}

void GsharePredictor::Clock() {
    PredictorPacket packet;
    unsigned long addr;
    long totalConnections = this->GetNumberOfConnections();
    for (long i = 0; i < totalConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            if (packet.type == PredictorPacketTypeRequestQuery) {
                addr = packet.data.requestQuery.staticInfo->instAddress;
                this->Query(&packet, addr);
                if (this->sendTo == NULL) {
                    this->SendResponseToConnection(i, &packet);
                } else {
                    this->sendTo->SendRequest(sendToId, &packet);
                }
            }
            if (packet.type == PredictorPacketTypeRequestDirectionUpdate) {
                this->wasBranchTaken = packet.data.directionUpdate.taken;
                this->Update();
            }
        }
    }
}

#ifndef NDEBUG
const int testSize = 2;

int TestGshare() {
    GsharePredictor predictor;
    PredictorPacket sendPackets[testSize * 2];
    PredictorPacket recvPackets[testSize];
    StaticInstructionInfo ins[testSize];
    const long addrs[] = {0x1, 0x2};
    const bool taken[] = {false, true};

    for (int i = 0; i < testSize; ++i) {
        sendPackets[i].type = PredictorPacketTypeRequestQuery;
        sendPackets[i].data.requestQuery.staticInfo = &ins[i];
        ins[i].instAddress = addrs[i];
    }
    for (int i = testSize; i < testSize * 2; ++i) {
        sendPackets[i].type = PredictorPacketTypeRequestDirectionUpdate;
        sendPackets[i].data.directionUpdate.taken = taken[i - testSize];
    }

    Map<Linkable*> aliases;
    yaml::Parser parser;
    predictor.Configure(
        CreateFakeConfig(&parser, "numberOfEntries: 2\n", &aliases));
    int id = predictor.Connect(testSize);

    // clock 1 (predictor is empty)
    for (int i = 0; i < testSize; ++i) {
        predictor.SendRequest(id, &sendPackets[i]);
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 2 (predictor should not respond)
    assert(predictor.ReceiveResponse(id, &recvPackets[0]) != 0);
    predictor.Clock();
    predictor.PosClock();

    // clock 3 (predictor expected to respond)
    for (int i = 0; i < testSize; ++i) {
        assert(predictor.ReceiveResponse(id, &recvPackets[i]) == 0);
        SINUCA3_DEBUG_PRINTF("Gshare Predicted [%d] for [%ld] ins addr\n",
                         recvPackets[i].type, ins[i].instAddress);
        predictor.SendRequest(id, &sendPackets[i + testSize]); // send update
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 4 (query again)
    for (int i = 0; i < testSize; ++i) {
        predictor.SendRequest(id, &sendPackets[i]);
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 5 (predictor should not respond)
    assert(predictor.ReceiveResponse(id, &recvPackets[0]) != 0);
    predictor.Clock();
    predictor.PosClock();

    // clock 6 (predictor expected to respond)
    for (int i = 0; i < testSize; ++i) {
        assert(predictor.ReceiveResponse(id, &recvPackets[i]) == 0);
        SINUCA3_DEBUG_PRINTF("Gshare Predicted [%d] for [%ld] ins addr\n",
                         recvPackets[i].type, ins[i].instAddress);
        predictor.SendRequest(id, &sendPackets[i + testSize]); // send update
    }
    predictor.Clock();
    predictor.PosClock();

    // clock 7 (process last update request)
    predictor.Clock();
    predictor.PrintStatistics();

    return 0;
}
#endif
