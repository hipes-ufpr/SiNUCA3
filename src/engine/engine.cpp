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
#include <cstring>
#include <ctime>
#include <sinuca3.hpp>
#include <vector>

#include "tracer/trace_reader.hpp"

int NewComponentDefinition(Map<Definition>* definitions,
                           Map<Linkable*>* aliases,
                           std::vector<InstanceWithDefinition>* instances,
                           Map<yaml::YamlValue>* config, const char* name,
                           const char* alias, yaml::YamlLocation location) {
    yaml::YamlValue* clazzYaml = config->Get("class");

    if (clazzYaml == NULL) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component class not passed.\n",
                             location.file, location.line, location.column);
        return 1;
    }
    if (clazzYaml->type != yaml::YamlValueTypeString) {
        SINUCA3_ERROR_PRINTF("%s:%lu:%lu Component class is not a string.\n",
                             clazzYaml->location.file, clazzYaml->location.line,
                             clazzYaml->location.column);
        return 1;
    }
    const char* clazz = clazzYaml->value.string;

    Definition definition;
    definition.config = config;
    definition.location = location;
    definitions->Insert(name, definition);

    if (alias != NULL) {
        Linkable* newComponent = CreateComponentByClass(clazz);
        if (newComponent == NULL) {
            SINUCA3_ERROR_PRINTF(
                "%s:%lu:%lu Component class %s doesn't exists.\n",
                clazzYaml->location.file, clazzYaml->location.line,
                clazzYaml->location.column, clazz);
            return 1;
        }
        aliases->Insert(alias, newComponent);

        InstanceWithDefinition instanceWithDefinition;
        instanceWithDefinition.definition = name;
        instanceWithDefinition.component = newComponent;
        instances->push_back(instanceWithDefinition);
    }

    return 0;
}

int Engine::Configure(Config config) {
    std::vector<InstanceWithDefinition> instances;
    Map<Definition>* definitions = config.Definitions();
    Map<Linkable*>* aliases = config.Aliases();
    // Heuristic.
    instances.reserve(32);

    aliases->Insert("ENGINE", this);

    Map<yaml::YamlValue>* map = config.RawYaml();

    yaml::YamlValue value;
    map->ResetIterator();
    for (const char* key = map->Next(&value); key != NULL;
         key = map->Next(&value)) {
        if (value.type != yaml::YamlValueTypeMapping) {
            SINUCA3_ERROR_PRINTF(
                "%s:%ld:%ld Toplevel configuration parameter is not a mapping: "
                "%s",
                value.location.file, value.location.line, value.location.column,
                key);
            return 1;
        }
        if (NewComponentDefinition(definitions, aliases, &instances,
                                   value.value.mapping, key, value.anchor,
                                   value.location) != 0) {
            for (unsigned int i = 0; i < instances.size(); ++i)
                delete instances[i].component;
            return 1;
        }
    }

    std::vector<Linkable*>* components = config.Components();

    components->reserve(instances.size() + 32);  // Heuristic.
    components->push_back(this);
    for (unsigned int i = 0; i < instances.size(); ++i)
        components->push_back(instances[i].component);

    for (unsigned int i = 0; i < instances.size(); ++i) {
        Linkable* component = instances[i].component;
        Definition* definition = definitions->Get(instances[i].definition);
        assert(definition != NULL);
        Config config = Config(components, aliases, definitions,
                               definition->config, definition->location);
        if (component->Configure(config)) {
            // Skip the engine.
            for (unsigned int i = 1; i < components->size(); ++i)
                delete (*components)[i];
            return 1;
        }
    }

    this->numberOfComponents = components->size();
    this->components = new Linkable*[this->numberOfComponents];
    for (unsigned int i = 0; i < this->numberOfComponents; ++i)
        this->components[i] = (*components)[i];

    return 0;
}

int Engine::SendBufferedAndFetch(int id) {
    static int fetcherIdToThreadId = 0;
    static int lastFetchedProcessId = -1;
    if (lastFetchedProcessId != this->fetcherIdToProcessId[id]) {
        fetcherIdToThreadId = 0;
        lastFetchedProcessId = this->fetcherIdToProcessId[id];
    } else {
        ++fetcherIdToThreadId;
    }

    InstructionPacket toSend = this->fetchBuffers[id];
    const FetchResult r =
        this->traceReaders[this->fetcherIdToProcessId[id]]->Fetch(
            &this->fetchBuffers[id], fetcherIdToThreadId);
    toSend.nextInstruction = this->fetchBuffers[id].staticInfo->instAddress;
    toSend.processId = this->fetcherIdToProcessId[id];

    if (r == FetchResultEnd) {
        this->end = true;
        return 1;
    } else if (r == FetchResultError) {
        this->error = true;
        return 1;
    } else if (r == FetchResultWait) {
        this->isFetcherWaiting[id] = true;
        return 1;
    }

    // This unfortunately drops the packet if the buffer is full. The component
    // must ensure the buffers never fills.
    if (this->SendResponseToConnection(id, (FetchPacket*)&toSend) != 0) {
        SINUCA3_WARNING_PRINTF(
            "== INSTRUCTION DROP DETECTED == core %d made requests "
            "with a full buffer, instructions will be dropped.\n",
            id);
    }

    ++this->fetchedInstructions;

    return 0;
}

