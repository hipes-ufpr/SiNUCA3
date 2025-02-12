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

#include "../trace_reader/trace_reader.hpp"
#include "../utils/logging.hpp"
#include "component.hpp"
#include "linkable.hpp"

int sinuca::engine::Engine::AddCPUs(sinuca::engine::Linkable** cpus,
                                    long numberOfCPUs) {
    this->cpus = new Component<InstructionPacket>*[numberOfCPUs];
    this->numberOfCPUs = numberOfCPUs;

    // This could be done without another allocation and in a single cast but
    // this way we got a better error message. Me when C++.
    for (long i = 0; i < numberOfCPUs; ++i) {
        // Ensure the component passed was a Component<InstructionPacket>.
        this->cpus[i] = dynamic_cast<Component<InstructionPacket>*>(cpus[i]);
        // If it wasn't, we error out.
        if (this->cpus[i] == NULL) {
            delete[] this->cpus;
            SINUCA3_ERROR_PRINTF(
                "CPU number %ld is not of type Component<InstructionPacket>.\n",
                i);
            return 1;
        }
    }

    delete[] cpus;

    return 0;
}

int sinuca::engine::Engine::Simulate(
    sinuca::traceReader::TraceReader* traceReader) {
    InstructionPacket packet;
    traceReader::FetchResult result;

    while ((result = traceReader->Fetch(&packet)) ==
           traceReader::FetchResultOk) {
        for (long i = 0; i < this->numberOfCPUs; ++i) this->cpus[i]->PreClock();
        for (long i = 0; i < this->numberOfComponents; ++i)
            this->components[i]->PreClock();

        for (long i = 0; i < this->numberOfCPUs; ++i) this->cpus[i]->Clock();
        for (long i = 0; i < this->numberOfComponents; ++i)
            this->components[i]->Clock();

        for (long i = 0; i < this->numberOfCPUs; ++i) this->cpus[i]->PosClock();
        for (long i = 0; i < this->numberOfComponents; ++i)
            this->components[i]->PosClock();
    }

    return (result == traceReader::FetchResultError) ? 1 : 0;
}
