#ifndef SINUCA3_BIMODAL_PREDICTOR_HPP
#define SINUCA3_BIMODAL_PREDICTOR_HPP

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
 * @file bimodal_counter.hpp
 * @brief Public API of a generic bimodal predictor.
 */

class BimodalCounter {
  private:
    unsigned char prediction; /**< The prediction bits. */

  public:
    BimodalCounter();

    /**
     * @brief Get the current prediction. Prediction is taken if bits are 10+,
     * not taken otherwise.
     * @return True if prediction is taken, false otherwise.
     */
    bool GetPrediction();

    /**
     * @brief Updates the prediciton with the new information.
     * @param branchTaken Informs whether the branch has been taken or not.
     */
    void UpdatePrediction(bool wasBranchTaken);

    ~BimodalCounter();
};

#endif
