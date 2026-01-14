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
 * @file ras.cpp
 * @brief Implementation of the Ras, a simple return address stack.
 */

#include "ras.hpp"

#include <sinuca3.hpp>

int Ras::Configure(Config config) {
    long size;
    if (config.Integer("size", &size, true)) return 1;
    if (size <= 0) return config.Error("size", "is not > 0.");
    this->size = size;

    if (config.ComponentReference("sendTo", &this->sendTo)) return 1;
    if (this->sendTo != NULL) this->forwardToID = this->sendTo->Connect(0);

    this->buffer = new unsigned long[this->size]();

    return 0;
}

inline void Ras::RequestQuery(InstructionPacket instruction, int connectionID) {
    unsigned long prediction = this->buffer[this->end];
    --this->end;
    if (this->end < 0) this->end = this->size - 1;

    PredictorPacket response;
    response.type = PredictorPacketTypeResponseTakeToAddress;
    response.data.targetResponse.instruction = instruction;
    response.data.targetResponse.target = prediction;

    this->SendResponseToConnection(connectionID, &response);
    if (this->sendTo != NULL) {
        this->sendTo->SendRequest(this->forwardToID, &response);
    }
}

inline void Ras::RequestUpdate(unsigned long targetAddress) {
    ++this->end;
    if (this->end >= this->size) this->end = 0;

    this->buffer[this->end] = targetAddress;
}

void Ras::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    PredictorPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case PredictorPacketTypeRequestQuery:
                    ++this->numQueries;
                    this->RequestQuery(packet.data.requestQuery, i);
                    break;
                case PredictorPacketTypeRequestTargetUpdate:
                    ++this->numUpdates;
                    this->RequestUpdate(packet.data.targetUpdate.target);
                    break;
                default:
                    SINUCA3_WARNING_PRINTF(
                        "Connection %ld send a response type message to Ras.\n",
                        i);
            }
        }
    }
}

void Ras::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Ras [%p]\n", this);
    SINUCA3_LOG_PRINTF("    Ras Queries: %lu\n", this->numQueries);
    SINUCA3_LOG_PRINTF("    Ras Updates: %lu\n", this->numUpdates);
}

Ras::~Ras() {
    if (this->buffer != NULL) delete[] this->buffer;
}

#ifndef NDEBUG

int TestRas() {
    Ras ras;
    Map<Linkable*> aliases;
    yaml::Parser parser;

    ras.Configure(CreateFakeConfig(&parser, "size: 5\n", &aliases));
    int id = ras.Connect(1);

    ras.Clock();
    ras.PosClock();

    PredictorPacket msg;
    msg.type = PredictorPacketTypeRequestTargetUpdate;

    ras.Clock();
    msg.data.targetUpdate.target = 0xcafebabe;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.data.targetUpdate.target = 0xdeadbeef;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = PredictorPacketTypeRequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond first query!\n");
        return 1;
    }
    if (msg.data.targetResponse.target != 0xdeadbeef) {
        SINUCA3_LOG_PRINTF(
            "Ras responded first query with wrong address %ld!\n",
            msg.data.targetResponse.target);
        return 1;
    }
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = PredictorPacketTypeRequestTargetUpdate;
    msg.data.targetUpdate.target = 0xb16b00b5;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = PredictorPacketTypeRequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond second query!\n");
        return 1;
    }
    if (msg.data.targetResponse.target != 0xb16b00b5) {
        SINUCA3_LOG_PRINTF(
            "Ras responded second query with wrong address %ld!\n",
            msg.data.targetResponse.target);
        return 1;
    }

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = PredictorPacketTypeRequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond third query!\n");
        return 1;
    }
    if (msg.data.targetResponse.target != 0xcafebabe) {
        SINUCA3_LOG_PRINTF(
            "Ras responded third query with wrong address %ld!\n",
            msg.data.targetResponse.target);
        return 1;
    }

    SINUCA3_LOG_PRINTF("Ras test was successful!\n");

    return 0;
}

#endif
