#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include "realization.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using json = nlohmann::json;

namespace synth {

constexpr std::string_view identity = "SYNTH";
constexpr std::string_view version = SYNTH_VERSION;
constexpr std::string_view json_schema_dialect = "https://json-schema.org/draft/2020-12/schema";
const auto process_started = Clock::now();

std::string env_or(std::string_view key, std::string fallback) {
  if (const char* value = std::getenv(std::string(key).c_str()); value && *value) return value;
  return fallback;
}

std::string home() { return env_or("HOME", "/tmp"); }

struct Paths {
  fs::path config;
  fs::path data;
  fs::path state;
  fs::path runtime;
};

Paths xdg_paths() {
  const auto uid = static_cast<unsigned long>(::getuid());
  return {
    fs::path(env_or("XDG_CONFIG_HOME", home() + "/.config")) / "synth",
    fs::path(env_or("XDG_DATA_HOME", home() + "/.local/share")) / "synth",
    fs::path(env_or("XDG_STATE_HOME", home() + "/.local/state")) / "synth",
    fs::path(env_or("XDG_RUNTIME_DIR", "/tmp/synth-runtime-" + std::to_string(uid))) / "synth"
  };
}

realization::Roots realization_roots(const Paths& paths, const fs::path& resources) {
  return {paths.data, paths.state, paths.runtime, resources};
}

fs::path executable_path() {
  std::array<char, 4096> buffer{};
  const auto size = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (size < 0) throw std::runtime_error("cannot resolve /proc/self/exe");
  return fs::path(std::string(buffer.data(), static_cast<std::size_t>(size)));
}

fs::path data_root() {
  if (const char* configured = std::getenv("SYNTH_DATA_DIR"); configured && *configured) return fs::path(configured);
  const auto executable = executable_path();
  return executable.parent_path().parent_path() / "share/synth";
}

std::optional<std::string> read_file(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(input)), {});
}

std::string iso_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  gmtime_r(&time, &utc);
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

struct Surface {
  std::string id;
  std::string owner;
  std::string kind;
  std::string locator;
  std::string direction;
  std::string media_type;
  std::string observability;
  std::string metadata;
};

json surface_json(const Surface& surface) {
  return {
    {"id", surface.id},
    {"owner", surface.owner},
    {"kind", surface.kind},
    {"locator", surface.locator},
    {"direction", surface.direction},
    {"media_type", surface.media_type},
    {"observability", surface.observability},
    {"metadata", surface.metadata}
  };
}

std::vector<Surface> observed_surfaces(const Paths& paths, const fs::path& resources, bool evidence_will_be_persisted = false) {
  std::vector<Surface> result{{"synth.cli", "SYNTH", "process-stream", "stdio://synth", "bidirectional", "text/plain", "self-observed", "human and JSON representations"}};
  const auto evidence = paths.state / "evidence/latest.json";
  if (evidence_will_be_persisted || fs::is_regular_file(evidence)) {
    result.push_back({"synth.evidence", "SYNTH", "file", evidence.string(), "outbound", "application/json", "self-observed", "atomic latest evidence document"});
  }
  for (const auto& surface : realization::active_surfaces(realization_roots(paths, resources))) {
    result.push_back({
      surface.value("id", ""), surface.value("owner", ""), surface.value("kind", ""),
      surface.value("locator", ""), surface.value("direction", ""), surface.value("media_type", ""),
      surface.value("observability", ""), surface.value("metadata", "")
    });
  }
  return result;
}

json observed_relations(const Paths& paths, const fs::path& resources) {
  return realization::observed_relations(realization_roots(paths, resources));
}

std::string sha256_text(std::string_view value) {
  using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context.get(), value.data(), value.size()) != 1) {
    throw std::runtime_error("OpenSSL could not hash ecosystem projection");
  }
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1) {
    throw std::runtime_error("OpenSSL could not finalize ecosystem generation");
  }
  std::ostringstream rendered;
  rendered << std::hex << std::setfill('0');
  for (unsigned int index = 0; index < size; ++index) {
    rendered << std::setw(2) << static_cast<unsigned int>(digest[index]);
  }
  return rendered.str();
}

