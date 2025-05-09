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

#include "interleavedBTB.hpp"

#include <cmath>

#include "../utils/logging.hpp"

BTBEntry::BTBEntry()
    : numBanks(0),
      entryTag(0),
      targetArray(NULL),
      branchTypes(NULL),
      predictorsArray(NULL) {}

int BTBEntry::Allocate(unsigned int numBanks) {
    this->numBanks = numBanks;
    this->targetArray = new unsigned long[numBanks];
    if (!(this->targetArray)) {
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    this->branchTypes = new BranchType[numBanks];
    if (!(this->branchTypes)) {
        delete[] this->targetArray;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    this->predictorsArray = new BimodalPredictor[numBanks];
    if (!(this->predictorsArray)) {
        delete[] this->targetArray;
        delete[] this->branchTypes;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    for (unsigned int i = 0; i < this->numBanks; ++i) {
        this->targetArray[i] = 0;
        this->branchTypes[i] = BranchTypeNone;
    }

    return 0;
}

int BTBEntry::NewEntry(unsigned long tag, unsigned int bank,
                       unsigned long targetAddress, BranchType type) {
    if (bank >= this->numBanks) return 1;

    this->entryTag = tag;
    this->targetArray[bank] = targetAddress;
    this->branchTypes[bank] = type;

    return 0;
}

int BTBEntry::UpdateEntry(unsigned long bank, bool branchState) {
    if ((bank >= this->numBanks) || this->entryTag == 0) return 1;

    this->predictorsArray[bank].UpdatePrediction(branchState);
    return 0;
}

BTBEntry::~BTBEntry() {
    if (this->targetArray) {
        delete[] this->targetArray;
    }

    if (this->branchTypes) {
        delete[] this->branchTypes;
    }

    if (this->predictorsArray) {
        delete[] this->predictorsArray;
    }
}

BranchTargetBuffer::BranchTargetBuffer()
    : btb(NULL),
      interleavingFactor(0),
      numEntries(0),
      interleavingBits(0),
      entriesBits(0){};

int BranchTargetBuffer::SetConfigParameter(const char* parameter,
                                           sinuca::config::ConfigValue value) {
    bool isInterleavingFactor = (strcmp(parameter, "interleavingFactor") == 0);
    bool isNumberOfEntries = (strcmp(parameter, "numberOfEntries") == 0);

    if (!(isInterleavingFactor) || !(isNumberOfEntries)) {
        SINUCA3_WARNING_PRINTF("BTB received an unknown parameter: %s.\n",
                               parameter);
        return 1;
    }

    if (isInterleavingFactor) {
        if (value.type != sinuca::config::ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter interleavingFactor is not an integer.\n");
            return 1;
        }
        unsigned int iFactor = value.value.integer;
        if (iFactor <= 0) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter interleavingFactor must be > 0.\n");
            return 1;
        }

        if (iFactor > MAX_INTERLEAVING_FACTOR) {
            this->interleavingFactor = MAX_INTERLEAVING_FACTOR;
        } else {
            this->interleavingFactor = iFactor;
        }
    }

    if (isNumberOfEntries) {
        if (value.type != sinuca::config::ConfigValueTypeInteger) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter numberOfEntries is not an integer.\n");
            return 1;
        }
        unsigned int entries = value.value.integer;
        if (entries <= 0) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter numberOfEntries must be > 0.\n");
            return 1;
        }
        this->numEntries = entries;
    }

    return 0;
}

int BranchTargetBuffer::FinishSetup() {
    if (this->interleavingFactor == 0) {
        SINUCA3_ERROR_PRINTF(
            "BTB did not receive the interleaving factor parameter.\n");
        return 1;
    }

    if (this->numEntries == 0) {
        SINUCA3_ERROR_PRINTF(
            "BTB did not receive the number of entries parameter.\n");
        return 1;
    }

    this->interleavingBits = floor(log(this->interleavingFactor));
    this->entriesBits = floor(log(this->numEntries));
    this->interleavingFactor = (1 << this->interleavingBits);
    this->numEntries = (1 << this->entriesBits);
    this->btb = new BTBEntry*[this->numEntries];
    if (!(this->btb)) {
        SINUCA3_ERROR_PRINTF("BTB could not be allocated.\n");
        return 1;
    }

    for (unsigned int i = 0; i < this->numEntries; ++i) {
        this->btb[i] = new BTBEntry;
        this->btb[i]->Allocate(this->interleavingFactor);
    }

    return 0;
}

