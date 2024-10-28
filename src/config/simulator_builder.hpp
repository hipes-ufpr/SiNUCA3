#ifndef SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
#define SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_

#include <yaml.h>

#include "../engine/engine.hpp"

namespace sinuca {
namespace config {

class SimulatorBuilder {
  public:
    SimulatorBuilder();
    ~SimulatorBuilder();
    sinuca::engine::Engine* InstantiateSimulationEngine(
        const std::string* configFile);
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