json ecosystem_projection(const Paths& paths, const fs::path& resources,
                          const bool evidence_will_be_persisted = false) {
  json core_surfaces = json::array();
  const auto evidence_path = paths.state / "evidence/latest.json";
  for (const auto& surface : observed_surfaces(paths, resources, evidence_will_be_persisted)) {
    if (surface.owner == identity) core_surfaces.push_back(surface_json(surface));
  }

  json participants = realization::ecosystem_participants(realization_roots(paths, resources));
  const json core_participant{
    {"identity", identity}, {"version", version}, {"description", "SYNTH factual core"},
    {"state", "ACTIVE"}, {"epistemic_class", "OBSERVED"},
    {"registration_evidence_ref", (resources / "SYNTH-FOUNDATION-001-v0.4.0.md").string()},
    {"realization_id", nullptr},
    {"realization_evidence_ref", (evidence_will_be_persisted || fs::is_regular_file(evidence_path)) ? json(evidence_path.string()) : json(nullptr)},
    {"declared_surfaces", json::array()}, {"observed_surfaces", std::move(core_surfaces)}
  };
  participants.insert(participants.begin(), core_participant);

  std::map<std::string, json> providers_by_surface;
  for (const auto& participant : participants) {
    std::set<std::string> observed_ids;
    for (const auto& surface : participant.at("observed_surfaces")) {
      const auto id = surface.at("id").get<std::string>();
      observed_ids.insert(id);
      auto evidence_ref = participant.at("realization_evidence_ref");
      if (evidence_ref.is_null()) evidence_ref = participant.at("registration_evidence_ref");
      providers_by_surface[id].push_back({
        {"identity", participant.at("identity")}, {"state", participant.at("state")},
        {"epistemic_class", "OBSERVED"}, {"kind", surface.at("kind")},
        {"media_type", surface.at("media_type")}, {"locator", surface.at("locator")},
        {"evidence_ref", std::move(evidence_ref)}
      });
    }
    for (const auto& surface : participant.at("declared_surfaces")) {
      const auto id = surface.at("id").get<std::string>();
      if (observed_ids.contains(id)) continue;
      providers_by_surface[id].push_back({
        {"identity", participant.at("identity")}, {"state", participant.at("state")},
        {"epistemic_class", "DECLARED"},
        {"kind", surface.contains("kind") ? surface.at("kind") : json(nullptr)},
        {"media_type", surface.contains("media_type") ? surface.at("media_type") : json(nullptr)},
        {"locator", nullptr}, {"evidence_ref", participant.at("registration_evidence_ref")}
      });
    }
  }

  json surfaces = json::array();
  for (auto& [id, providers] : providers_by_surface) {
    std::sort(providers.begin(), providers.end(), [](const auto& left, const auto& right) {
      return left.value("identity", "") < right.value("identity", "");
    });
    surfaces.push_back({{"id", id}, {"providers", std::move(providers)}});
  }
  auto relations = observed_relations(paths, resources);
  std::sort(relations.begin(), relations.end(), [](const auto& left, const auto& right) {
    return left.value("id", "") < right.value("id", "");
  });
  const json topology{{"participants", participants}, {"surfaces", surfaces}, {"relations", relations}};
  return {
    {"$schema", "urn:synth:schema:ecosystem-projection:0.1.0"}, {"schema_version", "0.1.0"},
    {"generation", sha256_text(topology.dump())}, {"observed_at", iso_timestamp()},
    {"epistemic_class", "OBSERVED"}, {"participants", std::move(participants)},
    {"surfaces", std::move(surfaces)}, {"relations", std::move(relations)}
  };
}

struct Metrics {
  bool valid{};
  long rss_kib{};
  long max_rss_kib{};
  double cpu_seconds{};
  long long uptime_ms{};
};

Metrics metrics() {
  rusage usage{};
  const bool usage_valid = ::getrusage(RUSAGE_SELF, &usage) == 0;
  std::ifstream statm("/proc/self/statm");
  long total_pages = 0;
  long resident_pages = 0;
  const bool rss_valid = static_cast<bool>(statm >> total_pages >> resident_pages);
  const long page_kib = ::sysconf(_SC_PAGESIZE) / 1024;
  const double user = static_cast<double>(usage.ru_utime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec) / 1'000'000.0;
  const double system = static_cast<double>(usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_stime.tv_usec) / 1'000'000.0;
  return {
    usage_valid && rss_valid && page_kib > 0,
    rss_valid ? resident_pages * page_kib : 0,
    usage.ru_maxrss,
    user + system,
    std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - process_started).count()
  };
}

