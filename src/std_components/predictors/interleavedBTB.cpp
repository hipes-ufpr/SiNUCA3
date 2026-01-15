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
#include <cstdio>
#include <sinuca3.hpp>

BTBEntry::BTBEntry()
    : valid(false),
      numBanks(0),
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

    this->predictorsArray = new BimodalCounter[numBanks];
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
                       unsigned long target,
                       const StaticInstructionInfo* instruction) {
    if (bank >= this->numBanks) return 1;

    this->valid = true;
    this->entryTag = tag;
    this->targetArray[bank] = target;
    this->branchTypes[bank] =
        BranchTypeFromSinucaBranch(instruction->branchType);

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
      sendTo(NULL),
      sendToID(0),
      interleavingFactor(0),
      numEntries(0),
      interleavingBits(0),
      entriesBits(0),
      btbHits(0),
      totalBranch(0),
      queries(0),
      occupation(0),
      replacements(0) {};

int BranchTargetBuffer::Configure(Config config) {
    long interleavingFactor;
    if (config.Integer("interleavingFactor", &interleavingFactor, true))
        return 1;
    if (interleavingFactor <= 0)
        return config.Error("interleavingFactor", "is not > 0.");
    if (interleavingFactor > MAX_INTERLEAVING_FACTOR)
        return config.Error("interleavingFactor",
                            "is > MAX_INTERLEAVING_FACTOR");
    this->interleavingFactor = interleavingFactor;

    long numberOfEntries;
    if (config.Integer("numberOfEntries", &numberOfEntries, true)) return 1;
    if (numberOfEntries <= 0)
        return config.Error("numberOfEntries", "is not > 0.");
    this->numEntries = numberOfEntries;

    this->sendTo = NULL;
    if (config.ComponentReference("sendTo", &this->sendTo)) return 1;
    if (this->sendTo != NULL) this->sendToID = this->sendTo->Connect(0);

    this->interleavingBits = floor(log2(this->interleavingFactor));
    this->entriesBits = floor(log2(this->numEntries));
    this->interleavingFactor = (1 << this->interleavingBits);
    this->numEntries = (1 << this->entriesBits);
    this->btb = new BTBEntry*[this->numEntries];

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

int BranchTargetBuffer::RegisterNewBranch(
    const StaticInstructionInfo* instruction, unsigned long target) {
    unsigned long index = this->CalculateIndex(instruction->instAddress);
    unsigned long tag = this->CalculateTag(instruction->instAddress);
    unsigned int bank = this->CalculateBank(instruction->instAddress);

    if (this->btb[index]->GetValid()) {
        this->replacements++;
    }

    return this->btb[index]->NewEntry(tag, bank, target, instruction);
}

int BranchTargetBuffer::UpdateBranch(const StaticInstructionInfo* instruction,
                                     bool branchState) {
    unsigned long index = this->CalculateIndex(instruction->instAddress);
    unsigned int bank = this->CalculateBank(instruction->instAddress);

    return this->btb[index]->UpdateEntry(bank, branchState);
}

inline void BranchTargetBuffer::Query(const StaticInstructionInfo* instruction,
                                      int connectionID) {
    this->queries++;
    unsigned long index = this->CalculateIndex(instruction->instAddress);
    unsigned long tag = this->CalculateTag(instruction->instAddress);
    BTBPacket response;

    response.data.response.instruction = instruction;
    response.data.response.target =
        instruction->instAddress + ((this->interleavingFactor == 1)
                                          ? instruction->instSize
                                          : (this->interleavingFactor));
    response.data.response.numberOfInstructions = this->interleavingFactor;
    response.data.response.interleavingBits = this->interleavingBits;

    BTBEntry* currentEntry = btb[index];
    if (currentEntry->GetTag() == tag) {
        this->btbHits++;
        // BTB Hit
        /*
         * In a BTB hit, searches for a taken branch instruction and fills in
         * the target address if it finds one. Marks all instructions before the
         * taken branch as valid.
         */
        bool branchTaken = false;
        response.data.response.instruction = instruction;
        response.data.response.target =
            instruction->instAddress + this->interleavingFactor;
        
        // TODO: change numberOfBits field; not present in struct response
        // response.data.response.numberOfBits = this->interleavingFactor;

        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            if (!(branchTaken)) {
                if ((currentEntry->GetBranchType(i) ==
                     BranchTypeUnconditional) ||
                    (currentEntry->GetPrediction(i))) {
                    response.data.response.target =
                        currentEntry->GetTargetAddress(i);
                    branchTaken = true;
                }
                response.data.response.validBits[i] = true;
            } else {
                response.data.response.validBits[i] = false;
            }
        }
        response.type = BTBPacketTypeResponseBTBHit;
    } else {
        // BTB Miss
        /*
         * In a BTB Miss, it assumes that all instructions are valid and that
         * the next fetch block is sequential.
         */
        response.data.response.instruction = instruction;
        response.data.response.target =
            instruction->instAddress + this->interleavingFactor;
        // TODO: change numberOfBits field; not present in struct response
        // response.data.response.numberOfBits = this->interleavingFactor;
        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            response.data.response.validBits[i] = true;
        }
        response.type = BTBPacketTypeResponseBTBMiss;
    }

    this->SendResponseToConnection(connectionID, &response);
    if (this->sendTo != NULL) {
        this->sendTo->SendRequest(this->sendToID, &response);
    }
}

