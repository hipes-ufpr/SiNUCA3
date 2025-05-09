#ifndef SINUCA3_INTERLEAVED_BTB_HPP
#define SINUCA3_INTERLEAVED_BTB_HPP

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
 * @file interleavedBTB.hpp
 * @brief Implementation of Interleaved Branch Target Buffer.
 * @details The Interleaved BTB is a structure that stores branches and has a
 * certain interleaving factor that allows multiple queries in a single cycle.
 * In this implementation, it is mapped directly and each entry has a tag for
 * recognizing the block of instructions and vectors assimilated to the block,
 * where each element of the vector corresponds to information from one of the
 * instructions in the block. Although the parameters are configurable, for
 * optimization reasons, the BTB is limited to using parameters that are
 * multiples of 2, allowing bitwise operations to optimize the BTB's overall
 * running time. When defining the parameters, the BTB itself internally defines
 * the parameters as multiples of 2, choosing the log2 floor of the parameter
 * passed in and using it to self-generate.
 * To avoid dynamic allocations in messages and a possible memory overload, the
 * BTB has a maximum interleaving factor set allowing a static vector to be used
 * in response messages. If you want a higher interleaving factor than the one
 * defined, simply change the value of the MAX_INTERLEAVING_FACTOR constant,
 * adjust the parameters in the configuration YAML and recompile with “make -B”.
 */

#include "../engine/component.hpp"
#include "../utils/bimodalPredictor.hpp"

const int MAX_INTERLEAVING_FACTOR = 16;

enum BranchType {
    BranchTypeNone,
    BranchTypeConditionalBranch,
    BranchTypeUnconditionalBranch
};

enum BTBPacketType {
    RequestQuery,
    RequestAddEntry,
    RequestUpdate,
    ResponseBTBHit,
    ResponseBTBMiss
};

struct BTBPacket {
    union {
        struct {
            unsigned long address; /**<The fetch address as BTB input. */
        } requestQuery;

        struct {
            unsigned long address; /**<The fetch address as BTB input. */
            bool branchState; /**<The result of the branch, whether it was taken
                                 or not. */
        } updateQuery;

        struct {
            unsigned long address;       /**<The fetch address as BTB input. */
            unsigned long targetAddress; /**<The branch's jump address. */
            BranchType typeOfBranch;     /**<The type of branch (Conditional /
                                            Unconditional). */
        } requestAddEntry;

        struct {
            unsigned int numberOfBits;   /**<Size of valid bits array. */
            unsigned long address;       /**<The fetch address as BTB */
            unsigned long targetAddress; /**<The target address for the next
                                            fetch block. */
            bool validBits[MAX_INTERLEAVING_FACTOR]; /**<The vector of valid
                                                        bits indicates which
                                                        instructions in the
                                                        block are expected to be
                                                        executed. */
        } responseQuery;

    } data;
    BTBPacketType type;
};

struct BTBEntry {
    unsigned int numBanks;             /**<The number of banks. */
    unsigned long entryTag;            /**<The entry tag. */
    unsigned long* targetArray;        /**<The target address array. */
    BranchType* branchTypes;           /**<The branch types array. */
    BimodalPredictor* predictorsArray; /**<The array of predictors. */

  public:
    BTBEntry();

    /**
     * @brief Allocates the BTB entry.
     * @param numBanks The number of banks in an entry.
     * @return 0 if successfuly, 1 otherwise.
     */
    int Allocate(unsigned int numBanks);

    /**
     * @brief Register a new entry.
     * @param tag The tag of this entry's instruction block.
     * @param bank The bank with which the branch will be registered.
     * @param targetAdress The branch's target address.
     * @param type The branch's type.
     * @return 0 if successfuly, 1 otherwise.
     */
    int NewEntry(unsigned long tag, unsigned int bank,
                 unsigned long targetAddress, BranchType type);

    /**
     * @brief Update the BTB entry.
     * @param bank The bank containing the branch to be updated.
     * @param branchState The state of the branch, whether it has been taken or
     * not.
     * @return 0 if successfuly, 1 otherwise.
     */
    int UpdateEntry(unsigned long bank, bool branchState);