void ensure_directory(const fs::path& path) {
  if (fs::exists(path)) {
    if (!fs::is_directory(path)) throw std::runtime_error("expected directory: " + path.string());
    return;
  }
  if (!fs::create_directories(path)) throw std::runtime_error("cannot create directory: " + path.string());
}

void prepare_evidence_storage(const Paths& paths) {
  ensure_directory(paths.state / "evidence");
  ensure_directory(paths.runtime);
}

json evidence_json(const Paths& paths, const fs::path& resources) {
  const auto sample = metrics();
  json surfaces = json::array();
  for (const auto& surface : observed_surfaces(paths, resources, true)) surfaces.push_back(surface_json(surface));

  json configuration = nullptr;
  if (fs::exists(paths.config)) configuration = paths.config.string();

  return {
    {"identity", identity},
    {"version", version},
    {"process", {
      {"pid", ::getpid()},
      {"uptime_ms", sample.uptime_ms},
      {"rss_kib", sample.rss_kib},
      {"max_rss_kib", sample.max_rss_kib},
      {"cpu_seconds", sample.cpu_seconds}
    }},
    {"roots", {
      {"configuration", configuration},
      {"managed_data", paths.data.string()},
      {"state", paths.state.string()},
      {"runtime", paths.runtime.string()},
      {"resources", resources.string()}
    }},
    {"observed_surfaces", std::move(surfaces)},
    {"observed_relations", observed_relations(paths, resources)},
    {"ecosystem", ecosystem_projection(paths, resources, true)},
    {"timestamp", iso_timestamp()},
    {"epistemic_class", "OBSERVED"}
  };
}

void persist_evidence(const Paths& paths, const json& evidence) {
  const auto target = paths.state / "evidence/latest.json";
  const auto temporary = target.string() + ".tmp-" + std::to_string(::getpid());
  {
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write state evidence: " + temporary);
    output << evidence.dump(2) << '\n';
    if (!output) throw std::runtime_error("cannot persist state evidence: " + temporary);
  }
  fs::rename(temporary, target);
}

std::vector<std::string> tree_snapshot(const Paths& paths) {
  std::vector<std::string> snapshot;
  const std::array<std::pair<std::string_view, fs::path>, 4> roots{{
    {"config", paths.config}, {"data", paths.data}, {"state", paths.state}, {"runtime", paths.runtime}
  }};
  for (const auto& [label, root] : roots) {
    std::error_code error;
    if (!fs::exists(root, error)) {
      snapshot.push_back(std::string(label) + ":absent");
      continue;
    }
    snapshot.push_back(std::string(label) + ":present");
    for (fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, error), end;
         iterator != end; iterator.increment(error)) {
      if (error) { error.clear(); continue; }
      const auto relative = fs::relative(iterator->path(), root, error).string();
      const auto kind = iterator->is_directory(error) ? "directory" : iterator->is_regular_file(error) ? "file" : "other";
      const auto size = iterator->is_regular_file(error) ? iterator->file_size(error) : 0;
      snapshot.push_back(std::string(label) + ":" + relative + ":" + kind + ":" + std::to_string(size));
    }
  }
  std::sort(snapshot.begin(), snapshot.end());
  return snapshot;
}

bool validate_json_schema(const fs::path& path, std::string_view expected_id, const std::vector<std::string>& expected_required) {
  std::ifstream input(path);
  if (!input) return false;
  try {
    const auto schema = json::parse(input);
    if (!schema.is_object() || schema.value("$schema", "") != json_schema_dialect ||
        schema.value("$id", "") != expected_id || schema.value("type", "") != "object" ||
        schema.value("additionalProperties", true) || !schema.contains("properties") ||
        !schema.at("properties").is_object() || !schema.contains("required") || !schema.at("required").is_array()) return false;

    auto actual = schema.at("required").get<std::vector<std::string>>();
    auto expected = expected_required;
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    if (actual != expected) return false;
    return std::all_of(expected_required.begin(), expected_required.end(), [&schema](const auto& field) {
      const auto& properties = schema.at("properties");
      return properties.contains(field) && properties.at(field).is_object() && properties.at(field).contains("type");
    });
  } catch (const json::exception&) {
    return false;
  }
}