unsigned int BranchTargetBuffer::CalculateBank(unsigned long address) {
    unsigned long bank = address;
    bank = bank & ((1 << this->interleavingBits) - 1);

    return bank;
}

unsigned long BranchTargetBuffer::CalculateTag(unsigned long address) {
    unsigned long tag = address;
    tag = tag >> this->interleavingBits;

    return tag;
}

unsigned long BranchTargetBuffer::CalculateIndex(unsigned long address) {
    unsigned long index = address;

    index = index >> this->interleavingBits;
    index = index & ((1 << this->entriesBits) - 1);

    return index;
}

int BranchTargetBuffer::RegisterNewBranch(unsigned long address,
                                          unsigned long targetAddress,
                                          BranchType type) {
    unsigned long index = this->CalculateIndex(address);
    unsigned long tag = this->CalculateTag(address);
    unsigned int bank = this->CalculateBank(address);

    return this->btb[index]->NewEntry(tag, bank, targetAddress, type);
}

int BranchTargetBuffer::UpdateBranch(unsigned long address, bool branchState) {
    unsigned long index = this->CalculateIndex(address);
    unsigned int bank = this->CalculateBank(address);

    return this->btb[index]->UpdateEntry(bank, branchState);
}

inline void BranchTargetBuffer::Query(unsigned long address, int connectionID) {
    unsigned long index = this->CalculateIndex(address);
    unsigned long tag = this->CalculateTag(address);
    BTBPacket response;

    BTBEntry* currentEntry = btb[index];
    if (currentEntry->GetTag() == tag) {
        // BTB Hit
        /*
         * In a BTB hit, searches for a taken branch instruction and fills in
         * the target address if it finds one. Marks all instructions before the
         * taken branch as valid.
         */
        bool branchTaken = false;
        response.data.responseQuery.address = address;
        response.data.responseQuery.targetAddress =
            address + this->interleavingFactor;
        response.data.responseQuery.numberOfBits = this->interleavingFactor;

        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            if (!(branchTaken)) {
                if ((currentEntry->GetBranchType(i) ==
                     BranchTypeUnconditionalBranch) ||
                    (currentEntry->GetPrediction(i) == TAKEN)) {
                    response.data.responseQuery.targetAddress =
                        currentEntry->GetTargetAddress(i);
                    branchTaken = true;
                }
                response.data.responseQuery.validBits[i] = true;
            } else {
                response.data.responseQuery.validBits[i] = false;
            }
        }
        response.type = ResponseBTBHit;
    } else {
        // BTB Miss
        /*
         * In a BTB Miss, it assumes that all instructions are valid and that
         * the next fetch block is sequential.
         */
        response.data.responseQuery.address = address;
        response.data.responseQuery.targetAddress =
            address + this->interleavingFactor;
        response.data.responseQuery.numberOfBits = this->interleavingFactor;
        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            response.data.responseQuery.validBits[i] = true;
        }
        response.type = ResponseBTBMiss;
    }

    this->SendResponseToConnection(connectionID, &response);
}

inline int BranchTargetBuffer::AddEntry(unsigned long address,
                                        unsigned long targetAddress,
                                        BranchType type) {
    return this->RegisterNewBranch(address, targetAddress, type);
}

inline int BranchTargetBuffer::Update(unsigned long address, bool branchState) {
    return this->UpdateBranch(address, branchState);
}

void BranchTargetBuffer::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    BTBPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case RequestQuery:
                    this->Query(packet.data.requestQuery.address, i);
                    break;
                case RequestAddEntry:
                    this->AddEntry(packet.data.requestAddEntry.address,
                                   packet.data.requestAddEntry.targetAddress,
                                   packet.data.requestAddEntry.typeOfBranch);
                    break;
                case RequestUpdate:
                    this->Update(packet.data.updateQuery.address,
                                 packet.data.updateQuery.branchState);
                    break;
                case ResponseBTBHit:
                case ResponseBTBMiss:
                    SINUCA3_WARNING_PRINTF(
                        "Connection %ld send a response type message to BTB.\n",
                        i);
                    break;
                default:
                    SINUCA3_WARNING_PRINTF(
                        "Connection %ld send a invalid message to BTB.\n", i);
            }
        }
    }
}

void BranchTargetBuffer::Flush() {};

BranchTargetBuffer::~BranchTargetBuffer() {
    if (!(this->btb)) return;

    for (unsigned int i = 0; i < this->numEntries; ++i) {
        if (this->btb[i]) {
            delete this->btb[i];
        }
    }

    delete[] this->btb;
}