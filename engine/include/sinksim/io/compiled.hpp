// Loading compiled ships and simulations (docs/FORMATS.md). Every numeric array is re-hashed after parsing
// and compared with the hash the compiler wrote; a mismatch is an error, because it means a value did not
// survive the round trip and the engine would be simulating a different ship.
#pragma once

#include <memory>
#include <string>

#include "sinksim/model.hpp"

namespace sinksim::io {

std::shared_ptr<CompiledShip> load_ship(const std::string& path);
CompiledSim load_sim(const std::string& path);

// Throws if the simulation was compiled against a different ship.
void check_pairing(const CompiledShip& ship, const CompiledSim& sim);

}  // namespace sinksim::io
