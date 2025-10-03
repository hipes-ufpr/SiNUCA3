#ifndef SINUCA3_TRACE_DUMPER_COMPONENT
#define SINUCA3_TRACE_DUMPER_COMPONENT

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
 * @file trace_dumper_component.hpp
 * @brief A component that prints information of every instruction it receives.
 */

#include <sinuca3.hpp>
#include <vector>

class TraceDumperComponent : public Component<int> {
  private:
    std::vector<const char*> overrides;
    Component<FetchPacket>* fetch;
    unsigned long fetched;
    unsigned int fetchID;
    bool def;

    int FetchParameter(ConfigValue value);
    int DefaultParameter(ConfigValue value);
    void Override(const char* instruction);
    bool IsOverride(const char* instruction);

  public:
    inline TraceDumperComponent()
        : fetch(NULL), fetched(0), fetchID(0), def(true) {};

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void Clock();
    virtual void PrintStatistics();

    virtual ~TraceDumperComponent();
};

#endif  // SINUCA3_TRACE_DUMPER_COMPONENT
