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

#include "../../utils/logging.hpp"

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
                       unsigned long target,
                       const sinuca::StaticInstructionInfo* instruction) {
    if (bank >= this->numBanks) return 1;

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
      numQueries(0),
      interleavingFactor(0),
      numEntries(0),
      interleavingBits(0),
      entriesBits(0) {};

int BranchTargetBuffer::SetConfigParameter(const char* parameter,
                                           sinuca::config::ConfigValue value) {
    if (strcmp(parameter, "interleavingFactor") == 0) {
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

        return 0;
    } else if (strcmp(parameter, "numberOfEntries") == 0) {
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

        return 0;
    } else if (strcmp(parameter, "sendTo") == 0) {
        if (value.type != sinuca::config::ConfigValueTypeComponentReference) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter sendTo is not a Component<BTBPacket>.\n");
            return 1;
        }
        this->sendTo = dynamic_cast<sinuca::Component<BTBPacket>*>(
            value.value.componentReference);
        if (this->sendTo == NULL) {
            SINUCA3_ERROR_PRINTF(
                "BTB parameter sendTo is not a Component<BTBPacket>.\n");
            return 1;
        }
        return 0;
    }

    SINUCA3_WARNING_PRINTF("BTB received an unknown parameter: %s.\n",
                           parameter);
    return 1;
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

    if (this->sendTo != NULL) {
        this->sendToID = this->sendTo->Connect(0);
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
    const sinuca::StaticInstructionInfo* instruction, unsigned long target) {
    unsigned long index = this->CalculateIndex(instruction->opcodeAddress);
    unsigned long tag = this->CalculateTag(instruction->opcodeAddress);
    unsigned int bank = this->CalculateBank(instruction->opcodeAddress);

    return this->btb[index]->NewEntry(tag, bank, target, instruction);
}

int BranchTargetBuffer::UpdateBranch(
    const sinuca::StaticInstructionInfo* instruction, bool branchState) {
    unsigned long index = this->CalculateIndex(instruction->opcodeAddress);
    unsigned int bank = this->CalculateBank(instruction->opcodeAddress);

    return this->btb[index]->UpdateEntry(bank, branchState);
}

inline void BranchTargetBuffer::Query(
    const sinuca::StaticInstructionInfo* instruction, int connectionID) {
    unsigned long index = this->CalculateIndex(instruction->opcodeAddress);
    unsigned long tag = this->CalculateTag(instruction->opcodeAddress);
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
        response.data.response.instruction = instruction;
        response.data.response.target =
            instruction->opcodeAddress + this->interleavingFactor;
        response.data.response.numberOfBits = this->interleavingFactor;

        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            if (!(branchTaken)) {
                if ((currentEntry->GetBranchType(i) ==
                     BranchTypeUnconditional) ||
                    (currentEntry->GetPrediction(i) == TAKEN)) {
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
            instruction->opcodeAddress + this->interleavingFactor;
        response.data.response.numberOfBits = this->interleavingFactor;
        for (unsigned int i = 0; i < this->interleavingFactor; ++i) {
            response.data.response.validBits[i] = true;
        }
        response.type = BTBPacketTypeResponseBTBMiss;
    }

    if (this->sendTo == NULL) {
        this->SendResponseToConnection(connectionID, &response);
    } else {
        this->sendTo->SendRequest(this->sendToID, &response);
    }
}

inline int BranchTargetBuffer::AddEntry(
    const sinuca::StaticInstructionInfo* instruction,
    unsigned long targetAddress) {
    return this->RegisterNewBranch(instruction, targetAddress);
}

inline int BranchTargetBuffer::Update(
    const sinuca::StaticInstructionInfo* instruction, bool branchState) {
    return this->UpdateBranch(instruction, branchState);
}

void BranchTargetBuffer::Clock() {
    long numberOfConnections = this->GetNumberOfConnections();
    BTBPacket packet;
    for (long i = 0; i < numberOfConnections; ++i) {
        if (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case BTBPacketTypeRequestQuery:
                    ++this->numQueries;
                    this->Query(packet.data.requestQuery, i);
                    break;
                case BTBPacketTypeRequestAddEntry:
                    this->AddEntry(packet.data.requestAddEntry.instruction,
                                   packet.data.requestAddEntry.target);
                    break;
                case BTBPacketTypeRequestUpdate:
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

void BranchTargetBuffer::Flush() {};

void BranchTargetBuffer::PrintStatistics() {
    SINUCA3_LOG_PRINTF("BranchTargetBuffer %p: %lu queries", this,
                       this->numQueries);
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
