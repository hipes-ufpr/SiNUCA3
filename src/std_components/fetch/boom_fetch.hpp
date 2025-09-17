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

#include "config/config.hpp"
#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include "std_components/predictors/interleavedBTB.hpp"
#include "std_components/predictors/ras.hpp"

/**
 * @brief Enum for flags for the fetch buffer of the fetcher.
 * @details We use constants and not enums because C++ complains about
 * usingenums as sets (bit-oring them and stuff). It's an int because alignment
 * would make anything occupy more space in the struct anyways so let's use the
 * type that enables us to operate faster on the registers.
 */
typedef int FetchBufferEntryFlags;

/** @brief This instruction was send to memory. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSendMemory = (1 << 0);
/** @brief This instruction was send to predictor. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSendPredictor = (1 << 1);
/** @brief This instruction was send to ras. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSendRas = (1 << 2);
/** @brief This instruction was processed by fetch. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSendInst = (1 << 3);
/** @brief This instruction was send to btb. */
const FetchBufferEntryFlags FetchBufferEntryFlagsSendBTB = (1 << 4);
struct FetchBufferEntry {
    InstructionPacket instruction; /**< Fetched instruction >*/
    FetchBufferEntryFlags flags;   /**< Flags for the entry >*/

    inline FetchBufferEntry() : flags((FetchBufferEntryFlags)0) {}
};

/**
 * @brief Boom Fetch is the implementation of the fetch stage of Boom,
 * containing its directly connected components.
 *
 * @details It accepts the following parameters:
 * - fetch (required): Component<InstructionPacket> from which to fetch
 *   instructions.
 * - instructionMemory (required): Component<InstructionPacket> to which send
 *   the instruction after fetching.
 * - btb (required): Interleaved BTB reference.
 * - ras (required) : Return Address Stack reference.
 * - fetchSize: The size in bytes to fetch per fetch cycle. Defaults to 0.
 * - fetchInterval: Integer amount of cycles to wait before fetching. I.e.,
 *   fetch every `fetchInterval` cycles. Defaults to 1.
 * - misspredictPenalty: Integer amount of cycles to idle when a missprediction
 *   happens.
 */
class BoomFetch : public Component<FetchPacket> {
  private:
    Component<FetchPacket>*
        fetch; /**< Component from which to fetch instructions >*/
    Component<InstructionPacket>*
        instructionMemory; /**< Component for instruction memory >*/
    BranchTargetBuffer*
        btb;  /**< Branch Target Buffer for storing branch targets >*/
    Ras* ras; /**< Return Address Stack for storing return addresses >*/
    Component<PredictorPacket>* predictor;
    FetchBufferEntry*
        fetchBuffer; /**< Fetch Buffer for storing fetched instructions >*/

    unsigned long
        fetchBufferUsage;        /**< Number of instructions in fetchBuffer >*/
    unsigned long fetchSize;     /**< Amount of bytes to fetch >*/
    unsigned long fetchInterval; /**< Cycle interval to fetch >*/
    unsigned long fetchClock;    /**< Counter to control when to fetch >*/
    unsigned long misspredictPenalty; /**< Amount of cycles to idle when a
                                         missprediction happens >*/

    unsigned long
        misspredictions; /**< Number of misspredictions that happened >*/
    unsigned long
        currentPenalty; /**< Counter to control the paying of penalties >*/

    unsigned long fetchedInstructions; /**< Number of fetched instructions >*/

    int fetchID;             /**< ID of the fetch component >*/
    int instructionMemoryID; /**< ID of the instruction memory component >*/
    int predictorID;         /**< ID of the predictor component */
    int btbID;               /**< ID of the BTB component */
    int rasID;               /**< ID of the RAS component */
    FetchBufferEntryFlags
        flagsToCheck; /**<Flags to check when removing entries from the buffer.
                         If there's a predictor, we need to check wether the
                         instruction was predicted. If there's no predictor, we
                         don't need to check anything >*/

    /** @brief Helper to set the fetch config parameter. */
    int FetchConfigParameter(ConfigValue value);
    /** @brief Helper to set the instruction memory config parameter. */
    int InstructionMemoryConfigParameter(ConfigValue value);
    /** @brief Helper to set the Predictor config parameter. */
    int PredictorConfigParameter(ConfigValue value);
    /** @brief Helper to set the fetch size config parameter. */
    int FetchSizeConfigParameter(ConfigValue value);
    /** @brief Helper to set the fetch interval config parameter. */
    int FetchIntervalConfigParameter(ConfigValue value);
    /** @brief Helper to set the missprediction penalty config parameter. */
    int MisspredictPenaltyConfigParameter(ConfigValue value);

    /** @brief Helper to send the fetched instructions to the memory, predictor
     * and btb. */
    void ClockSendBuffered();
    /** @brief Helper to check predicted instructions. */
    int ClockCheckPredictor();
    /** @brief Helper to check ras responses. */
    int ClockCheckRas();
    /** @brief Helper to check predicted instructions via BTB */
    int ClockCheckBTB();
    /** @brief Helper to remove instructions from the buffer. */
    void ClockUnbuffer();
    /** @brief Helper to request instructions to 'fetch'. */
    void ClockRequestFetch();
    /** @brief Helper to get the fetched instructions. */
    void ClockFetch();

  public:
    inline BoomFetch()
        : fetch(NULL),
          instructionMemory(NULL),
          btb(NULL),
          ras(NULL),
          fetchBuffer(NULL),
          fetchBufferUsage(0),
          fetchSize(1),
          fetchInterval(1),
          fetchClock(0),
          misspredictPenalty(0),
          misspredictions(0),
          currentPenalty(0),
          fetchedInstructions(0),
          fetchID(-1),
          instructionMemoryID(-1),
          predictorID(-1),
          btbID(-1),
          rasID(-1),
          flagsToCheck(FetchBufferEntryFlagsSendInst) {}

    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual int FinishSetup();
    virtual void Clock();
    virtual void PrintStatistics();
    virtual ~BoomFetch();
};

#endif  // SINUCA3_BOOM_FETCH_HPP_