#include "remote.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unistd.h>

#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>

namespace synth::remote {
namespace fs = std::filesystem;
namespace {

std::string timestamp() {
  const auto value = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
  gmtime_r(&value, &utc);
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

void ensure_directory(const fs::path& path) {
  if (fs::exists(path)) {
    if (!fs::is_directory(path)) throw std::runtime_error("expected directory: " + path.string());
    return;
  }
  if (!fs::create_directories(path)) throw std::runtime_error("cannot create directory: " + path.string());
}

json read_json(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot read JSON: " + path.string());
  return json::parse(input);
}

void write_json_atomic(const fs::path& path, const json& value) {
  ensure_directory(path.parent_path());
  const auto temporary = path.string() + ".tmp-" + std::to_string(::getpid());
  { std::ofstream output(temporary); output << value.dump(2) << '\n'; }
  fs::rename(temporary, path);
}

void validate(const fs::path& schema_path, const json& instance, std::string_view name) {
  std::ifstream input(schema_path);
  if (!input) throw std::runtime_error("cannot read schema: " + schema_path.string());
  auto schema = jsoncons::json::parse(input);
  const auto options = jsoncons::jsonschema::evaluation_options{}.require_format_validation(true);
  const auto compiled = jsoncons::jsonschema::make_json_schema(std::move(schema), options);
  std::optional<std::string> error;
  compiled.validate(jsoncons::json::parse(instance.dump()), [&error](const auto& message) {
    error = message.instance_location().string() + ": " + message.message();
    return jsoncons::jsonschema::walk_state::abort;
  });
  if (error) throw std::runtime_error(std::string(name) + " invalid: " + *error);
}

fs::path registrations(const Roots& roots) { return roots.state / "remote" / "registrations"; }
fs::path observations(const Roots& roots) { return roots.state / "remote" / "observations"; }

bool safe_identity(std::string_view value) {
  if (value.empty() || value.front() < 'a' || value.front() > 'z') return false;
  return std::all_of(value.begin(), value.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
  });
}

int register_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 3) throw std::runtime_error("usage: synth remote register <manifest.json>");
  auto manifest = read_json(args[2]);
  validate(roots.resources / "schemas/remote-participant.schema.json", manifest, "remote participant manifest");
  const auto identity = manifest.at("identity").get<std::string>();
  if (!safe_identity(identity)) throw std::runtime_error("invalid remote identity");
  const auto target = registrations(roots) / (identity + ".json");
  manifest["registered_at"] = timestamp();
  write_json_atomic(target, manifest);
  if (as_json) std::cout << json{{"status", "REGISTERED"}, {"identity", identity}, {"record", target.string()}}.dump() << '\n';
  else std::cout << identity << " remote participant registered.\n";
  return 0;
}

int attest_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 3) throw std::runtime_error("usage: synth remote attest <observation.json>");
  const auto observation = read_json(args[2]);
  validate(roots.resources / "schemas/remote-observation.schema.json", observation, "remote observation");
  const auto identity = observation.at("identity").get<std::string>();
  const auto registration_path = registrations(roots) / (identity + ".json");
  if (!fs::is_regular_file(registration_path)) throw std::runtime_error("remote identity is not registered: " + identity);
  const auto registration = read_json(registration_path);
  if (observation.at("endpoint") != registration.at("endpoint") ||
      observation.at("identity_assurance") != registration.at("identity_assurance")) {
    throw std::runtime_error("observation does not match remote registration");
  }
  const auto target = observations(roots) / (identity + ".json");
  write_json_atomic(target, observation);
  if (as_json) std::cout << json{{"status", observation.at("availability")}, {"identity", identity}, {"record", target.string()}}.dump() << '\n';
  else std::cout << identity << " remote observation: " << observation.at("availability").get<std::string>() << "\n";
  return 0;
}

