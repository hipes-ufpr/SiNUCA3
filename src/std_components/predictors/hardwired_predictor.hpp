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

#include "../../sinuca3.hpp"

class HardwiredPredictor : public sinuca::Component<sinuca::PredictorPacket> {
  private:
    sinuca::Component<sinuca::PredictorPacket>* sendTo;
    unsigned long numberOfSyscalls;
    unsigned long numberOfCalls;
    unsigned long numberOfRets;
    unsigned long numberOfUnconds;
    unsigned long numberOfConds;
    unsigned long numberOfNoBranchs;
    int sendToID;
    bool syscall;
    bool call;
    bool ret;
    bool uncond;
    bool cond;
    bool noBranch;

    int SetBoolParameter(const char* parameter, bool* ptr,
                         sinuca::config::ConfigValue value);
    void Respond(int id, sinuca::PredictorPacket request);

  public:
    inline HardwiredPredictor()
        : sendTo(NULL),
          numberOfSyscalls(0),
          numberOfCalls(0),
          numberOfRets(0),
          numberOfUnconds(0),
          numberOfConds(0),
          numberOfNoBranchs(0),
          syscall(true),
          call(true),
          ret(true),
          uncond(true),
          cond(true),
          noBranch(true) {}

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

    virtual ~HardwiredPredictor();
};

#endif  // SINUCA3_HARDWIRED_PREDICTOR_HPP_