void Engine::Fetch(int id, FetchPacket packet) {
    if (packet.request == 0) {
        this->SendBufferedAndFetch(id);
        return;
    }

    if (this->isFetcherWaiting[id]) {
        // The fetcher is waiting for an event, so it cannot fetch instructions.
        return;
    }

    long weight = this->fetchBuffers[id].staticInfo->instSize;

    while (weight < packet.request) {
        if (this->SendBufferedAndFetch(id)) {
            return;
        }
        weight += this->fetchBuffers[id].staticInfo->instSize;
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
    SINUCA3_LOG_PRINTF("Cycled %lu times.\n", this->totalCycles);
    SINUCA3_LOG_PRINTF("Fetched %lu instructions.\n",
                       this->fetchedInstructions);
}

unsigned long Engine::GetTraceSize() {
    unsigned long size = 0;
    for (unsigned int i = 0; i < this->traceReaders.size(); ++i) {
        for (int j = 0; j < this->traceReaders[i]->GetTotalThreads(); ++j) {
            size += this->traceReaders[i]->GetTotalInstToBeFetched(j);
        }
    }
    return size;
}

void Engine::PrintTime(time_t start, unsigned long cycle) {
    const unsigned long remaining = this->traceSize - this->fetchedInstructions;

    SINUCA3_LOG_PRINTF("Heartbeat at cycle %ld.\n", cycle);
    SINUCA3_LOG_PRINTF("Remaining instructions: %ld.\n", remaining);

    const time_t now = time(NULL);
    const time_t estimatedEnd =
        remaining == 0 ? now : (traceSize * (now - start) / remaining) + start;
    SINUCA3_LOG_PRINTF("Estimated simulation end: %s", ctime(&estimatedEnd));
}

int Engine::SetupSimulation(std::vector<TraceReader*>* traceReaders) {
    this->traceReaders = *traceReaders;
    this->numberOfFetchers = this->GetNumberOfConnections();
    this->fetchBuffers = new InstructionPacket[this->numberOfFetchers];
    this->isFetcherWaiting = new bool[this->numberOfFetchers];
    this->fetcherIdToProcessId = new int[this->numberOfFetchers];
    memset(this->isFetcherWaiting, 0, sizeof(bool) * this->numberOfFetchers);

    int threads = 0;
    int fetcher = 0;
    for (unsigned int i = 0; i < this->traceReaders.size(); ++i) {
        threads = this->traceReaders[i]->GetTotalThreads();
        if (i * threads > this->numberOfFetchers) {
            SINUCA3_ERROR_PRINTF(
                "The number of fetch connections (%d) is less than the "
                "number of threads in the trace files (%d).\n",
                this->numberOfFetchers, i * threads);
            return 1;
        }
        for (int j = 0; j < threads; ++j, ++fetcher) {
            this->fetcherIdToProcessId[fetcher] = i;
            FetchResult r =
                this->traceReaders[i]->Fetch(&this->fetchBuffers[fetcher], j);
            if (r == FetchResultError) {
                return 1;
            } else if (r == FetchResultEnd) {
                SINUCA3_WARNING_PRINTF(
                    "Unexpected end of trace file %d while setting up "
                    "simulation.\n",
                    i);
                this->end = true;
                return 1;
            } else if (r == FetchResultWait) {
                SINUCA3_WARNING_PRINTF(
                    "Thread %d of trace file %d is waiting for an event, it "
                    "will "
                    "not be fetched until the event is triggered.\n",
                    j, i);
                this->isFetcherWaiting[fetcher] = true;
            } else {
                ++this->fetchedInstructions;
                this->fetchBuffers[fetcher].processId = i;
            }
        }
        ++this->fetchedInstructions;
    }

    return 0;
}

int Engine::Simulate(std::vector<TraceReader*>* traceReaders) {
    if (this->SetupSimulation(traceReaders)) {
        return 1;
    }

    this->traceSize = this->GetTraceSize();

    const time_t start = time(NULL);

    SINUCA3_LOG_PRINTF("Simulation started at %s", ctime(&start));
    SINUCA3_LOG_PRINTF("Total instructions: %ld.\n", this->traceSize);

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
    SINUCA3_LOG_PRINTF("Simulation ended at %s", ctime(&end));
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