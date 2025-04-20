#ifndef SINUCA3_BIMODAL_PREDICTOR_HPP
#define SINUCA3_BIMODAL_PREDICTOR_HPP

/**
 * @file interleavedBTB.hpp
 * @brief Implementation of Interleaved Branch Target Buffer
 * @details A bimodal predictor consists of a simple two-bit structure to
 * predict whether a branch will be taken or not. It maps predictions onto two
 * bits in this way:
 * 00 - Not Taken
 * 01 - Not Taken
 * 10 - Taken
 * 11 - Taken
 * When a branch is taken, 1 is added to the predictor, 0 otherwise.
 * The current state of the predictor is used to predict the next branch.
 * This structure is a base for the Interleavd BTB, which stores the branch
 * address, so the BimodalPredictor class only deals with predicting and
 * updating the bits.
 */

const bool NTAKEN = 0;
const bool TAKEN = 1;

class BimodalPredictor {
  private:
    unsigned char prediction; /**< The prediction bits. */

  public:
    BimodalPredictor();

    /**
     * @brief get the current prediction.
     * @return Taken if Taken if the bits are 10+, Not Taken otherwise.
     */
    bool GetPrediction();

    /**
     * @brief Updates the prediciton with the new information.
     * @param branchTaken Informs whether the branch has been taken or not.
     */
    void UpdatePrediction(bool branchTaken);
    
    ~BimodalPredictor();
};

#endif