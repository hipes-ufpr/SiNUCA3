#ifndef SINUCA3_INTERLEAVED_BTB_HPP
#define SINUCA3_INTERLEAVED_BTB_HPP

/**
 * @file interleavedBTB.hpp
 * @brief Implementation of Interleaved Branch Target Buffer
 */
#include <cstdint>
#include "../engine/component.hpp"
#include "../utils/bimodalPredictor.hpp"

enum TypeBTBMessage {
    BTB_REQUEST,
    UNALLOCATED_ENTRY,
    ALLOCATED_ENTRY,
    BTB_ALLOCATION_REQUEST,
    BTB_UPDATE_REQUEST
};

struct BTBMessage {
    int channelID;
    uint32_t fetchAddress;
    uint32_t nextBlock;
    uint32_t* fetchTargets;
    bool* validBits;
    bool* executedInstructions;
    TypeBTBMessage messageType;
};

struct btb_entry;
typedef btb_entry* btb_bank;

struct btb_entry {
    private:
        bool validBit;
        uint32_t tag;
        uint32_t fetchTarget;
        BimodalPredictor* simplePredictor;

    public:
        btb_entry();

        /**
         * @brief Allocates the BTB entry
         */
        void Allocate();

        /**
         * @brief Gets the valid bit of entry
         */
        bool GetValid();

        /** 
         * @brief Gets the tag of the entry
         */
        uint32_t GetTag();

        /**
         * @brief Gets the fetch target
         */
        uint32_t GetTarget();

        /**
         * @brief Wrapper to TwoBitPredictor Method
         */
        bool GetPrediction();

        /**
         * @brief Defines the input fields
         */
        void SetEntry(uint32_t tag, uint32_t fetchTarget);

        /**
         * @brief Wrapper to TwoBitPredictor Method
         */
        void UpdatePrediction(bool branchTaken);

        ~btb_entry();
};

class BranchTargetBuffer : public sinuca::Component<BTBMessage> {
    private:
        uint32_t totalBranches;
        uint32_t totalHits;
        uint32_t nextFetchBlock;
        bool* instructionValidBits;
        btb_bank* banks;
        uint32_t numBanks, numEntries;

        /**
         * @brief Calculates the tag used to verify the BTB entry
         * @param FetchAddress Address used to access BTB
         * @details This method aligns the address with the interleaving factor and returns the value
         */
        uint32_t CalculateTag(uint32_t fetchAddress);
        /**
         * @brief Calculate the index to access the correct BTB entry
         * @param fetchAddress Address used to access BTB
         * @details The method calculates an index within a fixed range by applying bitwise shifts and masks. 
         * Aligning the fetch address with the interleaving factor and obtaining the index of the respective BTB entry for the fetch address.
         * @return The index to access BTB
         */
        uint32_t CalculateIndex(uint32_t fetchAddress);
    public:
        BranchTargetBuffer();

        /**
         * @brief Allocate the BTB
         * @param numBanks Number of bits used to index the banks (2 bits = 4 banks)
         * @param numEntries Number of bits used to index the entries (8 bits = 256 entries)
         */
        void Allocate(uint32_t numBanks, uint32_t numEntries);

        /**
         * @return The address of next instruction block
         */
        uint32_t GetNextFetchBlock();

        /**
         * @return The instructions predicted as executable from the instruction block
         */
        bool* GetInstructionValidBits();

        /**
         * @brief Register a new entry in BTB
         * @param fetchAddress The fetch address used to instruction block
         * @param fetchTargets The array of targets for each instruction in the new block
         * @details This method registers a new block in the BTB, defining the tag and target addresses.
         */
        void RegisterNewBlock(uint32_t fetchAddress, uint32_t* fetchTargets);

        /**
         * @brief Make a query on BTB from an address
         * @param fetchAddress The address used to fetch block
         * @details This method sets the "nextFetchBlock" and "instructionValidBits" attributes after querying the BTB and determining which instructions are predicted to be executable and the address of the next fetch block.
         * If the entry is not yet allocated, it assumes that the next fetch block is sequential and that all instructions will be executed.
         * @return Returns a message to the procedure calling the method, indicating whether the BTB entry is allocated or not allocated, as these cases require different procedures later.
         */
        TypeBTBMessage FetchBTBEntry(uint32_t fetchAddress);

        /**
         * @brief Updates the BTB block based on the instructions that were executed
         * @param fetchAddress The address used to fetch block
         * @param executedInstructions An array of booleans indicating which instructions were actually executed
         */
        void UpdateBlock(uint32_t fetchAddress, bool* executedInstructions);

        ~BranchTargetBuffer();
};

#endif