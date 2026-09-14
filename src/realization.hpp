#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace synth::realization {

namespace fs = std::filesystem;
using json = nlohmann::json;

struct Roots {
  fs::path data;
  fs::path state;
  fs::path runtime;
  fs::path resources;
};

class MutationLock {
 public:
  explicit MutationLock(const Roots& roots);
  ~MutationLock();

  MutationLock(const MutationLock&) = delete;
  MutationLock& operator=(const MutationLock&) = delete;

 private:
  int descriptor_{-1};
};

bool handles(const std::vector<std::string>& args);
int dispatch(const std::vector<std::string>& args, bool as_json, const Roots& roots);
json active_surfaces(const Roots& roots);
json observed_relations(const Roots& roots);
json ecosystem_participants(const Roots& roots);

} // namespace synth::realization