    /**
     * @brief Gets the tag of the entry
     */
    inline unsigned long GetTag() { return this->entryTag; };

    /**
     * @brief Gets the branch target address.
     * @param bank The bank containing the branch.
     */
    inline unsigned long GetTargetAddress(unsigned int bank) {
        if (bank < this->numBanks) return this->targetArray[bank];

        return 0;
    };

    /**
     * @brief Gets the branch type.
     * @param bank The bank containing the branch.
     */
    inline BranchType GetBranchType(unsigned int bank) {
        if (bank < this->numBanks) return this->branchTypes[bank];

        return BranchTypeNone;
    };

    /**
     * @brief Gets the branch prediction.
     * @param bank The bank containing the branch.
     */
    inline bool GetPrediction(unsigned int bank) {
        if (bank < this->numBanks) {
            return this->predictorsArray[bank].GetPrediction();
        }

        return false;
    };

    ~BTBEntry();
};

class BranchTargetBuffer : public sinuca::Component<struct BTBPacket> {
  private:
    BTBEntry** btb; /**<The pointer to BTB struct. */
    unsigned int
        interleavingFactor; /**<The interleaving factor, defining the number of
                              banks in which the BTB is interleaved. */

    unsigned int numEntries;       /**<The number of BTB entries. */
    unsigned int interleavingBits; /**< InterleavingFactor in bits. */
    unsigned int entriesBits;      /**<Number of entries in bits. */

    /**
     * @brief Calculate the bank based on address.
     * @param address The address that will be used to calculate the bank.
     * @return Which bank the instruction is in.
     */
    unsigned int CalculateBank(unsigned long address);

    /**
     * @brief Calculate the tag based on address.
     * @param address The address that will be used to calculate the tag.
     * @return The tag of instruction block.
     */
    unsigned long CalculateTag(unsigned long address);

    /**
     * @brief Calculate the index based on address.
     * @param address The address that will be used to calculate the index.
     * @return The index of branch, In which BTB entry is the branch located.
     */
    unsigned long CalculateIndex(unsigned long address);

    /**
     * @brief Register a new branch in BTB.
     * @param address The branch address.
     * @param targetAddress The target of the branch.
     * @param type The type of branch.
     * @return 0 if successfuly, 1 otherwise.
     */
    int RegisterNewBranch(unsigned long address, unsigned long targetAddress,
                          BranchType type);

    /**
     * @brief Update a BTB entry.
     * @param address The branch address.
     * @param branchState The Information on whether the branch has been taken.
     * or not.
     * @return 0 if successfuly, 1 otherwise.
     */
    int UpdateBranch(unsigned long address, bool branchState);

    /**
     * @brief Method for Request Query.
     * @param address The fetch address received by a request message.
     * @param connectionID The ID of the connection that received the request.
     * @details When it receives a request, the BTB checks that the address
     * received is valid (if there is an entry in the BTB for it). Then, if it
     * is valid, it checks the corresponding entry and sends a reply message
     * containing the address of the next fetch block and a limited vector
     * containing the prediction bits of the instructions in the current block.
     * If it is invalid, it sends a reply message containing the address of the
     * next sequential block (current address + interleaving factor) and a
     * limited vector with all bits set to 1, assuming that all instructions in
     * the block are predicted to execute.
     */
    inline void Query(unsigned long address, int connectionID);

    /**
     * @brief Method for RequestAddEntry.
     * @param address The address of a branch instruction.
     * @param targetAddress The target address of the branch instruction.
     * @param type The type of branch instruction.
     * @details A wrapper for the method of registering a new entry in the BTB.
     */
    inline int AddEntry(unsigned long address, unsigned long targetAddress,
                        BranchType type);

    /**
     * @brief Method for RequestUpdate.
     * @param address The address of a branch instruction.
     * @param branchState The Information on whether the branch has been taken.
     * @details A wrapper for the method of updating an entry in the BTB.
     */
    inline int Update(unsigned long address, bool branchState);

  public:
    BranchTargetBuffer();

    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);

    virtual int FinishSetup();

    virtual void Clock();

    virtual void Flush();

    ~BranchTargetBuffer();
};

#endif