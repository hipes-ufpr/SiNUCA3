#ifndef SINUCA3_BOOM_FETCH_HPP_
#define SINUCA3_BOOM_FETCH_HPP_

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
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

#include <sinuca3.hpp>
#include "engine/default_packets.hpp"
#include "std_components/fetch/fetcher.hpp"
#include "std_components/predictors/interleavedBTB.hpp"
#include "std_components/predictors/ras.hpp"

class BoomFetch : public Component<FetchPacket> {
    private:
        Component<FetchPacket>* fetch; /**< Component from which to fetch instructions >*/
        Component<InstructionPacket>* instructionMemory; /**< Component for instruction memory >*/
        FetchBufferEntry* fetchBuffer; /**< Fetch Buffer for storing fetched instructions >*/
        BranchTargetBuffer* btb; /**< Branch Target Buffer for storing branch targets >*/
        Ras* ras; /**< Return Address Stack for storing return addresses >*/

        unsigned long fetchBufferUsage; /**< Number of instructions in fetchBuffer >*/
        unsigned long fetchSize; /**< Amount of bytes to fetch >*/
        unsigned long fetchInterval; /**< Cycle interval to fetch >*/
        unsigned long fetchClock; /**< Counter to control when to fetch >*/
        unsigned long misspredictPenalty; /**< Amount of cycles to idle when a missprediction happens >*/

        unsigned long misspredictions; /**< Number of misspredictions that happened >*/
        unsigned long currentPenalty; /**< Counter to control the paying of penalties >*/

        unsigned long fetchedInstructions; /**< Number of fetched instructions >*/

        int fetchID; /**< ID of the fetch component >*/
        int instructionMemoryID; /**< ID of the instruction memory component >*/
        FetchBufferEntryFlags flagsToCheck; /**<Flags to check when removing entries from the buffer. If there's a predictor, we need to check wether the instruction was predicted. If there's no predictor, we don't >*/
};

#endif  // SINUCA3_BOOM_FETCH_HPP_