#include "interleavedBTB.hpp"
#include <cstdint>

btb_entry::btb_entry() : validBit(false), simplePredictor(nullptr) {};

void btb_entry::Allocate() {
    validBit = false;
    tag = 0;
    fetchTarget = 0;
    simplePredictor = new BimodalPredictor();
};

bool btb_entry::GetValid() {
    return validBit;
};

uint32_t btb_entry::GetTag() {
    return tag;
};

uint32_t btb_entry::GetTarget() {
    return fetchTarget;
};

bool btb_entry::GetPrediction() {
    return simplePredictor->GetPrediction();
};

void btb_entry::SetEntry(uint32_t tag, uint32_t fetchTarget) {
    this->validBit = true;
    this->tag = tag;
    this->fetchTarget = fetchTarget;
};

void btb_entry::UpdatePrediction(bool branchTaken) {
    simplePredictor->UpdatePrediction(branchTaken);
};

btb_entry::~btb_entry() {
    delete simplePredictor;
};

BranchTargetBuffer::BranchTargetBuffer() : Component<BTBMessage>(), instructionValidBits(nullptr), banks(nullptr) {};

uint32_t BranchTargetBuffer::CalculateTag(uint32_t fetchAddress) {
    uint32_t tag = fetchAddress;
    tag = tag >> numBanks;

    return tag;
};

uint32_t BranchTargetBuffer::CalculateIndex(uint32_t fetchAddress) {
    uint32_t index = fetchAddress;
    index = index >> numBanks;
    index = index & ((1 << numEntries) - 1);

    return index;
};

void BranchTargetBuffer::Allocate(uint32_t numBanks, uint32_t numEntries) {
    this->numBanks = numBanks;
    this->numEntries = numEntries;
    this->totalBranches = 0;
    this->totalHits = 0;
    this->nextFetchBlock = 0;

    int totalBanks = (1 << numBanks);
    int totalEntries = (1 << numEntries);
    this->instructionValidBits = new bool[totalBanks];
    this->banks = new btb_bank[totalBanks];
    for (int bank = 0; bank < totalBanks; ++bank) {
        this->instructionValidBits[bank] = false;
        this->banks[bank] = new btb_entry[totalEntries];

        for (int entries = 0; entries < totalEntries; ++entries) {
            this->banks[bank][entries].Allocate();
        }
    }
};

uint32_t BranchTargetBuffer::GetNextFetchBlock() {
    return nextFetchBlock;
};

bool* BranchTargetBuffer::GetInstructionValidBits() {
    return instructionValidBits;
};

void BranchTargetBuffer::RegisterNewBlock(uint32_t fetchAddress, uint32_t* fetchTargets) {
    uint32_t currentTag = CalculateTag(fetchAddress);
    uint32_t index = CalculateIndex(fetchAddress);

    for (uint32_t bank = 0; bank < numBanks; ++bank) {
        banks[bank][index].SetEntry(currentTag, fetchTargets[bank]);
    }
};

TypeBTBMessage BranchTargetBuffer::FetchBTBEntry(uint32_t fetchAddress) {
    bool alocated = true;
    uint32_t nextBlock = 0;
    uint32_t currentTag = CalculateTag(fetchAddress);
    uint32_t index = CalculateIndex(fetchAddress);

    for (uint32_t i = 0; i < numBanks; ++i) {
        if (banks[i][index].GetValid()) {
            if (banks[i][index].GetTag() == currentTag) {
                nextBlock = banks[i][index].GetTarget();
                instructionValidBits[i] = banks[i][index].GetPrediction();
            } else {
                alocated = false;
                instructionValidBits[i] = true;
            }
        } else {
            alocated = false;
            instructionValidBits[i] = true;
        }
    }

    if (nextBlock) {
        nextFetchBlock = nextBlock;
    } else {
        nextFetchBlock = fetchAddress + (1 << numBanks);
    }

    if (alocated) {
        return ALLOCATED_ENTRY;
    }

    return UNALLOCATED_ENTRY;
};

void BranchTargetBuffer::UpdateBlock(uint32_t fetchAddress, bool* executedInstructions) {
    uint32_t currentTag = CalculateTag(fetchAddress);
    uint32_t index = CalculateIndex(fetchAddress);

    for (uint32_t bank = 0; bank < numBanks; ++bank) {
        if (banks[bank][index].GetValid()) {
            if (banks[bank][index].GetTag() == currentTag) {
                banks[bank][index].UpdatePrediction(executedInstructions[bank]);
            }
        }
    }
};

BranchTargetBuffer::~BranchTargetBuffer() {
    if (instructionValidBits) {
        delete[] instructionValidBits;
        instructionValidBits = nullptr;
    }

    int totalBanks = (1 << numBanks);
    if (banks) {
        for (int i = 0; i < totalBanks; ++i) {
            delete[] banks[i];
        }
        delete[] banks;
        banks = nullptr;
    }
};