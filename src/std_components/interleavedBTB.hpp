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
    uint32_t fetchAddress;
    uint32_t nextBlock;
    uint32_t* fetchTargets;
    bool* validBits;
    bool* executedInstructions;
    TypeBTBMessage messageType;
};

struct btb_entry {
  private:
    unsigned int
        interleavingFactor; /**<The interleaving factor, the number of banks. */
    unsigned long entryTag; /**<The entry tag. */
    unsigned long* targetArray;        /**<The target address array. */
    branchType* branchTypes;           /**<The branch types array. */
    BimodalPredictor* predictorsArray; /**<The array of predictors. */

  public:
    btb_entry();

    /**
     * @brief Allocates the BTB entry.
     * @param interleavingFactor The interleaving factor defines the number of
     * branches that a BTB entry can store.
     * @return 0 if successfuly, 1 otherwise.
     */
    int Allocate(unsigned int interleavingFactor);

    /**
     * @brief Register a new entry.
     * @param tag The tag of this entry's instruction block.
     * @param bank The bank with which the branch will be registered.
     * @param targetAdress The branch's target address.
     * @param type The branch's type.
     * @return 0 if successfuly, 1 otherwise.
     */
    int NewEntry(unsigned long tag, unsigned long bank, long targetAddress,
                 branchType type);

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
    btb_entry* btb;
    /**
     * @brief Calculates the tag used to verify the BTB entry
     * @param FetchAddress Address used to access BTB
     * @details This method aligns the address with the interleaving factor and
     * returns the value
     */
    uint32_t CalculateTag(uint32_t fetchAddress);
    /**
     * @brief Calculate the index to access the correct BTB entry
     * @param fetchAddress Address used to access BTB
     * @details The method calculates an index within a fixed range by applying
     * bitwise shifts and masks. Aligning the fetch address with the
     * interleaving factor and obtaining the index of the respective BTB entry
     * for the fetch address.
     * @return The index to access BTB
     */
    uint32_t CalculateIndex(uint32_t fetchAddress);

  public:
    BranchTargetBuffer();

    ~BranchTargetBuffer();
};

}  // namespace sinuca

#endif