int remove_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 3) throw std::runtime_error("usage: synth remote remove <identity>");
  if (!safe_identity(args[2])) throw std::runtime_error("invalid remote identity");
  fs::remove(observations(roots) / (args[2] + ".json"));
  const bool removed = fs::remove(registrations(roots) / (args[2] + ".json"));
  if (as_json) std::cout << json{{"status", removed ? "REMOVED" : "NO_OP"}, {"identity", args[2]}}.dump() << '\n';
  else std::cout << args[2] << (removed ? " removed.\n" : " was not registered.\n");
  return 0;
}

std::optional<json> valid_observation(const Roots& roots, const json& registration) {
  const auto path = observations(roots) / (registration.at("identity").get<std::string>() + ".json");
  if (!fs::is_regular_file(path)) return std::nullopt;
  try {
    const auto observation = read_json(path);
    validate(roots.resources / "schemas/remote-observation.schema.json", observation, "remote observation");
    if (observation.at("endpoint") != registration.at("endpoint") ||
        observation.at("identity_assurance") != registration.at("identity_assurance") ||
        observation.value("expires_at", "") < timestamp()) return std::nullopt;
    return std::optional<json>{observation};
  } catch (...) { return std::nullopt; }
}

json registrations_list(const Roots& roots) {
  json values = json::array();
  if (!fs::is_directory(registrations(roots))) return values;
  for (const auto& entry : fs::directory_iterator(registrations(roots))) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
    try {
      auto value = read_json(entry.path());
      validate(roots.resources / "schemas/remote-participant.schema.json", value, "remote participant manifest");
      values.push_back(std::move(value));
    } catch (...) {}
  }
  std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.value("identity", "") < b.value("identity", ""); });
  return values;
}

} // namespace

bool handles(const std::vector<std::string>& args) { return !args.empty() && args[0] == "remote"; }

int dispatch(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() < 2) throw std::runtime_error("usage: synth remote {register|attest|remove|list}");
  if (args[1] == "list") {
    const auto values = participants(roots);
    if (as_json) std::cout << values.dump() << '\n';
    else for (const auto& value : values) std::cout << value.at("identity").get<std::string>() << "  [" << value.at("state").get<std::string>() << "]\n";
    return 0;
  }
  realization::MutationLock lock(roots);
  if (args[1] == "register") return register_command(args, as_json, roots);
  if (args[1] == "attest") return attest_command(args, as_json, roots);
  if (args[1] == "remove") return remove_command(args, as_json, roots);
  throw std::runtime_error("unknown remote command");
}

json participants(const Roots& roots) {
  json result = json::array();
  for (const auto& registration : registrations_list(roots)) {
    const auto observation = valid_observation(roots, registration);
    const bool available = observation && observation->value("availability", "") == "AVAILABLE";
    json declared = registration.at("provides").at("surfaces");
    for (auto& surface : declared) surface["epistemic_class"] = "DECLARED";
    json observed = json::array();
    if (available) observed = observation->at("observed_surfaces");
    result.push_back({
      {"identity", registration.at("identity")}, {"version", registration.at("version")},
      {"description", registration.at("description")},
      {"state", observation ? observation->at("availability") : json("REGISTERED")},
      {"epistemic_class", "OBSERVED"}, {"participant_kind", "REMOTE"},
      {"identity_assurance", registration.at("identity_assurance")},
      {"registration_evidence_ref", (registrations(roots) / (registration.at("identity").get<std::string>() + ".json")).string()},
      {"realization_id", nullptr},
      {"realization_evidence_ref", observation ? json((observations(roots) / (registration.at("identity").get<std::string>() + ".json")).string()) : json(nullptr)},
      {"availability_observed_at", observation ? observation->at("observed_at") : json(nullptr)},
      {"observation_expires_at", observation ? observation->at("expires_at") : json(nullptr)},
      {"declared_surfaces", std::move(declared)}, {"observed_surfaces", std::move(observed)}
    });
  }
  return result;
}

json available_surfaces(const Roots& roots) {
  json result = json::array();
  for (const auto& participant : participants(roots)) {
    if (participant.value("state", "") != "AVAILABLE") continue;
    for (auto surface : participant.at("observed_surfaces")) result.push_back(std::move(surface));
  }
  return result;
}

} // namespace synth::remote
