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
#include <sinuca3.hpp>

int Engine::FinishSetup() { return 0; }

int Engine::SetConfigParameter(const char* parameter, ConfigValue value) {
    (void)parameter;
    (void)value;
    return 0;
}

int Engine::SendBufferedAndFetch(int id) {
    InstructionPacket toSend = this->fetchBuffers[id];
    const FetchResult r = this->traceReader->Fetch(&this->fetchBuffers[id], id);
    toSend.nextInstruction = this->fetchBuffers[id].staticInfo->opcodeAddress;

    // This unfortunately drops the packet if the buffer is full. The component
    // must ensure the buffers never fills.
    if (this->SendResponseToConnection(id, (FetchPacket*)&toSend) != 0) {
        SINUCA3_WARNING_PRINTF(
            "engine: == INSTRUCTION DROP DETECTED == core %d made requests "
            "with a full buffer, instructions will be dropped.\n",
            id);
    }

    if (r == FetchResultEnd) {
        this->end = true;
        return 1;
    } else if (r == FetchResultError) {
        this->error = true;
        return 1;
    }

    ++this->fetchedInstructions;

    return 0;
}

void Engine::Fetch(int id, FetchPacket packet) {
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

void Engine::Clock() {
    FetchPacket packet;
    const int numberOfConnections = this->GetNumberOfConnections();

    for (int i = 0; i < numberOfConnections; ++i) {
        if (!this->ReceiveRequestFromConnection(i, &packet)) {
            this->Fetch(i, packet);
        }
    }
}

void Engine::PrintStatistics() {
    SINUCA3_LOG_PRINTF("engine: Cycled %lu times.\n", this->totalCycles);
    SINUCA3_LOG_PRINTF("engine: Fetched %lu instructions.\n",
                       this->fetchedInstructions);
}

unsigned long Engine::GetTraceSize() {
    unsigned long size = 0;
    for (int i = 0; i < this->traceReader->GetTotalThreads(); ++i) {
        size += this->traceReader->GetTotalInstToBeFetched(i);
    }
    return size;
}

void Engine::PrintTime(time_t start, unsigned long cycle) {
    const unsigned long remaining = this->traceSize - this->fetchedInstructions;

    SINUCA3_LOG_PRINTF("engine: Heartbeat at cycle %ld.\n", cycle);
    SINUCA3_LOG_PRINTF("engine: Remaining instructions: %ld.\n", remaining);

    const time_t now = time(NULL);
    const time_t estimatedEnd =
        remaining == 0 ? now : (traceSize * (now - start) / remaining) + start;
    SINUCA3_LOG_PRINTF("engine: Estimated simulation end: %s",
                       ctime(&estimatedEnd));
}

int Engine::SetupSimulation(TraceReader* traceReader) {
    this->traceReader = traceReader;
    this->numberOfFetchers = this->GetNumberOfConnections();
    this->fetchBuffers = new InstructionPacket[this->numberOfFetchers];

    // Bufferize the first instruction of each core.
    for (long i = 0; i < this->numberOfFetchers; ++i) {
        if (this->traceReader->Fetch(&this->fetchBuffers[i], i) !=
            FetchResultOk) {
            return 1;
        }
        ++this->fetchedInstructions;
    }

    return 0;
}

int Engine::Simulate(TraceReader* traceReader) {
    if (this->SetupSimulation(traceReader)) {
        return 1;
    }

    this->traceSize = this->GetTraceSize();

    const time_t start = time(NULL);

    SINUCA3_LOG_PRINTF("engine: Simulation started at %s", ctime(&start));
    SINUCA3_LOG_PRINTF("engine: Total instructions: %ld.\n", this->traceSize);

    while (!this->end && !this->error) {
        if ((this->totalCycles + 1) % (1 << 8) == 0)
            this->PrintTime(start, this->totalCycles + 1);

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

Engine::~Engine() {
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