inline int BranchTargetBuffer::AddEntry(
    const StaticInstructionInfo* instruction, unsigned long targetAddress) {
    this->totalBranch++;
    return this->RegisterNewBranch(instruction, targetAddress);
}

inline int BranchTargetBuffer::Update(const StaticInstructionInfo* instruction,
                                      bool branchState) {
    return this->UpdateBranch(instruction, branchState);
}

void BranchTargetBuffer::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    BTBPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case BTBPacketTypeRequestQuery:
                    SINUCA3_DEBUG_PRINTF(
                        "[BranchTargetBuffer] %p: consulting instruction [%lx] "
                        "%s\n",
                        this, packet.data.requestQuery->instAddress,
                        packet.data.requestQuery->instMnemonic);
                    this->Query(packet.data.requestQuery, i);
                    break;

                case BTBPacketTypeRequestAddEntry:
                    SINUCA3_DEBUG_PRINTF(
                        "[BranchTargetBuffer] %p: adding entry [%lx] %s\n",
                        this, packet.data.requestQuery->instAddress,
                        packet.data.requestQuery->instMnemonic);
                    this->AddEntry(packet.data.requestAddEntry.instruction,
                                   packet.data.requestAddEntry.target);
                    break;

                case BTBPacketTypeRequestUpdate:
                    SINUCA3_DEBUG_PRINTF(
                        "[BranchTargetBuffer] %p: updating [%lx] %s\n", this,
                        packet.data.requestQuery->instAddress,
                        packet.data.requestQuery->instMnemonic);
                    this->Update(packet.data.requestUpdate.instruction,
                                 packet.data.requestUpdate.branchState);
                    break;

                case BTBPacketTypeResponseBTBHit:
                case BTBPacketTypeResponseBTBMiss:
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

void BranchTargetBuffer::PrintStatistics() {
    for (unsigned int i = 0; i < this->numEntries; ++i) {
        if (this->btb[i]->GetValid()) this->occupation++;
    }

    SINUCA3_LOG_PRINTF("Branch Target Buffer [%p]\n", this);
    SINUCA3_LOG_PRINTF("    Entries: %d\n", this->numEntries);
    SINUCA3_LOG_PRINTF("    Total Queries: %lu\n", this->queries);
    SINUCA3_LOG_PRINTF("    Total Branches: %lu\n", this->totalBranch);
    SINUCA3_LOG_PRINTF("    BTB Hits: %lu\n", this->btbHits);
    SINUCA3_LOG_PRINTF("    BTB Occupation: %lu\n", this->occupation);
    SINUCA3_LOG_PRINTF("    Entry Replacements: %lu\n", this->replacements);
    SINUCA3_LOG_PRINTF("    Hit Ratio: %lf%%\n",
                       ((double)this->btbHits / (double)this->queries) * 100);
    SINUCA3_LOG_PRINTF(
        "    Occupation Ratio: %lf%%\n",
        ((double)this->occupation / (double)this->numEntries) * 100);
}

BranchTargetBuffer::~BranchTargetBuffer() {
    if (!(this->btb)) return;

    for (unsigned int i = 0; i < this->numEntries; ++i) {
        if (this->btb[i]) {
            delete this->btb[i];
        }
    }

    delete[] this->btb;
}