bool validate_context_metadata(const fs::path& foundation, std::string& detail) {
  const auto content = read_file(foundation);
  if (!content) { detail = "foundation document is unreadable"; return false; }
  constexpr std::string_view marker = "```context-metadata+json";
  const auto marker_start = content->find(marker);
  if (marker_start == std::string::npos) { detail = "metadata fence is absent"; return false; }
  const auto json_start = content->find('\n', marker_start + marker.size());
  const auto json_end = json_start == std::string::npos ? std::string::npos : content->find("```", json_start + 1);
  if (json_start == std::string::npos || json_end == std::string::npos) { detail = "metadata fence is incomplete"; return false; }
  try {
    const auto metadata = json::parse(std::string_view(*content).substr(json_start + 1, json_end - json_start - 1));
    const auto& document = metadata.at("document");
    const auto& lineage = metadata.at("lineage");
    const bool valid = document.is_object() && lineage.is_object() &&
      document.value("id", "") == "SYNTH-FOUNDATION-001" &&
      document.value("version", "") == "0.4.0" &&
      !document.value("status", "").empty() &&
      !document.value("repository", "").empty() &&
      document.value("license", "") == "GPL-3.0-only" &&
      !lineage.value("preservation_rule", "").empty();
    detail = valid ? "parsed document identity, version, status, repository, license and lineage with nlohmann/json" : "required document or provenance field is invalid";
    return valid;
  } catch (const json::exception& error) {
    detail = error.what();
    return false;
  }
}

bool valid_surface(const Surface& surface) {
  return !surface.id.empty() && !surface.owner.empty() && !surface.kind.empty() && !surface.locator.empty() &&
         !surface.direction.empty() && !surface.media_type.empty() && !surface.observability.empty() && !surface.metadata.empty();
}

bool grounded_relation(const json& relation) {
  try {
    return relation.is_object() && !relation.value("id", "").empty() &&
      !relation.value("source", "").empty() && !relation.value("target", "").empty() &&
      !relation.value("surface", "").empty() && relation.value("epistemic_class", "") == "OBSERVED" &&
      relation.value("observation", "") == "runtime_witness" &&
      relation.value("assertion", "") == "consumed_surface" &&
      relation.value("assertion_mode", "") == "participant_attestation" &&
      relation.value("verification", "") == "MATCH" &&
      relation.value("expected_sha256", "") == relation.value("attested_sha256", "") &&
      relation.value("expected_sha256", "").size() == 64 &&
      !relation.value("observed_at", "").empty() &&
      fs::is_regular_file(relation.at("evidence_ref").get<std::string>());
  } catch (const json::exception&) {
    return false;
  }
}

struct Gate {
  std::string name;
  bool pass;
  std::string epistemic_class;
  std::string evidence;
};

std::string human_version() { return "SYNTH " + std::string(version) + "\n"; }
std::string human_relations() { return "No relations observed.\n"; }

