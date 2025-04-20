#include "interleavedBTB.hpp"

#include <cmath>

#include "../utils/logging.hpp"

sinuca::btb_entry::btb_entry()
    : numBanks(0),
      entryTag(0),
      targetArray(NULL),
      branchTypes(NULL),
      predictorsArray(NULL) {}

int sinuca::btb_entry::Allocate(unsigned int numBanks) {
    this->numBanks = numBanks;
    targetArray = new unsigned long[numBanks];
    if (!(targetArray)) {
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    branchTypes = new branchType[numBanks];
    if (!(branchTypes)) {
        delete[] targetArray;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    predictorsArray = new BimodalPredictor[numBanks];
    if (!(predictorsArray)) {
        delete[] targetArray;
        delete[] branchTypes;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    for (unsigned int i = 0; i < numBanks; ++i) {
        targetArray[i] = 0;
        branchTypes[i] = NONE;
    }

    return 0;
}

int sinuca::btb_entry::NewEntry(unsigned long tag, unsigned int bank,
                                unsigned long targetAddress,
                                sinuca::branchType type) {
    if (bank >= this->numBanks) return 1;

    this->entryTag = tag;
    this->targetArray[bank] = targetAddress;
    this->branchTypes[bank] = type;

    return 0;
}

int sinuca::btb_entry::UpdateEntry(unsigned long bank, bool branchState) {
    if ((bank >= this->numBanks) || this->entryTag == 0) return 1;

    this->predictorsArray[bank].UpdatePrediction(branchState);
    return 0;
}

long sinuca::btb_entry::GetTag() { return this->entryTag; }

long sinuca::btb_entry::GetTargetAddress(unsigned int bank) {
    if (bank < this->numBanks) return this->targetArray[bank];

    return 0;
}

sinuca::branchType sinuca::btb_entry::GetBranchType(unsigned int bank) {
    if (bank < this->numBanks) return this->branchTypes[bank];

    return NONE;
}

bool sinuca::btb_entry::GetPrediction(unsigned int bank) {
    if (bank < this->numBanks) {
        return predictorsArray[bank].GetPrediction();
    }

    return false;
}

sinuca::btb_entry::~btb_entry() {
    if (targetArray) {
        delete[] targetArray;
    }

    if (branchTypes) {
        delete[] branchTypes;
    }

    if (predictorsArray) {
        delete[] predictorsArray;
    }
}

sinuca::BranchTargetBuffer::BranchTargetBuffer()
    : btb(NULL),
      interleavingFactor(0),
      numEntries(0),
      interleavingBits(0),
      entriesBits(0){};

int sinuca::BranchTargetBuffer::SetConfigParameter(
    const char* parameter, sinuca::config::ConfigValue value) {
    if ((strcmp(parameter, "interleavingFactor") != 0) ||
        (strcmp(parameter, "numberOfEntries") != 0)) {
        SINUCA3_WARNING_PRINTF("BTB received an unknown parameter: %s.\n",
                               parameter);
        return 1;
    }

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
        this->interleavingFactor = iFactor;
    }

    if (strcmp(parameter, "numberOfEntries") == 0) {
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

int sinuca::BranchTargetBuffer::FinishSetup() {
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
    this->btb = new btb_entry*[numEntries];
    if (!(this->btb)) {
        SINUCA3_ERROR_PRINTF("BTB could not be allocated.\n");
        return 1;
    }

    for (unsigned int i = 0; i < this->numEntries; ++i) {
        this->btb[i] = new btb_entry;
        this->btb[i]->Allocate(interleavingFactor);
    }

    return 0;
}

sinuca::BranchTargetBuffer::~BranchTargetBuffer() {
    if (!(this->btb)) return;

    for (unsigned int i = 0; i < this->numEntries; ++i) {
        if (this->btb[i]) {
            delete this->btb[i];
        }
    }

    delete[] this->btb;
}

unsigned int sinuca::BranchTargetBuffer::CalculateBank(unsigned long address) {
    unsigned long bank = address;
    bank = bank & ((1 << this->interleavingBits) - 1);

    return bank;
}

unsigned long sinuca::BranchTargetBuffer::CalculateTag(unsigned long address) {
    unsigned long tag = address;
    tag = tag >> this->interleavingBits;

    return tag;
}

unsigned long sinuca::BranchTargetBuffer::CalculateIndex(
    unsigned long address) {
    unsigned long index = address;

    index = index >> this->interleavingBits;
    index = index & ((1 << this->entriesBits) - 1);

    return index;
}

int sinuca::BranchTargetBuffer::RegisterNewBranch(unsigned long address,
                                                  unsigned long targetAddress,
                                                  branchType type) {
    unsigned long index = CalculateIndex(address);
    unsigned long tag = CalculateTag(address);
    unsigned int bank = CalculateBank(address);

    return this->btb[index]->NewEntry(tag, bank, targetAddress, type);
}

int sinuca::BranchTargetBuffer::UpdateBranch(unsigned long address,
                                             bool branchState) {
    unsigned long index = CalculateIndex(address);
    unsigned int bank = CalculateBank(address);

    return this->btb[index]->UpdateEntry(bank, branchState);
}