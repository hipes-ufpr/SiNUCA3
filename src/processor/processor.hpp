#ifndef SINUCA3_PROCESSOR_HPP_
#define SINUCA3_PROCESSOR_HPP_

//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
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
 * @file processor.hpp
 * @brief Public API of the Processor, which use methods for the frontend and
 * attributes for the backend.
 */

#include "../sinuca3.hpp"

class Processor : public sinuca::MemoryRequester {
  private:
    sinuca::MemoryComponent* cache;

  public:
    Processor();
    void Process();
    void Response(sinuca::MemoryPacket packet);
};

#endif  // SINUCA3_PROCESSOR_HPP_
