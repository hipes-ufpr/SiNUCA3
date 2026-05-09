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

#include "engine/default_packets.hpp"
#include "tracer/trace_reader.hpp"
#include "utils/logger.hpp"

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
    if (!this->fetchers[id].Ready()) return 1;
    InstructionPacket toSend;
    this->fetchers[id].GetPkt(&toSend);
    // This unfortunately drops the packet if the buffer is full. The component
    // must ensure the buffers never fills.
    if (this->SendResponseToConnection(id, (FetchPacket*)&toSend) != 0) {
        SINUCA3_WARNING_PRINTF(
            "== INSTRUCTION DROP DETECTED == core %d made requests "
            "with a full buffer, instructions will be dropped.\n",
            id);
    }
    this->fetchers[id].TryFetch();
    return 0;
}

void Engine::Fetch(int id) {
    this->fetchers[id].waiting = false;
    if (this->fetchers[id].requested == 0) {
        this->SendBufferedAndFetch(id);
        return;
    }

    long weight = fetchers[id].curr.staticInfo->instSize;
    while (weight < this->fetchers[id].requested) {
        if (this->SendBufferedAndFetch(id)) {
            return;
        }
        weight += fetchers[id].curr.staticInfo->instSize;;
    }
}

void Engine::Clock() {
    FetchPacket packet;
    const int numberOfConnections = this->GetNumberOfConnections();

    for (int i = 0; i < numberOfConnections; ++i) {
        if (!this->fetchers[i].waiting && !this->ReceiveRequestFromConnection(i, &packet)) {
            this->fetchers[i].requested = packet.request;
            this->fetchers[i].waiting = true;
        }
        if (this->fetchers[i].Ready()) {
            if (this->fetchers[i].waiting)
                this->Fetch(i);
        } else {
            this->fetchers[i].TryFetch();
        }
        this->end |= this->fetchers[i].end;
        this->error |= this->fetchers[i].error;
    }
}

void Engine::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Cycled %lu times.\n", this->totalCycles);
    SINUCA3_LOG_PRINTF("Fetched %lu instructions.\n",
                       this->fetchedInstructions);
}

unsigned long Engine::GetTraceSize() {
    unsigned long size = 0;
    for (unsigned int i = 0; i < this->numberOfFetchers; ++i) {
        size += this->fetchers[i].GetInstToBeFetched();
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

int Engine::SetupSimulation(std::vector<TraceReader*>* tracers) {
    this->numberOfFetchers = this->GetNumberOfConnections();
    this->fetchers = new Fetcher[this->numberOfFetchers];
    /* Map each thread to a fetcher. */
    for (unsigned long i = 0, k = 0; i < tracers->size(); i++) {
        int threads = tracers->at(i)->GetTotalThreads();
        for (int j = 0; j < threads; j++, k++) {
            this->fetchers[k].Set(tracers->at(i), j);
        }
    }
    return 0;
    // configure mmu
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
    if (this->fetchers != NULL) {
        delete[] this->fetchers;
    }
}

void Engine::Fetcher::Set(TraceReader* tracer, int tid) {
    this->tracer = tracer;
    this->tid = tid;
}

bool Engine::Fetcher::FetcherSet() const {
    return (this->tracer != NULL && this->tid >= 0);
}

bool Engine::Fetcher::Ready() const {
    return (this->FetcherSet() && this->currValid);
}

void Engine::Fetcher::TryFetch() {
    if (!this->FetcherSet()) {
        return;
    }

    InstructionPacket* target = (this->currFetched) ? &this->next : &this->curr;

    const FetchResult r = this->tracer->Fetch(target, this->tid);
    if (r == FetchResultError) {
        this->error = true;
    } else if (r == FetchResultEnd) {
        this->end = true;
    } else if (r != FetchResultWait) {
        if (target == &this->next) {
            this->currValid = true;
            this->curr.nextInstruction = this->next.staticInfo->instAddress;
        } else {
            this->currFetched = true;
        }
    }
}

void Engine::Fetcher::GetPkt(InstructionPacket* target) {
    *target = this->curr;
    this->curr = this->next;
    this->currFetched = this->currValid;
    this->currValid = false;
}

unsigned long Engine::Fetcher::GetInstToBeFetched() const {
    if (!this->FetcherSet()) {
        return 0;
    }
    return this->tracer->GetTotalInstToBeFetched(this->tid);
}