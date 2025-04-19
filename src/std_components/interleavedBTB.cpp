#include "interleavedBTB.hpp"
#include <cstdint>

btb_entry::btb_entry() : validBit(false), simplePredictor(nullptr) {};

void btb_entry::Allocate() {
    this->validBit = false;
    this->tag = 0;
    this->fetchTarget = 0;
    this->simplePredictor = new BimodalPredictor();
};

bool btb_entry::GetValid() {
    return this->validBit;
};

uint32_t btb_entry::GetTag() {
    return this->tag;
};

uint32_t btb_entry::GetTarget() {
    return this->fetchTarget;
};

bool btb_entry::GetPrediction() {
    return this->simplePredictor->GetPrediction();
};

void btb_entry::SetEntry(uint32_t tag, uint32_t fetchTarget) {
    this->validBit = true;
    this->tag = tag;
    this->fetchTarget = fetchTarget;
};

void btb_entry::UpdatePrediction(bool branchTaken) {
    this->simplePredictor->UpdatePrediction(branchTaken);
};

btb_entry::~btb_entry() {
    delete this->simplePredictor;
};

BranchTargetBuffer::BranchTargetBuffer() : Component<BTBMessage>(), instructionValidBits(nullptr), banks(nullptr) {};

uint32_t BranchTargetBuffer::CalculateTag(uint32_t fetchAddress) {
    uint32_t tag = fetchAddress;
    tag = tag >> this->numBanks; 
    return tag;
};

uint32_t BranchTargetBuffer::CalculateIndex(uint32_t fetchAddress) {
    uint32_t index = fetchAddress;
    index = index >> this->numBanks; 
    index = index & ((1 << this->numEntries) - 1); 
    return index;
};

void BranchTargetBuffer::Allocate(uint32_t numBanks, uint32_t numEntries) {
    this->numBanks = numBanks;
    this->numEntries = numEntries;
    this->totalBranches = 0;
    this->totalHits = 0;
    this->nextFetchBlock = 0;

    int totalBanks = (1 << this->numBanks); 
    int totalEntries = (1 << this->numEntries); 
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
    return this->nextFetchBlock;
};

bool* BranchTargetBuffer::GetInstructionValidBits() {
    return this->instructionValidBits;
};

void BranchTargetBuffer::RegisterNewBlock(uint32_t fetchAddress, uint32_t* fetchTargets) {
    uint32_t currentTag = this->CalculateTag(fetchAddress); 
    uint32_t index = this->CalculateIndex(fetchAddress); 

    for (uint32_t bank = 0; bank < numBanks; ++bank) {
        this->banks[bank][index].SetEntry(currentTag, fetchTargets[bank]);
    }
};

TypeBTBMessage BranchTargetBuffer::FetchBTBEntry(uint32_t fetchAddress) {
    bool alocated = true;
    uint32_t nextBlock = 0;
    uint32_t currentTag = this->CalculateTag(fetchAddress); 
    uint32_t index = this->CalculateIndex(fetchAddress); 

    for (uint32_t i = 0; i < this->numBanks; ++i) { 
        if (this->banks[i][index].GetValid()) { 
            if (this->banks[i][index].GetTag() == currentTag) {
                nextBlock = this->banks[i][index].GetTarget(); 
                this->instructionValidBits[i] = this->banks[i][index].GetPrediction(); 
            } else {
                alocated = false;
                this->instructionValidBits[i] = true; 
            }
        } else {
            alocated = false;
            this->instructionValidBits[i] = true; 
        }
    }

    if (nextBlock) {
        this->nextFetchBlock = nextBlock; 
    } else {
        this->nextFetchBlock = fetchAddress + (1 << this->numBanks); 
    }

    if (alocated) {
        return ALLOCATED_ENTRY;
    }

    return UNALLOCATED_ENTRY;
};

void BranchTargetBuffer::UpdateBlock(uint32_t fetchAddress, bool* executedInstructions) {
    uint32_t currentTag = this->CalculateTag(fetchAddress); 
    uint32_t index = this->CalculateIndex(fetchAddress); 

    for (uint32_t bank = 0; bank < this->numBanks; ++bank) { 
        if (this->banks[bank][index].GetValid()) { 
            if (this->banks[bank][index].GetTag() == currentTag) { 
                this->banks[bank][index].UpdatePrediction(executedInstructions[bank]); 
            }
        }
    }
};

BranchTargetBuffer::~BranchTargetBuffer() {
    if (this->instructionValidBits) { 
        delete[] this->instructionValidBits; 
        this->instructionValidBits = nullptr; 
    }

    int totalBanks = (1 << this->numBanks); 
    if (this->banks) { 
        for (int i = 0; i < totalBanks; ++i) {
            delete[] this->banks[i]; 
        }
        delete[] this->banks; 
    }
};