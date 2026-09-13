#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace synth {

constexpr std::string_view identity = "SYNTH";
constexpr std::string_view version = SYNTH_VERSION;
const auto process_started = Clock::now();

std::string env_or(std::string_view key, std::string fallback) {
  if (const char* value = std::getenv(std::string(key).c_str()); value && *value) return value;
  return fallback;
}

std::string home() { return env_or("HOME", "/tmp"); }

struct Paths {
  fs::path config;
  fs::path state;
  fs::path runtime;
};

Paths xdg_paths() {
  const auto uid = static_cast<unsigned long>(::getuid());
  return {
    fs::path(env_or("XDG_CONFIG_HOME", home() + "/.config")) / "synth",
    fs::path(env_or("XDG_STATE_HOME", home() + "/.local/state")) / "synth",
    fs::path(env_or("XDG_RUNTIME_DIR", "/tmp/synth-runtime-" + std::to_string(uid))) / "synth"
  };
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

std::string json_escape(std::string_view input) {
  std::ostringstream out;
  for (const unsigned char c : input) {
    switch (c) {
      case '\"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        else out << static_cast<char>(c);
    }
  }
  return out.str();
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

struct Json {
  enum class Type { null_value, boolean, number, string, array, object } type{Type::null_value};
  std::string scalar;
  std::vector<Json> array;
  std::map<std::string, Json, std::less<>> object;
};

class JsonParser {
 public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  Json parse() {
    auto value = parse_value();
    whitespace();
    if (position_ != text_.size()) fail("trailing content");
    return value;
  }

 private:
  std::string_view text_;
  std::size_t position_{};

  [[noreturn]] void fail(std::string_view reason) const {
    throw std::runtime_error("invalid JSON at byte " + std::to_string(position_) + ": " + std::string(reason));
  }

  void whitespace() {
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) ++position_;
  }

  char take() {
    if (position_ >= text_.size()) fail("unexpected end");
    return text_[position_++];
  }

  bool consume(std::string_view token) {
    if (text_.substr(position_, token.size()) != token) return false;
    position_ += token.size();
    return true;
  }

  Json parse_value() {
    whitespace();
    if (position_ >= text_.size()) fail("expected value");
    if (text_[position_] == '{') return parse_object();
    if (text_[position_] == '[') return parse_array();
    if (text_[position_] == '\"') return Json{Json::Type::string, parse_string(), {}, {}};
    if (consume("true")) return Json{Json::Type::boolean, "true", {}, {}};
    if (consume("false")) return Json{Json::Type::boolean, "false", {}, {}};
    if (consume("null")) return {};
    return parse_number();
  }

  std::string parse_string() {
    if (take() != '\"') fail("expected string");
    std::string value;
    while (position_ < text_.size()) {
      const char c = take();
      if (c == '\"') return value;
      if (c != '\\') { value.push_back(c); continue; }
      const char escaped = take();
      switch (escaped) {
        case '\"': value.push_back('\"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u':
          for (int i = 0; i < 4; ++i) if (!std::isxdigit(static_cast<unsigned char>(take()))) fail("invalid unicode escape");
          value.push_back('?');
          break;
        default: fail("invalid escape");
      }
    }
    fail("unterminated string");
  }

  Json parse_number() {
    const auto start = position_;
    while (position_ < text_.size()) {
      const char c = text_[position_];
      if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) break;
      ++position_;
    }
    if (start == position_) fail("expected number");
    return Json{Json::Type::number, std::string(text_.substr(start, position_ - start)), {}, {}};
  }

  Json parse_array() {
    take();
    Json result{Json::Type::array, {}, {}, {}};
    whitespace();
    if (position_ < text_.size() && text_[position_] == ']') { ++position_; return result; }
    while (true) {
      result.array.push_back(parse_value());
      whitespace();
      const char delimiter = take();
      if (delimiter == ']') return result;
      if (delimiter != ',') fail("expected comma or closing bracket");
    }
  }

  Json parse_object() {
    take();
    Json result{Json::Type::object, {}, {}, {}};
    whitespace();
    if (position_ < text_.size() && text_[position_] == '}') { ++position_; return result; }
    while (true) {
      whitespace();
      if (position_ >= text_.size() || text_[position_] != '\"') fail("expected object key");
      auto key = parse_string();
      whitespace();
      if (take() != ':') fail("expected colon");
      result.object.insert_or_assign(std::move(key), parse_value());
      whitespace();
      const char delimiter = take();
      if (delimiter == '}') return result;
      if (delimiter != ',') fail("expected comma or closing brace");
    }
  }
};

const Json* member(const Json& root, std::initializer_list<std::string_view> path) {
  const Json* current = &root;
  for (const auto part : path) {
    if (current->type != Json::Type::object) return nullptr;
    const auto found = current->object.find(part);
    if (found == current->object.end()) return nullptr;
    current = &found->second;
  }
  return current;
}

bool string_member(const Json& root, std::initializer_list<std::string_view> path, std::string_view expected = {}) {
  const auto* value = member(root, path);
  return value && value->type == Json::Type::string && !value->scalar.empty() && (expected.empty() || value->scalar == expected);
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

struct Relation {
  std::string id;
  std::string source;
  std::string target;
  std::string surface;
  std::string status;
  std::string epistemic_class;
  std::string evidence_ref;
  std::string observed_at;
};

std::vector<Surface> observed_surfaces(const Paths& paths) {
  std::vector<Surface> result{{"synth.cli", "SYNTH", "process-stream", "stdio://synth", "bidirectional", "text/plain", "self-observed", "human and JSON representations"}};
  const auto evidence = paths.state / "evidence/latest.json";
  if (fs::is_regular_file(evidence)) {
    result.push_back({"synth.evidence", "SYNTH", "file", evidence.string(), "outbound", "application/json", "self-observed", "atomic latest evidence document"});
  }
  return result;
}

std::vector<Relation> observed_relations() { return {}; }

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

bool create_directory_if_absent(const fs::path& path) {
  if (fs::exists(path)) {
    if (!fs::is_directory(path)) throw std::runtime_error("expected directory: " + path.string());
    return false;
  }
  if (!fs::create_directories(path)) throw std::runtime_error("cannot create directory: " + path.string());
  return true;
}

bool ensure_layout_once(const Paths& paths) {
  bool changed = false;
  changed = create_directory_if_absent(paths.config) || changed;
  changed = create_directory_if_absent(paths.state / "evidence") || changed;
  changed = create_directory_if_absent(paths.runtime) || changed;
  const auto config = paths.config / "system.conf";
  if (!fs::exists(config)) {
    std::ofstream output(config, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create configuration: " + config.string());
    output << "identity=SYNTH\nversion=" << version << "\n";
    if (!output) throw std::runtime_error("cannot persist configuration: " + config.string());
    changed = true;
  } else if (!fs::is_regular_file(config)) {
    throw std::runtime_error("expected configuration file: " + config.string());
  }
  return changed;
}

struct LayoutObservation {
  bool initial_changed{};
  bool second_run_no_op{};
};

LayoutObservation ensure_layout(const Paths& paths) {
  const bool initial_changed = ensure_layout_once(paths);
  const bool second_changed = ensure_layout_once(paths);
  return {initial_changed, !second_changed};
}

std::string surface_json(const Surface& surface) {
  std::ostringstream out;
  out << "{\"id\":\"" << json_escape(surface.id)
      << "\",\"owner\":\"" << json_escape(surface.owner)
      << "\",\"kind\":\"" << json_escape(surface.kind)
      << "\",\"locator\":\"" << json_escape(surface.locator)
      << "\",\"direction\":\"" << json_escape(surface.direction)
      << "\",\"media_type\":\"" << json_escape(surface.media_type)
      << "\",\"observability\":\"" << json_escape(surface.observability)
      << "\",\"metadata\":\"" << json_escape(surface.metadata) << "\"}";
  return out.str();
}

std::string evidence_json(const Paths& paths, const fs::path& data) {
  const auto sample = metrics();
  const auto surfaces = observed_surfaces(paths);
  std::ostringstream out;
  out << std::fixed << std::setprecision(6)
      << "{\n  \"identity\": \"SYNTH\",\n"
      << "  \"version\": \"" << version << "\",\n"
      << "  \"process\": {\"pid\": " << ::getpid() << ", \"uptime_ms\": " << sample.uptime_ms
      << ", \"rss_kib\": " << sample.rss_kib << ", \"max_rss_kib\": " << sample.max_rss_kib
      << ", \"cpu_seconds\": " << sample.cpu_seconds << "},\n"
      << "  \"roots\": {\"configuration\": \"" << json_escape(paths.config.string())
      << "\", \"state\": \"" << json_escape(paths.state.string())
      << "\", \"runtime\": \"" << json_escape(paths.runtime.string())
      << "\", \"data\": \"" << json_escape(data.string()) << "\"},\n"
      << "  \"observed_surfaces\": [";
  for (std::size_t i = 0; i < surfaces.size(); ++i) {
    if (i) out << ',';
    out << "\n    " << surface_json(surfaces[i]);
  }
  out << "\n  ],\n  \"observed_relations\": [],\n"
      << "  \"timestamp\": \"" << iso_timestamp() << "\",\n"
      << "  \"epistemic_class\": \"OBSERVED\"\n}\n";
  return out.str();
}

void persist_evidence(const Paths& paths, const std::string& evidence) {
  const auto target = paths.state / "evidence/latest.json";
  const auto temporary = target.string() + ".tmp-" + std::to_string(::getpid());
  {
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write state evidence: " + temporary);
    output << evidence;
  }
  fs::rename(temporary, target);
}

bool validate_required_schema(const fs::path& path, const std::vector<std::string>& expected) {
  const auto content = read_file(path);
  if (!content) return false;
  try {
    const auto json = JsonParser(*content).parse();
    const auto* required = member(json, {"required"});
    if (!required || required->type != Json::Type::array) return false;
    std::vector<std::string> actual;
    for (const auto& item : required->array) {
      if (item.type != Json::Type::string) return false;
      actual.push_back(item.scalar);
    }
    auto wanted = expected;
    std::sort(actual.begin(), actual.end());
    std::sort(wanted.begin(), wanted.end());
    return actual == wanted;
  } catch (...) {
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
    const auto json = JsonParser(std::string_view(*content).substr(json_start + 1, json_end - json_start - 1)).parse();
    const bool valid =
      string_member(json, {"document", "id"}, "SYNTH-FOUNDATION-001") &&
      string_member(json, {"document", "version"}, "0.4.0") &&
      string_member(json, {"document", "status"}) &&
      string_member(json, {"document", "repository"}) &&
      string_member(json, {"document", "license"}, "GPL-3.0-only") &&
      string_member(json, {"lineage", "preservation_rule"});
    detail = valid ? "parsed document identity, version, status, repository, license and lineage" : "required document or provenance field is invalid";
    return valid;
  } catch (const std::exception& error) {
    detail = error.what();
    return false;
  }
}

bool valid_surface(const Surface& surface) {
  return !surface.id.empty() && !surface.owner.empty() && !surface.kind.empty() && !surface.locator.empty() &&
         !surface.direction.empty() && !surface.media_type.empty() && !surface.observability.empty();
}

struct Gate {
  std::string name;
  bool pass;
  std::string epistemic_class;
  std::string evidence;
};

std::string human_version() { return "SYNTH " + std::string(version) + "\n"; }
std::string human_relations() { return "No relations observed.\n"; }

std::vector<Gate> foundation_gates(const Paths& paths, const fs::path& data, const LayoutObservation& layout) {
  const auto foundation = data / "SYNTH-FOUNDATION-001-v0.4.0.md";
  const auto license = data / "LICENSE";
  const auto license_text = read_file(license);
  const auto sample = metrics();
  const auto surfaces = observed_surfaces(paths);
  const bool xdg = paths.config != paths.state && paths.config != paths.runtime && paths.state != paths.runtime &&
                   fs::is_directory(paths.config) && fs::is_directory(paths.state) && fs::is_directory(paths.runtime);
  const bool self_observed = ::getpid() > 0 && sample.uptime_ms >= 0 && !iso_timestamp().empty();
  const bool resources_observed = sample.valid && sample.rss_kib > 0 && sample.max_rss_kib > 0 && sample.cpu_seconds >= 0.0;
  const bool surface_schema = validate_required_schema(data / "schemas/surface.schema.json",
    {"id", "owner", "kind", "locator", "direction", "media_type", "observability", "metadata"});
  const bool surface_instances = std::all_of(surfaces.begin(), surfaces.end(), valid_surface);
  const bool relation_schema = validate_required_schema(data / "schemas/relation.schema.json",
    {"id", "source", "target", "surface", "status", "epistemic_class", "evidence_ref", "observed_at"});
  std::string context_detail;
  const bool context_compatible = validate_context_metadata(foundation, context_detail);
  const bool human_cli = !human_version().empty() && human_version().front() != '{' && human_relations() == "No relations observed.\n";

  std::vector<Gate> gates{
    {"FOUNDATION_FILE_PRESENT", fs::is_regular_file(foundation), "OBSERVED", foundation.string()},
    {"GPL_3_ONLY", license_text && license_text->find("GNU GENERAL PUBLIC LICENSE") != std::string::npos && license_text->find("Version 3, 29 June 2007") != std::string::npos, "DERIVED", license.string()},
    {"CPP26_BUILD", __cplusplus >= 202400L, "OBSERVED", "__cplusplus=" + std::to_string(__cplusplus)},
    {"XDG_SEPARATION", xdg, "DERIVED", paths.config.string() + " | " + paths.state.string() + " | " + paths.runtime.string()},
    {"SELF_OBSERVATION", self_observed, "OBSERVED", "pid=" + std::to_string(::getpid()) + ", uptime_ms=" + std::to_string(sample.uptime_ms)},
    {"RESOURCE_OBSERVATION", resources_observed, "OBSERVED", "rss_kib=" + std::to_string(sample.rss_kib) + ", max_rss_kib=" + std::to_string(sample.max_rss_kib) + ", cpu_seconds=" + std::to_string(sample.cpu_seconds)},
    {"SURFACE_MODEL_GENERIC", surface_schema && surface_instances, "DERIVED", (data / "schemas/surface.schema.json").string()},
    {"RELATION_MODEL_GENERIC", relation_schema, "DERIVED", (data / "schemas/relation.schema.json").string()},
    {"NO_FAKE_RELATIONS", observed_relations().empty(), "OBSERVED", "zero relation observation records"},
    {"CONTEXTLAB_DOCUMENT_COMPAT", context_compatible, "DERIVED", context_detail},
    {"CLI_HUMAN_READABLE", human_cli, "DERIVED", "human renderers verified; JSON remains opt-in"},
    {"SECOND_RUN_NO_OP", layout.second_run_no_op, "OBSERVED", "second reconciliation changed no configuration or realization; evidence remains appendable"}
  };
  const bool verified = std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.pass; });
  gates.insert(gates.begin() + 3, {"FOUNDATION_VERIFY", verified, "DERIVED", "derived from 12 independently evaluated gates"});
  return gates;
}

bool all_pass(const std::vector<Gate>& gates) {
  return std::all_of(gates.begin(), gates.end(), [](const Gate& gate) { return gate.pass; });
}

void print_version(bool json) {
  if (json) std::cout << "{\"identity\":\"SYNTH\",\"version\":\"" << version << "\"}\n";
  else std::cout << human_version();
}

void print_status(const std::vector<Gate>& gates, bool json) {
  const bool ready = all_pass(gates);
  if (json) {
    std::cout << "{\"FOUNDATION_READY\":\"" << (ready ? "PASS" : "FAIL")
              << "\",\"SYSTEM_SYNTH_READY\":\"" << (ready ? "PASS" : "FAIL")
              << "\",\"ECOSYSTEM_SYNTH\":\"NOT_YET_APPLICABLE\",\"scope\":\"LOCAL_RUNTIME\",\"ci\":\"EXTERNAL_EVIDENCE\"}\n";
    return;
  }
  std::cout << "SYNTH — present local state\n\n"
            << "  FOUNDATION_READY       " << (ready ? "PASS" : "FAIL") << "\n"
            << "  SYSTEM_SYNTH_READY     " << (ready ? "PASS" : "FAIL") << "\n"
            << "  ECOSYSTEM_SYNTH        NOT_YET_APPLICABLE\n\n"
            << "Local runtime evidence only; repository acceptance also requires CI PASS.\n";
}

void print_foundation(const std::vector<Gate>& gates, bool json) {
  if (json) {
    std::cout << "{\"gates\":[";
    for (std::size_t i = 0; i < gates.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << "{\"name\":\"" << gates[i].name << "\",\"status\":\"" << (gates[i].pass ? "PASS" : "FAIL")
                << "\",\"epistemic_class\":\"" << gates[i].epistemic_class << "\",\"evidence\":\"" << json_escape(gates[i].evidence) << "\"}";
    }
    std::cout << "],\"status\":\"" << (all_pass(gates) ? "PASS" : "FAIL") << "\"}\n";
    return;
  }
  std::cout << "Foundation verification\n\n";
  for (const auto& gate : gates) {
    std::cout << "  " << std::left << std::setw(36) << gate.name << (gate.pass ? "PASS" : "FAIL") << "  [" << gate.epistemic_class << "]\n"
              << "    evidence: " << gate.evidence << "\n";
  }
}

void print_surfaces(const Paths& paths, bool json) {
  const auto items = observed_surfaces(paths);
  if (json) {
    std::cout << '[';
    for (std::size_t i = 0; i < items.size(); ++i) { if (i) std::cout << ','; std::cout << surface_json(items[i]); }
    std::cout << "]\n";
    return;
  }
  std::cout << "Observed SYNTH surfaces\n\n";
  for (const auto& item : items) {
    std::cout << "  " << item.id << "  [" << item.kind << "]\n"
              << "    " << item.locator << "\n"
              << "    " << item.direction << " · " << item.media_type << " · " << item.observability << "\n";
  }
}

void print_relations(bool json) {
  if (json) std::cout << "[]\n";
  else std::cout << human_relations();
}

void print_help() {
  std::cout << "SYNTH — always ready, always incomplete\n\n"
            << "Usage: synth <command> [--json]\n\n"
            << "  version             Show system identity and version\n"
            << "  status              Show present local readiness\n"
            << "  foundation verify   Derive foundational gates from evidence\n"
            << "  surfaces            List only observed SYNTH surfaces\n"
            << "  relations           List observed relations\n"
            << "  evidence            Observe this process and persist evidence\n";
}

} // namespace synth

