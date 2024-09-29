//
// Configuration for your build of the simulator.
//

//
// TODO: Create the JSON configuration API.
//

#include "config.hpp"

#include "components/simple_memory.hpp"
#include "sinuca3.hpp"

MemoryComponent* config::cache = new SimpleMemory();
