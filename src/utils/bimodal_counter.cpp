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
 * @file bimodal_counter.cpp
 * @details Implementation of a generic bimodal predictor.
 */

#include "bimodal_counter.hpp"

BimodalCounter::BimodalCounter() : prediction(2) {}

bool BimodalCounter::GetPrediction() { return (this->prediction >> 1); }

void BimodalCounter::UpdatePrediction(bool wasBranchTaken) {
    if ((wasBranchTaken == true) && (this->prediction < 3)) {
        this->prediction++;

        return;
    }

    if ((wasBranchTaken == false) && (this->prediction > 0)) {
        this->prediction--;

        return;
    }
}

BimodalCounter::~BimodalCounter() {}