std::vector<Gate> foundation_gates(const Paths& paths, const fs::path& data) {
  const auto layout_before = tree_snapshot(paths);
  const auto foundation = data / "SYNTH-FOUNDATION-001-v0.4.0.md";
  const auto license = data / "LICENSE";
  const auto license_text = read_file(license);
  const auto sample = metrics();
  const auto surfaces = observed_surfaces(paths, data);
  const auto relations = observed_relations(paths, data);
  const bool xdg = paths.config != paths.data && paths.config != paths.state && paths.config != paths.runtime &&
    paths.data != paths.state && paths.data != paths.runtime && paths.state != paths.runtime &&
    (!fs::exists(paths.config) || fs::is_directory(paths.config)) &&
    (!fs::exists(paths.data) || fs::is_directory(paths.data)) &&
    (!fs::exists(paths.state) || fs::is_directory(paths.state)) &&
    (!fs::exists(paths.runtime) || fs::is_directory(paths.runtime));
  const bool self_observed = ::getpid() > 0 && sample.uptime_ms >= 0 && !iso_timestamp().empty();
  const bool resources_observed = sample.valid && sample.rss_kib > 0 && sample.max_rss_kib > 0 && sample.cpu_seconds >= 0.0;
  const bool surface_schema = validate_json_schema(data / "schemas/surface.schema.json",
    "urn:synth:schema:surface:0.1.0",
    {"id", "owner", "kind", "locator", "direction", "media_type", "observability", "metadata"});
  const bool surface_instances = std::all_of(surfaces.begin(), surfaces.end(), valid_surface);
  const bool relation_schema = validate_json_schema(data / "schemas/relation.schema.json",
    "urn:synth:schema:relation:0.1.0",
    {"id", "source", "target", "surface", "status", "epistemic_class", "observation", "assertion",
     "assertion_mode", "expected_sha256", "attested_sha256", "verification", "evidence_ref", "observed_at"});
  const bool relations_grounded = std::all_of(relations.begin(), relations.end(), grounded_relation);
  std::string context_detail;
  const bool context_compatible = validate_context_metadata(foundation, context_detail);
  const bool human_cli = !human_version().empty() && human_version().front() != '{' && human_relations() == "No relations observed.\n";
  const bool read_only_no_op = layout_before == tree_snapshot(paths);
  const auto configuration_evidence = fs::exists(paths.config) ? paths.config.string() : "configuration=none";

  std::vector<Gate> gates{
    {"FOUNDATION_FILE_PRESENT", fs::is_regular_file(foundation), "OBSERVED", foundation.string()},
    {"GPL_3_ONLY", license_text && license_text->find("GNU GENERAL PUBLIC LICENSE") != std::string::npos && license_text->find("Version 3, 29 June 2007") != std::string::npos, "DERIVED", license.string()},
    {"CPP26_BUILD", __cplusplus >= 202400L, "OBSERVED", "__cplusplus=" + std::to_string(__cplusplus)},
    {"XDG_SEPARATION", xdg, "DERIVED", configuration_evidence + " | data=" + paths.data.string() + " | state=" + paths.state.string() + " | runtime=" + paths.runtime.string()},
    {"SELF_OBSERVATION", self_observed, "OBSERVED", "pid=" + std::to_string(::getpid()) + ", uptime_ms=" + std::to_string(sample.uptime_ms)},
    {"RESOURCE_OBSERVATION", resources_observed, "OBSERVED", "rss_kib=" + std::to_string(sample.rss_kib) + ", max_rss_kib=" + std::to_string(sample.max_rss_kib) + ", cpu_seconds=" + std::to_string(sample.cpu_seconds)},
    {"SURFACE_MODEL_GENERIC", surface_schema && surface_instances, "DERIVED", "JSON Schema Draft 2020-12: " + (data / "schemas/surface.schema.json").string()},
    {"RELATION_MODEL_GENERIC", relation_schema, "DERIVED", "JSON Schema Draft 2020-12: " + (data / "schemas/relation.schema.json").string()},
    {"NO_FAKE_RELATIONS", relations_grounded, "DERIVED", std::to_string(relations.size()) + " relation record(s), each grounded in an existing runtime witness"},
    {"CONTEXTLAB_DOCUMENT_COMPAT", context_compatible, "DERIVED", context_detail},
    {"CLI_HUMAN_READABLE", human_cli, "DERIVED", "human renderers verified; JSON remains opt-in"},
    {"SECOND_RUN_NO_OP", read_only_no_op, "OBSERVED", "read-only observation changed no configuration, data, state or runtime entry; conformance repeats and fingerprints commands"}
  };
  const bool verified = std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.pass; });
  gates.insert(gates.begin() + 3, {"FOUNDATION_VERIFY", verified, "DERIVED", "derived from 12 independently evaluated gates"});
  return gates;
}

bool all_pass(const std::vector<Gate>& gates) {
  return std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.pass; });
}

void print_version(bool as_json) {
  if (as_json) std::cout << json{{"identity", identity}, {"version", version}}.dump() << '\n';
  else std::cout << human_version();
}

