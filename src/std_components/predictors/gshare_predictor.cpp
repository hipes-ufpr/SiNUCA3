

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
      numberOfWrongpredictions(0),
      indexQueueSize(0),
      indexBitsSize(0) {}

GsharePredictor::~GsharePredictor() { this->Deallocate(); }

int GsharePredictor::Allocate() {
    this->entries = new BimodalCounter[this->numberOfEntries];
    if (!this->entries) {
        SINUCA3_ERROR_PRINTF("Gshare failed to allocate Bim\n");
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
    int bits = (unsigned int)floor(log2(requestedSize));
    if (bits == 0) {
        return 1;
    }
    this->numberOfEntries = 1 << bits;
    this->indexBitsSize = bits;
    return 0;
}

void GsharePredictor::PreparePacket(PredictorPacket* pkt) {
    if (this->directionPredicted == TAKEN) {
        pkt->type = PredictorPacketTypeResponseTake;
    } else {
        pkt->type = PredictorPacketTypeResponseDontTake;
    }
}

void GsharePredictor::ReadPacket(PredictorPacket* pkt) {
    this->directionTaken = pkt->data.directionRequestUpdate.direction;
}

int GsharePredictor::EnqueueIndex() {
    return this->indexQueue.Enqueue(&this->currentIndex);
}

int GsharePredictor::DequeueIndex() {
    return this->indexQueue.Dequeue(&this->currentIndex);
}

void GsharePredictor::UpdateEntry() {
    bool pred = this->entries[this->currentIndex].GetPrediction();
    if (pred != this->directionTaken) {
        this->numberOfWrongpredictions++;
    }
    this->entries[this->currentIndex].UpdatePrediction(this->directionTaken);
}

void GsharePredictor::UpdateGlobBranchHistReg() {
    this->globalBranchHistReg <<= 1;
    if (this->directionTaken == TAKEN) {
        this->globalBranchHistReg |= 1;
    }
}

void GsharePredictor::QueryEntry() {
    this->numberOfPredictions++;
    this->directionPredicted =
        this->entries[this->currentIndex].GetPrediction();
}

void GsharePredictor::CalculateIndex(unsigned long addr) {
    this->currentIndex =
        (this->globalBranchHistReg ^ addr) & this->indexBitsSize;
}

int GsharePredictor::SetConfigParameter(const char* parameter,
                                        ConfigValue value) {
    if (strcmp(parameter, "numberOfEntries") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            return 1;
        }
        unsigned long requestedNumberOfEntries = value.value.integer;
        if (requestedNumberOfEntries <= 1) {
            return 1;
        }
        return 0;
    }
    if (strcmp(parameter, "indexQueueSize") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            return 1;
        }
        this->indexQueueSize = value.value.integer;
        return 0;
    }
    SINUCA3_ERROR_PRINTF("Gshare predictor got unkown parameter\n");
    return 1;
}

void GsharePredictor::PrintStatistics() {
    double percentage =
        (double)this->numberOfWrongpredictions / this->numberOfPredictions;
    SINUCA3_DEBUG_PRINTF(
        "Gshare table size [%lu] & number of index bits [%u]\n",
        this->numberOfEntries, this->indexBitsSize);
    SINUCA3_LOG_PRINTF("Gshare number of predictions [%lu]\n",
                       this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("Gshare rate of wrong predictions [%lf]%%\n",
                       percentage);
}

int GsharePredictor::FinishSetup() {
    if (this->Allocate()) {
        return 1;
    }
    return 0;
}

void GsharePredictor::Clock() {
    PredictorPacket packet;
    unsigned long addr;
    long totalConnections = this->GetNumberOfConnections();
    for (long i = 0; i < totalConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case PredictorPacketTypeRequestQuery:
                    addr = packet.data.requestQuery.staticInfo->opcodeAddress;
                    this->CalculateIndex(addr);
                    this->QueryEntry();
                    if (this->EnqueueIndex()) {
                        SINUCA3_WARNING_PRINTF("Gshare index buffer full\n");
                    }
                    this->PreparePacket(&packet);
                    this->sendTo->SendRequest(sendToId, &packet);
                    break;
                case PredictorPacketTypeRequestUpdate:
                    if (this->DequeueIndex()) {
                        SINUCA3_ERROR_PRINTF("Gshare table not updated\n");
                        return;
                    }
                    this->ReadPacket(&packet);
                    this->UpdateEntry();
                    this->UpdateGlobBranchHistReg();
                    break;
                default:
                    SINUCA3_ERROR_PRINTF("Gshare invalid packet type\n");
            }
        }
    }
}