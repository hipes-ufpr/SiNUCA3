#ifndef SINUCA3_GSHARE_PREDICTOR_HPP_
#define SINUCA3_GSHARE_PREDICTOR_HPP_

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
 * @file gshare_predictor.hpp
 * @brief
 * @details
 */

#include <sinuca3.hpp>
#include <utils/bimodal_counter.hpp>
#include <utils/circular_buffer.hpp>

/** @brief Refer to gshare_predictor.hpp documentation for details */
class GsharePredictor : public Component<PredictorPacket> {
  private:
    BimodalCounter* entries;           /**< */
    CircularBuffer indexQueue;         /**< */
    unsigned long globalBranchHistReg; /**< */
    unsigned long numberOfEntries;
    unsigned long numberOfPredictions;
    unsigned long numberOfWrongPredictions;
    unsigned long currentIndex;  /**< */
    unsigned int indexQueueSize; /**<Queue size. Default value is 0> */
    unsigned int indexBitsSize;  /**< */
    bool directionPredicted;
    bool directionTaken;

    Component<PredictorPacket>* sendTo;
    int sendToId;

    int Allocate();
    void Deallocate();
    /**
     * @brief
     */
    int RoundNumberOfEntries(unsigned long requestedSize);
    /**
     * @brief
     */
    void PreparePacket(PredictorPacket* pkt);
    /**
     * @brief
     */
    void ReadPacket(PredictorPacket* pkt);
    int EnqueueIndex();
    int DequeueIndex();
    /**
     * @brief
     */
    void CalculateIndex(unsigned long addr);
    void UpdateEntry();
    void UpdateGlobBranchHistReg();
    /**
     * @brief
     */
    void QueryEntry();

  public:
    GsharePredictor();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics();
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~GsharePredictor();
};

#endif  // SINUCA3_GSHARE_PREDICTOR