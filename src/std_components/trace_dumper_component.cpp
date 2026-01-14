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
 * @file trace_dumper_component.cpp
 * @brief A component that prints information of every instruction it receives.
 */

#include "trace_dumper_component.hpp"

#include <cstring>
#include <sinuca3.hpp>

void TraceDumperComponent::Override(const char* instruction) {
    int size = strlen(instruction) + 1;
    const char* str = new char[size];
    memcpy((void*)str, (void*)instruction, size);
    this->overrides.push_back(str);
}

int TraceDumperComponent::Configure(Config config) {
    if (config.ComponentReference("fetch", &this->fetch, true)) return 1;
    if (config.Bool("default", &this->def)) return 1;

    this->fetchID = this->fetch->Connect(0);

    // Use raw yaml to get the overrides.
    yaml::YamlValue value;
    Map<yaml::YamlValue>* rawConfig = config.RawYaml();
    rawConfig->ResetIterator();
    for (const char* key = rawConfig->Next(&value); key != NULL;
         key = rawConfig->Next(&value)) {
        if (strcmp(key, "fetch") && strcmp(key, "default")) this->Override(key);
    }

    return 0;
}

bool TraceDumperComponent::IsOverride(const char* instruction) {
    for (unsigned int i = 0; i < this->overrides.size(); ++i) {
        if (strcmp(instruction, this->overrides[i]) == 0) return true;
    }
    return false;
}

void TraceDumperComponent::Clock() {
    FetchPacket fetch;
    fetch.request = 0;
    this->fetch->SendRequest(this->fetchID, &fetch);
    if (this->fetch->ReceiveResponse(this->fetchID, &fetch) == 0) {
        InstructionPacket instruction = fetch.response;
        if (this->def ^ this->IsOverride(instruction.staticInfo->instMnemonic)) {
            ++this->fetched;
          SINUCA3_LOG_PRINTF("TraceDumperComponent %p: Fetched {\n", this);
            SINUCA3_LOG_PRINTF("  instMnemonic: %s\n",
                               instruction.staticInfo->instMnemonic);
            SINUCA3_LOG_PRINTF("  instAddress: %ld\n",
                               instruction.staticInfo->instAddress);
            SINUCA3_LOG_PRINTF("  instSize: %lu\n",
                               instruction.staticInfo->instSize);
          SINUCA3_LOG_PRINTF("  readRegs: [");
            for (unsigned char i = 0; i < instruction.staticInfo->numberOfReadRegs;
                 ++i) {
                SINUCA3_LOG_PRINTF("%u", instruction.staticInfo->readRegsArray[i]);
                if (i + 1 < instruction.staticInfo->numberOfReadRegs)
                    SINUCA3_LOG_PRINTF(", ");
            }
            SINUCA3_LOG_PRINTF("]\n");
          SINUCA3_LOG_PRINTF("  writeRegs: [");
            for (unsigned char i = 0; i < instruction.staticInfo->numberOfWriteRegs;
                 ++i) {
                SINUCA3_LOG_PRINTF("%u", instruction.staticInfo->writtenRegsArray[i]);
                if (i + 1 < instruction.staticInfo->numberOfWriteRegs)
                    SINUCA3_LOG_PRINTF(", ");
            }
            SINUCA3_LOG_PRINTF("]\n");
          SINUCA3_LOG_PRINTF(
                "  branchType: %d\n",
                static_cast<int>(instruction.staticInfo->branchType));
            SINUCA3_LOG_PRINTF(
                "  isIndirect: %s\n",
                instruction.staticInfo->isIndirectControlFlowInst ? "true" : "false");
            SINUCA3_LOG_PRINTF(
                "  isPredicated: %s\n",
                instruction.staticInfo->isPredicatedInst ? "true" : "false");
            SINUCA3_LOG_PRINTF(
                "  isPrefetch: %s\n",
                instruction.staticInfo->isPrefetchHintInst ? "true" : "false");
            SINUCA3_LOG_PRINTF("}\n");
        }
    }
}

void TraceDumperComponent::PrintStatistics() {
    SINUCA3_LOG_PRINTF("TraceDumperComponent %p: fetched %lu instructions.\n",
                       this, this->fetched);
}

TraceDumperComponent::~TraceDumperComponent() {
    for (unsigned int i = 0; i < this->overrides.size(); ++i)
        delete[] this->overrides[i];
}
