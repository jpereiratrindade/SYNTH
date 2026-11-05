#pragma once

#include <string>
#include <vector>

#include "realization.hpp"

namespace synth::remote {

using realization::Roots;
using realization::json;

bool handles(const std::vector<std::string>& args);
int dispatch(const std::vector<std::string>& args, bool as_json, const Roots& roots);
json participants(const Roots& roots);
json available_surfaces(const Roots& roots);

} // namespace synth::remote
