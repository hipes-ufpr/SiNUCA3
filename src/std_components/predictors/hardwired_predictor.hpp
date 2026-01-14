#ifndef SINUCA3_HARDWIRED_PREDICTOR_HPP_
#define SINUCA3_HARDWIRED_PREDICTOR_HPP_

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
 * @file hardwired_predictor.hpp
 * @brief API of the HardwiredPredictor, a component that always predicts or
 * misses a specific set of instructions.
 */

#include <sinuca3.hpp>

/**
 * @brief The HardwiredPredictor is a predictor that always predicted correctly
 * or misspredicts a given set of instructions. It accepts the following boolean
 * parameters, each one if false makes the predictor always misspredicts the
 * given instruction set: `syscall`, `call`, `return`, `uncond`, `cond` and
 * `noBranch`. The default for every set is true. Additionally, it accepts a
 * `sendTo` Component<PredictorPacket> parameter. If `sendTo` is set, the
 * predictor sends all responses to that component instead of sending in the
 * response channel.
 */
class HardwiredPredictor : public Component<PredictorPacket> {
  private:
    Component<PredictorPacket>*
        sendTo; /** @brief If set, sends responses to this component. */
    unsigned long numberOfSyscalls; /** @brief Number of syscalls executed. */
    unsigned long numberOfCalls;    /** @brief Number of calls executed. */
    unsigned long numberOfRets;     /** @brief Number of returns executed. */
    unsigned long numberOfSysrets; 
    unsigned long
        numberOfUnconds; /** @brief Number of unconditional branchs executed. */
    unsigned long
        numberOfConds; /** @brief Number of conditional branchs executed. */
    unsigned long
        numberOfNoBranchs; /** @brief Number of normal instructions executed. */
    int sendToID;          /** @brief ID of `sendTo`. */
    bool syscall;          /** @brief Wether to predict syscalls correctly.  */
    bool call;             /** @brief Wether to predict calls correctly.  */
    bool ret;              /** @brief Wether to predict returns correctly.  */
    bool sysret;
    bool uncond; /** @brief Wether to predict unconditional branches correctly.
                  */
    bool cond; /** @brief Wether to predict conditional branches correctly.  */
    bool
        noBranch; /** @brief Wether to predict normal instructions correctly. */

    /**
     * @brief Helper to respond a request.
     * @param id The connection id of the client.
     * @param request The request itself.
     */
    void Respond(int id, PredictorPacket request);

  public:
    inline HardwiredPredictor()
        : sendTo(NULL),
          numberOfSyscalls(0),
          numberOfCalls(0),
          numberOfRets(0),
          numberOfSysrets(0),
          numberOfUnconds(0),
          numberOfConds(0),
          numberOfNoBranchs(0),
          syscall(true),
          call(true),
          ret(true),
          sysret(true),
          uncond(true),
          cond(true),
          noBranch(true) {}

    virtual int Configure(Config config);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~HardwiredPredictor();
};

#endif  // SINUCA3_HARDWIRED_PREDICTOR_HPP_
