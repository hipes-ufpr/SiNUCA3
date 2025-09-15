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
 * @brief Implementation of gshare predictor.
 * @details The gshare predictor uses a table of bimodal counters to make
 * predictions on the direction the flow of execution will take. The table is
 * indexed by hashing the instruction address and the value stored in the
 * globalBranchHistReg attribute. The latter can store information of the
 * direction taken by up to 64 instructions. When instantiating the gshare, it
 * will round the number of entries to the greatest power of 2 less than the
 * number requested, in such a way that bitwise operations can be used to
 * calculate the index.
 *
 * Note that when queried, this component stores the calculated index in a
 * queue to later update the right positions in the bimodal counter table.
 */

#include <sinuca3.hpp>
#include <utils/bimodal_counter.hpp>
#include <utils/circular_buffer.hpp>

#include "engine/default_packets.hpp"

/** @brief Refer to gshare_predictor.hpp documentation for details */
class GsharePredictor : public Component<PredictorPacket> {
  private:
    BimodalCounter* entries; /**<Bimodal counter table> */
    CircularBuffer indexQueue;
    unsigned long globalBranchHistReg;
    unsigned long numberOfEntries;          /**<Size of table> */
    unsigned long numberOfPredictions;      /**<Used for statistics> */
    unsigned long numberOfWrongPredictions; /**<Used for statistics> */
    unsigned long currentIndex;
    unsigned int indexQueueSize; /**<Queue size. Default is unlimited size> */
    unsigned int indexBitsSize;  /**<Number of bits used to address table> */
    bool wasPredictedToBeTaken;
    bool wasBranchTaken;

    Component<PredictorPacket>* sendTo;
    int sendToId;

    int Allocate();
    void Deallocate();
    /**
     * @brief Round the number of entries to the greatest power of 2 less than
     * the value in requestedSize.
     */
    int RoundNumberOfEntries(unsigned long requestedSize);
    /**
     * @brief Fill response packet with predicted direction of execution.
     */
    void PreparePacket(PredictorPacket* pkt);
    /**
     * @brief Update the predictor table and gbhr.
     */
    void Update();
    void UpdateGlobBranchHistReg();
    void UpdateEntry();
    /**
     * @brief Calculate index of access, save it and fill packet with prediction
     * from table.
     */
    void Query(PredictorPacket* pkt, unsigned long addr);
    /**
     * @note Since this predictor does not have a tag in each entry, when
     * queried, it will always output a valid answer.
     */
    void QueryEntry();
    void CalculateIndex(unsigned long addr);
    /**
     * @return 0 if successfuly, 1 otherwise.
     */
    int EnqueueIndex();
    /**
     * @return 0 if successfuly, 1 otherwise.
     */
    int DequeueIndex();

  public:
    GsharePredictor();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics();
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~GsharePredictor();
};

#ifndef NDEBUG
int TestGshare();
#endif

#endif  // SINUCA3_GSHARE_PREDICTOR