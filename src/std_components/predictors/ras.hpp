#ifndef SINUCA3_RAS_HPP_
#define SINUCA3_RAS_HPP_

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
 * @file ras.hpp
 * @brief API of the Ras. A simple, generic return address stack. It does not
 * care at all about wrong predictions.
 *
 * @details It does not checks wether the branch is a return on queries.
 * Clients should make this check before sending a query request. It always
 * responds with a ResponseTakeToAddress. Queries need not to fill any data, and
 * updates can have only the target address.
 */

#include "../../sinuca3.hpp"

class Ras : public sinuca::Component<sinuca::PredictorPacket> {
  private:
    sinuca::Component<sinuca::PredictorPacket>* sendTo;
    unsigned long* buffer;
    long size;
    long end;
    unsigned long numQueries;
    unsigned long numUpdates;

    int forwardToID;

    inline void RequestQuery(const sinuca::StaticInstructionInfo* instruction,
                             int connectionID);

    inline void RequestUpdate(unsigned long targetAddress);

  public:
    inline Ras() : sendTo(NULL), buffer(NULL), size(0), end(0) {}
    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

    virtual ~Ras();
};

#ifndef NDEBUG
int TestRas();
#endif

#endif  // SINUCA3_RAS_HPP_
