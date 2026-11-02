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
 * @file simple_core.cpp
 * @details Public API of the SimpleCore, a testing core that executes
 * everything in a single clock cycle.
 */

#include "simple_core.hpp"

#include <cassert>
#include <cstring>
#include <sinuca3.hpp>

#include "engine/linkable.hpp"
#include "utils/logger.hpp"

int SimpleCore::Configure(Config config) {
    if (this->EstablishCoreID() != 0) {
        SINUCA3_ERROR_PRINTF("Failed to establish core ID for core [%p].\n",
                             this);
        return 1;
    }
    if (this->EstablishConnections(config) != 0) {
        SINUCA3_ERROR_PRINTF("Failed to establish connections for core [%d].\n",
                             this->coreId);
        return 1;
    }
    if (this->EstablishChildComponents() != 0) {
        SINUCA3_ERROR_PRINTF(
            "Failed to establish child components for core [%d].\n",
            this->coreId);
        return 1;
    }
    if (this->EstablishContextAndPropagate() != 0) {
        SINUCA3_ERROR_PRINTF("Failed to establish context for core [%d].\n",
                             this->coreId);
        return 1;
    }
    return 0;
}

int SimpleCore::PosConfigure() {
    SINUCA3_LOG_PRINTF("Core [%d] configured with:\n", this->coreId);

    SINUCA3_LOG_PRINTF("Instruction Memory: [%s]\n",
                       this->instructionMemory != NULL ? "ON" : "OFF");
    SINUCA3_LOG_PRINTF("Data Memory: [%s]\n",
                       this->dataMemory != NULL ? "ON" : "OFF");
    SINUCA3_LOG_PRINTF("Memory Management Unit: [%s]\n\n",
                       this->mmu != NULL ? "ON" : "OFF");
    return 0;
}

int SimpleCore::EstablishCoreID() {
    int coreId = this->nextCoreID;
    if (coreId < 0) {
        SINUCA3_ERROR_PRINTF("Core ID overflow for core [%p].\n", this);
        return 1;
    }
    this->coreId = coreId;
    SimpleCore::StepCoreID();
    return 0;
}

int SimpleCore::EstablishContextAndPropagate() {
    Context* context = Context::CreateContext();
    if (context == NULL) {
        SINUCA3_ERROR_PRINTF("Failed to create context for core [%d].\n",
                             this->coreId);
        return 1;
    }

    int contextId = this->engineConnectionID;
    int engineConnId = this->engineConnectionID;
    Linkable* engine = this->engine;
    context->SetCoreContext(contextId, engineConnId, engine);

    if (context->PropagateContext(this) != 0) {
        SINUCA3_ERROR_PRINTF(
            "Failed to propagate context for core [%d] and its children.\n",
            this->coreId);
        return 1;
    }

    return 0;
}

int SimpleCore::EstablishChildComponents() {
    if (this->mmu != NULL) this->AddChild(this->mmu);
    if (this->fetcher == NULL) {
        SINUCA3_ERROR_PRINTF(
            "Fetcher component is required for core [%d] but was not "
            "provided.\n",
            this->coreId);
        return 1;
    }
    this->AddChild(this->fetcher);
    return 0;
}

int SimpleCore::EstablishConnections(Config config) {
    if (config.ComponentReference("engine", &this->engine, true)) return 1;
    if (config.ComponentReference("instructionMemory", &this->instructionMemory, true)) return 1;
    if (config.ComponentReference("dataMemory", &this->dataMemory, true)) return 1;
    if (config.ComponentReference("mmu", &this->mmu, true)) return 1;
    if (config.ComponentReference("fetcher", &this->fetcher, true)) return 1;

    if (this->instructionMemory != NULL)
        this->instructionConnectionID = this->instructionMemory->Connect(0);
    if (this->dataMemory != NULL)
        this->dataConnectionID = this->dataMemory->Connect(0);
    if (this->mmu != NULL) {
        this->mmuConnectionID = this->mmu->Connect(0);
    }
    this->fetcherConnectionID = this->fetcher->Connect(0);
    this->engineConnectionID = this->engine->Connect(0);
    return 0;
}

void SimpleCore::Clock() {
    /* The fetching responsibility migrated to the fetcher component, so this
     * simple core is only responsible for establishing the simulated core
     * environment for now. */
    /*
        FetchPacket fetch;
        fetch.request = 0;
        this->fetcher->SendRequest(this->fetcherConnectionID, &fetch);
        if (this->fetcher->ReceiveResponse(this->fetcherConnectionID, &fetch) ==
            0) {
            ++this->numFetchedInstructions;
            if (this->instructionMemory != NULL) {
                MemoryPacket fetchPacket =
       fetch.response.staticInfo->instAddress;
                this->instructionMemory->SendRequest(this->instructionConnectionID,
                                                     &fetchPacket);
                if (this->dataMemory != NULL) {
                    for (long i = 0; i < fetch.response.dynamicInfo.numReadings;
                         ++i) {
                        this->dataMemory->SendRequest(
                            this->dataConnectionID,
                            (unsigned long*)&(
                                fetch.response.dynamicInfo.readsAddr[i]));
                    }

                    for (long i = 0; i < fetch.response.dynamicInfo.numWritings;
                         ++i) {
                        this->dataMemory->SendRequest(
                            this->dataConnectionID,
                            (unsigned long*)&(
                                fetch.response.dynamicInfo.writesAddr[i]));
                    }
                }
            }
        }
    */
}

void SimpleCore::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Core [%d]: [%lu] instructions fetched\n", this->coreId,
                       this->numFetchedInstructions);
}

SimpleCore::~SimpleCore() {}

// C++ requires the definition of static members outside the class declaration.
int SimpleCore::nextCoreID = 0;
