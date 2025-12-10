#ifndef SINUCA3_FETCHER_HPP_
#define SINUCA3_FETCHER_HPP_

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

/**
 * @file fetcher.hpp
 * @brief API of the Fetcher, a generic component for simulating logic in the
 * first stage of a pipeline, supporting integration with predictors and caches.
 */

#include <cstdlib>
#include <sinuca3.hpp>

/**
 * @brief Enum for flags for the fetch buffer of the fetcher.
 * @details We use constants and not enums because C++ complains about
 * usingenums as sets (bit-oring them and stuff). It's an int because alignment
 * would make anything occupy more space in the struct anyways so let's use the
 * type that enables us to operate faster on the registers.
 */
typedef int FetchBufferEntryFlags;
/** @brief Predictor responded about this instruction. */
const FetchBufferEntryFlags FetchBufferEntryFlagsPredicted = (1 << 0);
/** @brief We already sent this instruction to the predictor. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSentToPredictor = (1 << 1);
/** @brief We already sent this instruction to the memory. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSentToMemory = (1 << 2);

/** @brief Represents an instruction alongside with useful information in the
 * fetch buffer. */
struct FetchBufferEntry {
    InstructionPacket instruction;
    FetchBufferEntryFlags flags;

    inline FetchBufferEntry() : flags((FetchBufferEntryFlags)0) {}
};

/**
 * @brief The Fetcher is a generic fetcher with support to an instruction memory
 * and a predictor. It handles the pay of misspredicton penalties.
 *
 * @details It accepts the following parameters:
 * - fetch (required): Component<InstructionPacket> from which to fetch
 *   instructions.
 * - instructionMemory (required): Component<InstructionPacket> to which send
 *   the instruction after fetching.
 * - fetchSize: The size in bytes to fetch per fetch cycle. Defaults to 0.
 * - fetchInterval: Integer amount of cycles to wait before fetching. I.e.,
 *   fetch every `fetchInterval` cycles. Defaults to 1.
 * - predictor: Component<InstructionPacket> to which send prediction requests.
 * - misspredictPenalty: Integer amount of cycles to idle when a missprediction
 *   happens.
 */
class Fetcher : public Component<int> {
  private:
    Component<FetchPacket>*
        fetch; /** @brief Component from which to fetch instructions. */
    Component<InstructionPacket>*
        instructionMemory; /** @brief Component to which send the instructions
                              after fetching. */
    Component<PredictorPacket>*
        predictor; /** @brief Component to which send prediction requests. */
    FetchBufferEntry* fetchBuffer; /** @brief Fetched instructions. */
    unsigned long
        fetchBufferUsage; /** @brief Number of instructions in fetchBuffer. */
    unsigned long fetchSize;     /** @brief Amount of bytes to fetch. */
    unsigned long fetchInterval; /** @brief Cycle interval to fetch. */
    unsigned long fetchClock;    /** @brief Counter to control when to fetch. */
    unsigned long misspredictPenalty; /** @brief Amount of cycles to idle when a
                                         missprediction happens. */
    unsigned long
        misspredictions; /** @brief Number of misspredictions that happened. */
    unsigned long currentPenalty; /** @brief Counter to control the paying of
                                     penalties. */
    unsigned long
        fetchedInstructions; /** @brief Number of fetched instructions. */
    int predictorID;         /** @brief Connection ID of predictor. */
    int fetchID;             /** @brief Connection ID of fetch. */
    int instructionMemoryID; /** @brief Connection ID of instructionMemory. */
    FetchBufferEntryFlags
        flagsToCheck; /** @brief Flags to check when removing instructions from
                         the buffer. If there's a predictor, we need to check
                         wether the instruction was predicted. If there's no
                         predictor, we don't. */

    /** @brief Helper to send the fetched instructions to the memory and the
     * predictor. */
    void ClockSendBuffered();
    /** @brief Helper to check predicted instructions. */
    int ClockCheckPredictor();
    /** @brief Helper to remove instructions from the buffer. */
    void ClockUnbuffer();
    /** @brief Helper to request instructions to `fetch`. */
    void ClockRequestFetch();
    /** @brief Helper to get the fetched instructions. */
    void ClockFetch();

  public:
    inline Fetcher()
        : fetch(NULL),
          instructionMemory(NULL),
          predictor(NULL),
          fetchBuffer(NULL),
          fetchBufferUsage(0),
          fetchSize(1),
          fetchInterval(1),
          fetchClock(0),
          misspredictPenalty(0),
          misspredictions(0),
          currentPenalty(0),
          fetchedInstructions(0),
          flagsToCheck(FetchBufferEntryFlagsSentToMemory) {}
    virtual int FinishSetup();
    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~Fetcher();
};

#endif  // SINUCA3_FETCHER_HPP_