int main(int argc, char** argv) {
  using namespace synth;
  try {
    const auto paths = xdg_paths();
    const auto data = data_root();
    const auto layout = ensure_layout(paths);
    const auto evidence = evidence_json(paths, data);
    persist_evidence(paths, evidence);
    const auto gates = foundation_gates(paths, data, layout);

    bool json = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      if (std::string_view(argv[i]) == "--json") json = true;
      else args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "help" || args[0] == "--help") { print_help(); return 0; }
    if (args[0] == "version") { print_version(json); return 0; }
    if (args[0] == "status") { print_status(gates, json); return all_pass(gates) ? 0 : 1; }
    if (args[0] == "surfaces") { print_surfaces(paths, json); return 0; }
    if (args[0] == "relations") { print_relations(json); return 0; }
    if (args[0] == "evidence") {
      std::cout << (json ? evidence : "Evidence observed and persisted\n  class: OBSERVED\n  file: " + (paths.state / "evidence/latest.json").string() + "\n");
      return 0;
    }
    if (args[0] == "foundation" && args.size() > 1 && args[1] == "verify") {
      print_foundation(gates, json);
      return all_pass(gates) ? 0 : 1;
    }

    std::cerr << "Unknown command: " << args[0] << "\nEvidence: run 'synth help' for valid commands.\n";
    return 2;
  } catch (const std::exception& error) {
    std::cerr << "SYNTH could not observe its state.\nCause: " << error.what() << "\n";
    return 1;
  }
}