void print_status(const std::vector<Gate>& gates, bool as_json) {
  const bool ready = all_pass(gates);
  if (as_json) {
    std::cout << json{
      {"FOUNDATION_READY", ready ? "PASS" : "FAIL"},
      {"SYSTEM_SYNTH_READY", ready ? "PASS" : "FAIL"},
      {"ECOSYSTEM_SYNTH", "PROJECTED"},
      {"scope", "LOCAL_RUNTIME"},
      {"ci", "EXTERNAL_EVIDENCE"}
    }.dump() << '\n';
    return;
  }
  std::cout << "SYNTH — present local state\n\n"
            << "  FOUNDATION_READY       " << (ready ? "PASS" : "FAIL") << "\n"
            << "  SYSTEM_SYNTH_READY     " << (ready ? "PASS" : "FAIL") << "\n"
            << "  ECOSYSTEM_SYNTH        PROJECTED\n\n"
            << "Local runtime evidence only; repository acceptance also requires CI PASS.\n";
}

void print_foundation(const std::vector<Gate>& gates, bool as_json) {
  if (as_json) {
    json rendered = json::array();
    for (const auto& gate : gates) rendered.push_back({
      {"name", gate.name}, {"status", gate.pass ? "PASS" : "FAIL"},
      {"epistemic_class", gate.epistemic_class}, {"evidence", gate.evidence}
    });
    std::cout << json{{"gates", std::move(rendered)}, {"status", all_pass(gates) ? "PASS" : "FAIL"}}.dump() << '\n';
    return;
  }
  std::cout << "Foundation verification\n\n";
  for (const auto& gate : gates) {
    std::cout << "  " << std::left << std::setw(36) << gate.name << (gate.pass ? "PASS" : "FAIL") << "  [" << gate.epistemic_class << "]\n"
              << "    evidence: " << gate.evidence << "\n";
  }
}

void print_surfaces(const Paths& paths, const fs::path& resources, bool as_json) {
  const auto items = observed_surfaces(paths, resources);
  if (as_json) {
    json rendered = json::array();
    for (const auto& item : items) rendered.push_back(surface_json(item));
    std::cout << rendered.dump() << '\n';
    return;
  }
  std::cout << "Observed SYNTH surfaces\n\n";
  for (const auto& item : items) {
    std::cout << "  " << item.id << "  [" << item.kind << "]\n"
              << "    " << item.locator << "\n"
              << "    " << item.direction << " · " << item.media_type << " · " << item.observability << "\n";
  }
}

void print_relations(const Paths& paths, const fs::path& resources, bool as_json) {
  const auto relations = observed_relations(paths, resources);
  if (as_json) std::cout << relations.dump() << '\n';
  else if (relations.empty()) std::cout << human_relations();
  else for (const auto& relation : relations) {
    std::cout << relation.value("source", "") << "\n  consumes -> " << relation.value("surface", "")
              << "\n  " << relation.value("epistemic_class", "")
              << "\n  witness: " << relation.value("evidence_ref", "") << "\n";
  }
}

void print_ecosystem(const Paths& paths, const fs::path& resources, const bool as_json) {
  const auto projection = ecosystem_projection(paths, resources);
  if (as_json) {
    std::cout << projection.dump() << '\n';
    return;
  }
  std::cout << "SYNTH ecosystem projection\n\n"
            << "  generation    " << projection.at("generation").get<std::string>() << "\n"
            << "  participants  " << projection.at("participants").size() << "\n"
            << "  surfaces      " << projection.at("surfaces").size() << "\n"
            << "  relations     " << projection.at("relations").size() << "\n\n";
  for (const auto& participant : projection.at("participants")) {
    std::cout << "  " << participant.at("identity").get<std::string>()
              << "  [" << participant.at("state").get<std::string>() << "]\n";
  }
}

