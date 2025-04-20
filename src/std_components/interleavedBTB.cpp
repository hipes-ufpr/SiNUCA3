#include "interleavedBTB.hpp"

#include "../utils/logging.hpp"

sinuca::btb_entry::btb_entry()
    : entryTag(-1),
      interleavingFactor(0),
      targetArray(nullptr),
      branchTypes(nullptr),
      predictorsArray(nullptr) {}

int sinuca::btb_entry::Allocate(unsigned int interleavingFactor) {
    this->interleavingFactor = interleavingFactor;
    targetArray = new unsigned long[interleavingFactor];
    if (!(targetArray)) {
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    branchTypes = new branchType[interleavingFactor];
    if (!(branchTypes)) {
        delete[] targetArray;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    predictorsArray = new BimodalPredictor[interleavingFactor];
    if (!(predictorsArray)) {
        delete[] targetArray;
        delete[] branchTypes;
        SINUCA3_ERROR_PRINTF("BTB entry could not be allocated");
        return 1;
    }

    for (int i = 0; i < interleavingFactor; ++i) {
        targetArray[i] = 0;
        branchTypes[i] = NONE;
    }

    return 0;
}

int sinuca::btb_entry::NewEntry(unsigned long tag, unsigned long bank,
                                long targetAddress, sinuca::branchType type) {
    if (bank >= this->interleavingFactor) return 1;

    this->entryTag = tag;
    this->targetArray[bank] = targetAddress;
    this->branchTypes[bank] = type;

    return 0;
}

int sinuca::btb_entry::UpdateEntry(unsigned long bank, bool branchState) {
    if (bank >= this->interleavingFactor) return 1;

    this->predictorsArray[bank].UpdatePrediction(branchState);
    return 0;
}

long sinuca::btb_entry::GetTag() { return this->entryTag; }

long sinuca::btb_entry::GetTargetAddress(unsigned int bank) {
    if (bank < this->interleavingFactor) return this->targetArray[bank];

    return 0;
}

sinuca::branchType sinuca::btb_entry::GetBranchType(unsigned int bank) {
    if (bank < this->interleavingFactor) return this->branchTypes[bank];

    return NONE;
}

bool sinuca::btb_entry::GetPrediction(unsigned int bank) {
    if (bank < this->interleavingFactor) {
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

sinuca::BranchTargetBuffer::BranchTargetBuffer() : Component<BTBMessage>(){};

uint32_t sinuca::BranchTargetBuffer::CalculateTag(uint32_t fetchAddress) {
    return 0;
}

uint32_t sinuca::BranchTargetBuffer::CalculateIndex(uint32_t fetchAddress) {
    uint32_t index = fetchAddress;
    return index;
}
