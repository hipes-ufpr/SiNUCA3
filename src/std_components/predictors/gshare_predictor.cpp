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
#include <cstdlib>
#include <sinuca3.hpp>

#include "engine/default_packets.hpp"
#include "utils/logger.hpp"

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
        SINUCA3_ERROR_PRINTF("[Allocate] Gshare failed to allocate table!\n");
        return 1;
    }

    this->indexQueue.Allocate(this->indexQueueSize, sizeof(unsigned long));
    return 0;
}

void GsharePredictor::Deallocate() {
    if (this->entries) {
        delete[] this->entries;
        this->entries = NULL;
    }
    if (!this->indexQueue.IsEmpty()) {
        SINUCA3_WARNING_PRINTF(
            "[Deallocate] Gshare index queue not empty when it was expected to "
            "be\n");
    }
    this->indexQueue.Deallocate();
}

void GsharePredictor::RoundNumberOfEntries(unsigned long requestedSize) {
    unsigned int bits = (unsigned int)floor(log2(requestedSize));
    if (bits == 0) {
        this->numberOfEntries = 2;
        this->indexBitsSize = 1;
    } else {
        this->numberOfEntries = 1 << bits;
        this->indexBitsSize = bits;
    }
    SINUCA3_DEBUG_PRINTF("[RoundNumberOfEntries] number of entries is [%lu]\n",
                         this->numberOfEntries);
}

void GsharePredictor::PreparePacket(PredictorPacket* pkt, bool predIsTake) {
    pkt->type = predIsTake ? PredictorPacketTypeResponseTake
                           : PredictorPacketTypeResponseDontTake;
}

int GsharePredictor::EnqueueIndex(unsigned long idx) {
    bool ret = this->indexQueue.Enqueue(&idx);
    return ret;
}

int GsharePredictor::DequeueIndex(unsigned long* idx) {
    bool ret = this->indexQueue.Dequeue(idx);
    return ret;
}

void GsharePredictor::UpdateEntry(unsigned long index, bool wasBranchTaken) {
    bool pred = this->entries[index].GetPrediction();
    if (pred != wasBranchTaken) {
        this->numberOfWrongPredictions++;
    }
    this->entries[index].UpdatePrediction(wasBranchTaken);
}

void GsharePredictor::UpdateGlobBranchHistReg(bool wasBranchTaken) {
    this->globalBranchHistReg <<= 1;
    this->globalBranchHistReg |= (wasBranchTaken) ? 1 : 0;
}

void GsharePredictor::Update(bool wasBranchTaken) {
    unsigned long index;
    if (this->DequeueIndex(&index)) {
        SINUCA3_ERROR_PRINTF("[Update] Dequeue failed!\n");
        return;
    }
    this->UpdateEntry(index, wasBranchTaken);
    this->UpdateGlobBranchHistReg(wasBranchTaken);
}

bool GsharePredictor::QueryEntry(unsigned long index) {
    this->numberOfPredictions++;
    return this->entries[index].GetPrediction();
}

void GsharePredictor::Query(PredictorPacket* pkt, unsigned long addr) {
    unsigned long idx = this->CalculateIndex(addr);
    this->PreparePacket(pkt, this->QueryEntry(idx));
    if (this->EnqueueIndex(idx)) {
        SINUCA3_WARNING_PRINTF("[Query] Enqueue failed!\n");
    }
}

unsigned long GsharePredictor::CalculateIndex(unsigned long addr) {
    unsigned long mask = (1 << this->indexBitsSize) - 1;
    return (this->globalBranchHistReg ^ addr) & mask;
}

int GsharePredictor::Configure(Config config) {
    long requestedSize = 0;
    if (config.Integer("requestedSize", &requestedSize, true)) return 1;
    if (requestedSize <= 0) return config.Error("requestedSize", "is not > 0.");
    this->RoundNumberOfEntries(requestedSize);

    long indexQueueSize = 0;
    if (config.Integer("indexQueueSize", &indexQueueSize)) return 1;
    if (indexQueueSize < 0)
        return config.Error("indexQueueSize", "is not > 0.");
    this->indexQueueSize = indexQueueSize;

    if (config.ComponentReference("sendTo", &this->sendTo)) return 1;
    if (this->sendTo != NULL) this->sendToId = this->sendTo->Connect(0);

    return this->Allocate();
}

void GsharePredictor::PrintStatistics() {
    double fraction =
        ((double)this->numberOfWrongPredictions / this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("table size [%lu] & number of index bits [%u]\n",
                       this->numberOfEntries, this->indexBitsSize);
    SINUCA3_LOG_PRINTF("number of predictions [%lu]\n",
                       this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("number of wrong predictions [%lu]\n",
                       this->numberOfWrongPredictions);
    SINUCA3_LOG_PRINTF("rate of wrong predictions [%.0lf]%%\n", fraction * 100);
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
                this->Update(packet.data.directionUpdate.taken);
            }
        }
    }
}

#ifndef NDEBUG
const int numInst = 101;
const int len = 4;
const int numUpdatesPerIns = 12;

int TestGshare() {
    GsharePredictor predictor;
    PredictorPacket* queryPackets = new PredictorPacket[numInst];
    PredictorPacket* updatePackets = new PredictorPacket[numInst];
    StaticInstructionInfo* ins = new StaticInstructionInfo[numInst];
    bool* taken = new bool[numInst];

    srand(0);
    for (int i = 0; i < numInst; i++) {
        ins[i].instAddress = (long)rand();
        taken[i] = (bool)(rand() % 2);
    }

    Map<Linkable*> aliases;
    yaml::Parser parser;
    if (predictor.Configure(
            CreateFakeConfig(&parser, "requestedSize: 1024\n", &aliases))) {
        return 1;
    }
    int id = predictor.Connect(len);

    // update predictor `numUpdatesPerIns` times for each instruction
    for (int i = 0; i < numUpdatesPerIns; i++) {
        // simulate fetch of instructions
        for (int j = 0; j < numInst;) {
            int k;
            // send query requests
            for (k = 0; k < len && j < numInst; k++, j++) {
                queryPackets[j].type = PredictorPacketTypeRequestQuery;
                queryPackets[j].data.requestQuery.staticInfo = &ins[j];
                predictor.SendRequest(id, &queryPackets[j]);
            }

            predictor.Clock();
            predictor.PosClock();
            predictor.Clock();
            predictor.PosClock();

            j -= k;
            // send update requests
            for (k = 0; k < len && j < numInst; k++, j++) {
                updatePackets[j].type =
                    PredictorPacketTypeRequestDirectionUpdate;
                updatePackets[j].data.directionUpdate.taken = taken[j];
                predictor.SendRequest(id, &updatePackets[j]);
            }

            predictor.Clock();
            predictor.PosClock();
            predictor.Clock();
            predictor.PosClock();
        }
    }

    predictor.PrintStatistics();
    delete[] queryPackets;
    delete[] updatePackets;
    delete[] taken;
    delete[] ins;

    return 0;
}
#endif
