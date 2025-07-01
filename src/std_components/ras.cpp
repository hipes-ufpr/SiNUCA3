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

#include "../utils/logging.hpp"

int Ras::FinishSetup() {
    if (this->size == 0) {
        SINUCA3_ERROR_PRINTF(
            "Ras didn't received obrigatory parameter size.\n");
        return 1;
    }

    this->buffer = new unsigned long[this->size];

    return 0;
}

int Ras::SetConfigParameter(const char* parameter,
                            sinuca::config::ConfigValue value) {
    if (strcmp(parameter, "size") != 0) {
        SINUCA3_WARNING_PRINTF("Ras received an unknown parameter: %s.\n",
                               parameter);
        return 0;
    }

    if (value.type != sinuca::config::ConfigValueTypeInteger) {
        SINUCA3_ERROR_PRINTF("Ras parameter size is not an integer.\n");
        return 1;
    }

    const long size = value.value.integer;
    if (size <= 0) {
        SINUCA3_ERROR_PRINTF(
            "Invalid value for Ras parameter size: should be > 0.\n");
        return 1;
    }

    this->size = size;

    return 0;
}

inline void Ras::RequestQuery(int connectionID) {
    unsigned long prediction = this->buffer[this->end];
    --this->end;
    if (this->end < 0) this->end = this->size - 1;

    sinuca::PredictorPacket response;
    response.type = sinuca::ResponseTakeToAddress;
    response.data.responseAddress = prediction;

    this->SendResponseToConnection(connectionID, &response);
}

inline void Ras::RequestUpdate(unsigned long targetAddress) {
    ++this->end;
    if (this->end >= this->size) this->end = 0;

    this->buffer[this->end] = targetAddress;
}

void Ras::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    sinuca::PredictorPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case sinuca::RequestQuery:
                    ++this->numQueries;
                    this->RequestQuery(i);
                    break;
                case sinuca::RequestUpdate:
                    ++this->numUpdates;
                    this->RequestUpdate(
                        packet.data.requestUpdate.targetAddress);
                    break;
                default:
                    SINUCA3_WARNING_PRINTF(
                        "Connection %ld send a response type message to Ras.\n",
                        i);
            }
        }
    }
}

void Ras::Flush() {}

void Ras::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Ras %p: %lu queries\n", this, this->numQueries);
    SINUCA3_LOG_PRINTF("Ras %p: %lu updates\n", this, this->numUpdates);
}

Ras::~Ras() {
    if (this->buffer != NULL) delete[] this->buffer;
}

#ifndef NDEBUG

int TestRas() {
    Ras ras;

    ras.SetConfigParameter("size", sinuca::config::ConfigValue((long)5));
    int id = ras.Connect(1);
    ras.FinishSetup();

    ras.Clock();
    ras.PosClock();

    sinuca::PredictorPacket msg;
    msg.type = sinuca::RequestUpdate;

    ras.Clock();
    msg.data.requestUpdate.targetAddress = 0xcafebabe;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.data.requestUpdate.targetAddress = 0xdeadbeef;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = sinuca::RequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond first query!\n");
        return 1;
    }
    if (msg.data.responseAddress != 0xdeadbeef) {
        SINUCA3_LOG_PRINTF(
            "Ras responded first query with wrong address %ld!\n",
            msg.data.responseAddress);
        return 1;
    }
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = sinuca::RequestUpdate;
    msg.data.requestUpdate.targetAddress = 0xb16b00b5;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = sinuca::RequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond second query!\n");
        return 1;
    }
    if (msg.data.responseAddress != 0xb16b00b5) {
        SINUCA3_LOG_PRINTF(
            "Ras responded second query with wrong address %ld!\n",
            msg.data.responseAddress);
        return 1;
    }

    ras.Clock();
    ras.PosClock();

    ras.Clock();
    msg.type = sinuca::RequestQuery;
    ras.SendRequest(id, &msg);
    ras.PosClock();

    ras.Clock();
    ras.PosClock();

    if (ras.ReceiveResponse(id, &msg)) {
        SINUCA3_LOG_PRINTF("Ras did not respond third query!\n");
        return 1;
    }
    if (msg.data.responseAddress != 0xcafebabe) {
        SINUCA3_LOG_PRINTF(
            "Ras responded third query with wrong address %ld!\n",
            msg.data.responseAddress);
        return 1;
    }

    SINUCA3_LOG_PRINTF("Ras test was successful!\n");

    return 0;
}

#endif
