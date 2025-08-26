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
 * @file bimodal_predictor.hpp
 * @brief Public API of a generic bimodal predictor.
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
