#ifndef SINUCA3_ENGINE_HPP_
#define SINUCA3_ENGINE_HPP_

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
 * @file engine.hpp
 * @brief Public API of the simulation engine.
 */

#include <ctime>

#include "../trace_reader/trace_reader.hpp"
#include "component.hpp"
#include "default_packets.hpp"
#include "linkable.hpp"

namespace sinuca {
namespace engine {

/**
 * @brief The engine itself.
 * @details A component may fetch an instruction by sending a message to the
 * engine. In the configuration file, it's accessible via the pre-defined alias
 * *ENGINE. Each connection to the engine represents a core.
 */
class Engine : public Component<FetchPacket> {
  private:
    Linkable**
        components; /** @brief The components of the simulation INCLUDING
                       THE ENGINE ITSELF, guaranteed to be the first element. */
    traceReader::TraceReader* traceReader; /** @brief The trace reader. */
    sinuca::InstructionPacket*
        fetchBuffers;        /** @brief Fetch buffers for each connection. */
    long numberOfComponents; /** @brief The number of components. */
    long numberOfFetchers; /** @brief The number of components connected to the
                              engine. I.e., cores. */
    unsigned long totalCycles; /** @brief Counter of cycles. */
    unsigned long
        fetchedInstructions; /** @brief Counter of instructions fetched. */

    /**
     * @brief This variable tells wether a flush should occur in the beggining
     * of the next clock.
     */
    bool flush;
    /**
     * @brief Will be one when there's no more instructions in the trace file.
     */
    bool end;
    /** @brief Will be set if the traceReader returns an error. */
    bool error;

    /**
     * @brief Prints the estimated remaining simulation time.
     */
    void PrintTime(time_t start, unsigned long cycle);

    /** @brief Called at the beggining of Simulate(). */
    int SetupSimulation(sinuca::traceReader::TraceReader* traceReader);

    /** @brief Auxiliar to Fetch(). */
    int SendBufferedAndFetch(int id);

    /** @brief Responds to requests. */
    void Fetch(int id, sinuca::FetchPacket packet);

  public:
    inline Engine()
        : components(NULL),
          fetchBuffers(NULL),
          numberOfComponents(0),
          numberOfFetchers(0),
          totalCycles(0),
          fetchedInstructions(0),
          flush(false),
          end(false),
          error(false) {}

    /** @brief Instantiates a simulation from the array of components. */
    inline void Instantiate(Linkable** components, long numberOfComponents) {
        this->components = components;
        this->numberOfComponents = numberOfComponents;
    }

    /**
     * @brief Self-explanatory.
     * @returns Non-zero if the simulation stopped because of a problem. 0 if it
     * stopped normally.
     */
    int Simulate(traceReader::TraceReader* traceReader);

    virtual int FinishSetup();
    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual void Clock();
    virtual void Flush();
    virtual void PrintStatistics();

    virtual ~Engine();
};

}  // namespace engine
}  // namespace sinuca

#endif  // SINUCA3_ENGINE_HPP_
