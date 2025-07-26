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

#include "../../sinuca3.hpp"

// We use constants and not enums because C++ complains about using enums as
// sets (bit-oring them and stuff). It's an int because alignment would make
// anything occupy more space in the struct anyways so let's use the type that
// enables us to operate faster on the registers.
typedef int FetchBufferEntryFlags;
const FetchBufferEntryFlags FetchBufferEntryFlagsPredicted = (1 << 0);
const FetchBufferEntryFlags FetchBufferEntryFlagsSentToPredictor = (1 << 1);
const FetchBufferEntryFlags FetchBufferEntryFlagsSentToMemory = (1 << 2);

struct FetchBufferEntry {
    sinuca::InstructionPacket instruction;
    unsigned long prediction;
    FetchBufferEntryFlags flags;

    inline FetchBufferEntry() : flags((FetchBufferEntryFlags)0) {}
};

class Fetcher : public sinuca::Component<int> {
  private:
    sinuca::Component<sinuca::FetchPacket>* fetch;
    sinuca::Component<sinuca::InstructionPacket>* instructionMemory;
    sinuca::Component<sinuca::PredictorPacket>* predictor;
    int predictorID;
    FetchBufferEntry* fetchBuffer;
    unsigned long fetchBufferUsage;
    unsigned long fetchSize;
    unsigned long fetchInterval;
    unsigned long fetchClock;
    unsigned long numberOfPredictors;
    unsigned long misspredictPenalty;
    unsigned long misspredictions;
    unsigned long currentPenalty;
    long lastPrediction;
    int fetchID;
    int instructionMemoryID;

    int FetchConfigParameter(sinuca::config::ConfigValue value);
    int InstructionMemoryConfigParameter(sinuca::config::ConfigValue value);
    int FetchSizeConfigParameter(sinuca::config::ConfigValue value);
    int FetchIntervalConfigParameter(sinuca::config::ConfigValue value);
    int PredictorConfigParameter(sinuca::config::ConfigValue value);
    int MisspredictPenaltyConfigParameter(sinuca::config::ConfigValue value);

    void ClockSendBuffered();
    void ClockCheckPredictor();
    void ClockUnbuffer();
    void ClockRequestFetch();
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
          numberOfPredictors(0),
          misspredictPenalty(0),
          misspredictions(0),
          currentPenalty(0),
          lastPrediction(0) {}
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

    virtual ~Fetcher();
};

#endif  // SINUCA3_FETCHER_HPP_