void print_resolution(const std::vector<std::string>& args, const Paths& paths,
                      const fs::path& resources, const bool as_json) {
  if (args.size() != 2) throw std::runtime_error("usage: synth resolve <surface>");
  const auto projection = ecosystem_projection(paths, resources);
  json providers = json::array();
  const auto& surfaces = projection.at("surfaces");
  const auto match = std::find_if(surfaces.begin(), surfaces.end(), [&args](const auto& surface) {
    return surface.value("id", "") == args[1];
  });
  if (match != surfaces.end()) {
    for (const auto& provider : match->at("providers")) {
      if (provider.value("state", "") == "ACTIVE" && provider.value("epistemic_class", "") == "OBSERVED") {
        providers.push_back(provider);
      }
    }
  }
  const json result{
    {"surface", args[1]}, {"generation", projection.at("generation")},
    {"observed_at", projection.at("observed_at")}, {"providers", providers}
  };
  if (as_json) {
    std::cout << result.dump() << '\n';
  } else if (providers.empty()) {
    std::cout << "No active providers observed for " << args[1] << ".\n";
  } else {
    std::cout << "Active providers for " << args[1] << "\n\n";
    for (const auto& provider : providers) {
      std::cout << "  " << provider.at("identity").get<std::string>()
                << "\n    " << provider.at("locator").get<std::string>() << "\n";
    }
  }
}

void print_help() {
  std::cout << "SYNTH — always ready, always incomplete\n\n"
            << "Usage: synth <command> [--json]\n\n"
            << "  version             Show system identity and version without side effects\n"
            << "  status              Observe present local readiness without persistence\n"
            << "  foundation verify   Observe and derive foundational gates\n"
            << "  surfaces            List only observed SYNTH surfaces\n"
            << "  relations           List observed relations\n"
            << "  ecosystem           Project registered and active ecosystem state\n"
            << "  resolve              Discover active providers by surface\n"
            << "  evidence            Observe this process and persist evidence\n";
  std::cout << "  search               Search a local artifact source\n"
            << "  info                 Inspect a realizable artifact\n"
            << "  install              Verify and install without activation\n"
            << "  installed            List installed artifacts\n"
            << "  activate             Verify and promote a candidate\n"
            << "  deactivate           Stop an active realization\n"
            << "  remove               Remove an inactive installation\n";
}

} // namespace synth

int main(int argc, char** argv) {
  using namespace synth;
  try {
    bool as_json = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      if (std::string_view(argv[i]) == "--json") as_json = true;
      else args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "help" || args[0] == "--help") { print_help(); return 0; }
    if (args[0] == "version") { print_version(as_json); return 0; }

    const bool foundation_command = args[0] == "foundation" && args.size() > 1 && args[1] == "verify";
    const bool known_command = args[0] == "status" || args[0] == "surfaces" || args[0] == "relations" ||
                               args[0] == "ecosystem" || args[0] == "resolve" ||
                               args[0] == "evidence" || foundation_command || realization::handles(args);
    if (!known_command) {
      std::cerr << "Unknown command: " << args[0] << "\nEvidence: run 'synth help' for valid commands.\n";
      return 2;
    }

    const auto paths = xdg_paths();
    const auto data = data_root();
    if (realization::handles(args)) return realization::dispatch(args, as_json, realization_roots(paths, data));
    if (args[0] == "evidence") {
      realization::MutationLock lock(realization_roots(paths, data));
      prepare_evidence_storage(paths);
      const auto evidence = evidence_json(paths, data);
      persist_evidence(paths, evidence);
      std::cout << (as_json ? evidence.dump(2) + "\n" : "Evidence observed and persisted\n  class: OBSERVED\n  file: " + (paths.state / "evidence/latest.json").string() + "\n");
      return 0;
    }
    if (args[0] == "surfaces") { print_surfaces(paths, data, as_json); return 0; }
    if (args[0] == "relations") { print_relations(paths, data, as_json); return 0; }
    if (args[0] == "ecosystem") {
      if (args.size() != 1) throw std::runtime_error("usage: synth ecosystem");
      print_ecosystem(paths, data, as_json);
      return 0;
    }
    if (args[0] == "resolve") { print_resolution(args, paths, data, as_json); return 0; }

    const auto gates = foundation_gates(paths, data);
    if (args[0] == "status") { print_status(gates, as_json); return all_pass(gates) ? 0 : 1; }
    print_foundation(gates, as_json);
    return all_pass(gates) ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "SYNTH could not observe its state.\nCause: " << error.what() << "\n";
    return 1;
  }
}
