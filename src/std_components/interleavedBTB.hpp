#ifndef SINUCA3_INTERLEAVED_BTB_HPP
#define SINUCA3_INTERLEAVED_BTB_HPP

/**
 * @file interleavedBTB.hpp
 * @brief Implementation of Interleaved Branch Target Buffer
 */

#include "../engine/component.hpp"
#include "../utils/bimodalPredictor.hpp"

namespace sinuca {

enum branchType { NONE, CONDITIONAL_BRANCH, UNCONDITIONAL_BRANCH };

enum TypeBTBMessage {
    BTB_REQUEST,
    UNALLOCATED_ENTRY,
    ALLOCATED_ENTRY,
    BTB_ALLOCATION_REQUEST,
    BTB_UPDATE_REQUEST
};

struct BTBMessage {
    int channelID;
    unsigned int fetchAddress;
    unsigned int nextBlock;
    unsigned int* fetchTargets;
    bool* validBits;
    bool* executedInstructions;
    TypeBTBMessage messageType;
};

struct btb_entry {
  private:
    unsigned int numBanks;             /**<The number of banks. */
    unsigned long entryTag;            /**<The entry tag. */
    unsigned long* targetArray;        /**<The target address array. */
    branchType* branchTypes;           /**<The branch types array. */
    BimodalPredictor* predictorsArray; /**<The array of predictors. */

  public:
    btb_entry();

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
                 unsigned long targetAddress, branchType type);

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
    long GetTag();

    /**
     * @brief Gets the branch target address.
     * @param bank The bank containing the branch.
     */
    long GetTargetAddress(unsigned int bank);

    /**
     * @brief Gets the branch type.
     * @param bank The bank containing the branch.
     */
    branchType GetBranchType(unsigned int bank);

    /**
     * @brief Gets the branch prediction.
     * @param bank The bank containing the branch.
     */
    bool GetPrediction(unsigned int bank);

    ~btb_entry();
};

class BranchTargetBuffer : public sinuca::Component<BTBMessage> {
  private:
    btb_entry** btb;
    unsigned int interleavingFactor;
    unsigned int numEntries;
    unsigned int interleavingBits;
    unsigned int entriesBits;

    unsigned int CalculateBank(unsigned long address);
    unsigned long CalculateTag(unsigned long address);
    unsigned long CalculateIndex(unsigned long address);
    int RegisterNewBranch(unsigned long address, unsigned long targetAddress,
                          branchType type);
    int UpdateBranch(unsigned long address, bool branchState);

  public:
    BranchTargetBuffer();

    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual int FinishSetup();

    virtual void Clock();
    virtual void Flush();
    ~BranchTargetBuffer();
};

}  // namespace sinuca

#endif