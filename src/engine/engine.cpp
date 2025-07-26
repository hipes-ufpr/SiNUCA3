//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
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
 * @file engine.cpp
 * @brief Implementation of the simulation engine.
 */

#include "engine.hpp"

#include <cstdio>
#include <ctime>

#include "../utils/logging.hpp"

int sinuca::engine::Engine::FinishSetup() { return 0; }

int sinuca::engine::Engine::SetConfigParameter(
    const char* parameter, sinuca::config::ConfigValue value) {
    (void)parameter;
    (void)value;
    return 0;
}

int sinuca::engine::Engine::SendBufferedAndFetch(int id) {
    if (this->SendResponseToConnection(id,
                                       (FetchPacket*)&this->fetchBuffers[id])) {
        return 1;
    }

    const traceReader::FetchResult r =
        this->traceReader->Fetch(&this->fetchBuffers[id], id);
    if (r == traceReader::FetchResultEnd) {
        this->end = true;
        return 1;
    } else if (r == traceReader::FetchResultError) {
        this->error = true;
        return 1;
    }

    ++this->fetchedInstructions;

    return 0;
}

void sinuca::engine::Engine::Fetch(int id, sinuca::FetchPacket packet) {
    if (packet.request == 0) {
        this->SendBufferedAndFetch(id);
        return;
    }

    long weight = this->fetchBuffers[id].staticInfo->opcodeSize;

    while (weight < packet.request) {
        if (this->SendBufferedAndFetch(id)) {
            return;
        }
        weight += this->fetchBuffers[id].staticInfo->opcodeSize;
    }
}

void sinuca::engine::Engine::Clock() {
    sinuca::FetchPacket packet;
    const int numberOfConnections = this->GetNumberOfConnections();

    for (int i = 0; i < numberOfConnections; ++i) {
        if (!this->ReceiveRequestFromConnection(i, &packet)) {
            this->Fetch(i, packet);
        }
    }
}

void sinuca::engine::Engine::Flush() { this->flush = true; }

void sinuca::engine::Engine::PrintStatistics() {
    SINUCA3_LOG_PRINTF("engine: Cycled %lu times.\n", this->totalCycles);
    SINUCA3_LOG_PRINTF("engine: Fetched %lu instructions.\n",
                       this->fetchedInstructions);
}

void sinuca::engine::Engine::PrintTime(time_t start, unsigned long cycle) {
    const unsigned long traceSize = this->traceReader->GetTraceSize();
    const unsigned long remaining =
        traceSize - this->traceReader->GetNumberOfFetchedInstructions();

    SINUCA3_LOG_PRINTF("engine: Heartbeat at cycle %ld.\n", cycle);
    SINUCA3_LOG_PRINTF("engine: Remaining instructions: %ld.\n", remaining - 1);

    const time_t now = time(NULL);
    const time_t estimatedEnd = (traceSize * (now - start) / remaining) + start;
    SINUCA3_LOG_PRINTF("engine: Estimated simulation end: %s",
                       ctime(&estimatedEnd));
}

int sinuca::engine::Engine::SetupSimulation(
    sinuca::traceReader::TraceReader* traceReader) {
    this->traceReader = traceReader;
    this->numberOfFetchers = this->GetNumberOfConnections();
    this->fetchBuffers = new sinuca::InstructionPacket[this->numberOfFetchers];

    // Bufferize the first instruction of each core.
    for (long i = 0; i < this->numberOfFetchers; ++i) {
        if (this->traceReader->Fetch(&this->fetchBuffers[i], i) !=
            traceReader::FetchResultOk) {
            return 1;
        }
        ++this->fetchedInstructions;
    }

    return 0;
}

int sinuca::engine::Engine::Simulate(
    sinuca::traceReader::TraceReader* traceReader) {
    if (this->SetupSimulation(traceReader)) {
        return 1;
    }

    const time_t start = time(NULL);

    SINUCA3_LOG_PRINTF("engine: Simulation started at %s", ctime(&start));
    SINUCA3_LOG_PRINTF("engine: Total instructions: %ld.\n",
                       traceReader->GetTraceSize());

    while (!this->end && !this->error) {
        if ((this->totalCycles + 1) % (1 << 8) == 0)
            this->PrintTime(start, this->totalCycles + 1);

        if (this->flush) {
            for (long i = 0; i < this->numberOfComponents; ++i) {
                this->components[i]->Flush();
                this->components[i]->LinkableFlush();
            }
            this->flush = false;
        }

        for (long i = 0; i < this->numberOfComponents; ++i)
            this->components[i]->Clock();

        for (long i = 0; i < this->numberOfComponents; ++i)
            this->components[i]->PosClock();

        ++this->totalCycles;
    }

    const time_t end = time(NULL);
    SINUCA3_LOG_PRINTF("engine: Simulation ended at %s", ctime(&end));
    SINUCA3_LOG_PRINTF("=== SIMULATION STATISTICS ===\n");

    if (this->error) {
        SINUCA3_ERROR_PRINTF(
            "Simulation ended due to error in trace fetching!\n");
    }

    for (long i = 0; i < this->numberOfComponents; ++i) {
        this->components[i]->PrintStatistics();
    }

    return this->error;
}

sinuca::engine::Engine::~Engine() {
    if (this->components != NULL) {
        // The first component is a pointer to the engine itself, thus we start
        // from the second.
        for (long i = 1; i < this->numberOfComponents; ++i) {
            delete this->components[i];
        }
        delete[] this->components;
    }
    if (this->fetchBuffers != NULL) {
        delete[] this->fetchBuffers;
    }
}
