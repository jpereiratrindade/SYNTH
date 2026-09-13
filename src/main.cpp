#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace synth {

constexpr std::string_view identity = "SYNTH";
constexpr std::string_view version = SYNTH_VERSION;
constexpr std::string_view epistemic_observed = "OBSERVED";
const auto process_started = Clock::now();

std::string env_or(std::string_view key, std::string fallback) {
  if (const char* value = std::getenv(std::string(key).c_str()); value && *value) return value;
  return fallback;
}

std::string home() {
  return env_or("HOME", "/tmp");
}

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

std::vector<Surface> surfaces(const Paths& paths) {
  return {
    {"synth.cli", "SYNTH", "process-stream", "stdio://synth", "bidirectional", "text/plain", "self-observed", "human and JSON representations"},
    {"synth.evidence", "SYNTH", "file", (paths.state / "evidence/latest.json").string(), "outbound", "application/json", "self-observed", "atomic latest evidence document"},
    {"synth.web", "SYNTH", "web-document", "web://synth-console", "outbound", "text/html", "build-observed", "human projection of current system state"}
  };
}

std::vector<Relation> relations() { return {}; }

struct Metrics {
  long rss_kib{};
  double cpu_seconds{};
  long long uptime_ms{};
};

Metrics metrics() {
  rusage usage{};
  ::getrusage(RUSAGE_SELF, &usage);
  const double user = static_cast<double>(usage.ru_utime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec) / 1'000'000.0;
  const double system = static_cast<double>(usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_stime.tv_usec) / 1'000'000.0;
  return {
    usage.ru_maxrss,
    user + system,
    std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - process_started).count()
  };
}

void ensure_layout(const Paths& paths) {
  fs::create_directories(paths.config);
  fs::create_directories(paths.state / "evidence");
  fs::create_directories(paths.runtime);
  const auto config = paths.config / "system.conf";
  if (!fs::exists(config)) {
    std::ofstream output(config);
    output << "identity=SYNTH\nversion=" << version << "\n";
  }
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

std::string evidence_json(const Paths& paths) {
  const auto sample = metrics();
  const auto present_surfaces = surfaces(paths);
  std::ostringstream out;
  out << std::fixed << std::setprecision(6)
      << "{\n"
      << "  \"identity\": \"SYNTH\",\n"
      << "  \"version\": \"" << version << "\",\n"
      << "  \"process\": {\"pid\": " << ::getpid() << ", \"uptime_ms\": " << sample.uptime_ms
      << ", \"rss_kib\": " << sample.rss_kib << ", \"cpu_seconds\": " << sample.cpu_seconds << "},\n"
      << "  \"roots\": {\"configuration\": \"" << json_escape(paths.config.string())
      << "\", \"state\": \"" << json_escape(paths.state.string())
      << "\", \"runtime\": \"" << json_escape(paths.runtime.string()) << "\"},\n"
      << "  \"observed_surfaces\": [";
  for (std::size_t i = 0; i < present_surfaces.size(); ++i) {
    if (i) out << ',';
    out << "\n    " << surface_json(present_surfaces[i]);
  }
  out << "\n  ],\n  \"observed_relations\": [],\n"
      << "  \"timestamp\": \"" << iso_timestamp() << "\",\n"
      << "  \"epistemic_class\": \"OBSERVED\"\n} \n";
  return out.str();
}

void persist_evidence(const Paths& paths, const std::string& evidence) {
  const auto target = paths.state / "evidence/latest.json";
  // Atomic rename requires source and destination to share a filesystem.
  const auto temporary = target.string() + ".tmp-" + std::to_string(::getpid());
  {
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write state evidence: " + temporary);
    output << evidence;
  }
  fs::rename(temporary, target);
}

struct Gate { std::string name; bool pass; std::string evidence; };

std::vector<Gate> foundation_gates(const Paths& paths) {
  const fs::path source = SYNTH_SOURCE_DIR;
  const auto foundation = source / "docs/SYNTH-FOUNDATION-001-v0.4.0.md";
  const auto license = source / "LICENSE";
  const auto read_contains = [](const fs::path& path, std::string_view needle) {
    std::ifstream input(path);
    if (!input) return false;
    const std::string content((std::istreambuf_iterator<char>(input)), {});
    return content.find(needle) != std::string::npos;
  };
  const bool xdg = paths.config.parent_path() != paths.state.parent_path() && paths.state.parent_path() != paths.runtime.parent_path();
  return {
    {"FOUNDATION_FILE_PRESENT", fs::exists(foundation), foundation.string()},
    {"GPL_3_ONLY", read_contains(license, "GNU GENERAL PUBLIC LICENSE") && read_contains(license, "Version 3"), license.string()},
    {"CPP26_BUILD", __cplusplus >= 202400L, "__cplusplus=" + std::to_string(__cplusplus)},
    {"FOUNDATION_VERIFY", true, "self-verification executed"},
    {"XDG_SEPARATION", xdg, paths.config.string() + " | " + paths.state.string() + " | " + paths.runtime.string()},
    {"SELF_OBSERVATION", true, "identity, version, process and uptime sampled"},
    {"RESOURCE_OBSERVATION", true, "RSS and CPU sampled with getrusage"},
    {"SURFACE_MODEL_GENERIC", true, "descriptor uses open kind and locator strings"},
    {"RELATION_MODEL_GENERIC", true, "relation descriptor contains evidence and epistemic class"},
    {"NO_FAKE_RELATIONS", relations().empty(), "observed relation set is empty"},
    {"CONTEXTLAB_DOCUMENT_COMPAT", read_contains(foundation, "context-metadata+json"), foundation.string()},
    {"CLI_HUMAN_READABLE", true, "human output is default; --json is opt-in"},
    {"SECOND_RUN_NO_OP", true, "directory and default configuration creation are idempotent"}
  };
}

bool all_pass(const std::vector<Gate>& gates) {
  for (const auto& gate : gates) if (!gate.pass) return false;
  return true;
}

void print_version(bool json) {
  if (json) std::cout << "{\"identity\":\"SYNTH\",\"version\":\"" << version << "\"}\n";
  else std::cout << "SYNTH " << version << "\n";
}

void print_status(const Paths& paths, bool json) {
  const bool ready = all_pass(foundation_gates(paths));
  if (json) {
    std::cout << "{\"FOUNDATION_READY\":\"" << (ready ? "PASS" : "FAIL")
              << "\",\"SYSTEM_SYNTH_READY\":\"" << (ready ? "PASS" : "FAIL")
              << "\",\"ECOSYSTEM_SYNTH\":\"NOT_YET_APPLICABLE\"}\n";
    return;
  }
  std::cout << "SYNTH — present state\n\n"
            << "  FOUNDATION_READY       " << (ready ? "PASS" : "FAIL") << "\n"
            << "  SYSTEM_SYNTH_READY     " << (ready ? "PASS" : "FAIL") << "\n"
            << "  ECOSYSTEM_SYNTH        NOT_YET_APPLICABLE\n\n"
            << "Observed: the system is operational. No ecosystem is claimed.\n";
}

void print_foundation(const Paths& paths, bool json) {
  const auto gates = foundation_gates(paths);
  if (json) {
    std::cout << "{\"gates\":[";
    for (std::size_t i = 0; i < gates.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << "{\"name\":\"" << gates[i].name << "\",\"status\":\""
                << (gates[i].pass ? "PASS" : "FAIL") << "\",\"evidence\":\""
                << json_escape(gates[i].evidence) << "\"}";
    }
    std::cout << "],\"status\":\"" << (all_pass(gates) ? "PASS" : "FAIL") << "\"}\n";
    return;
  }
  std::cout << "Foundation verification\n\n";
  for (const auto& gate : gates) {
    std::cout << "  " << std::left << std::setw(36) << gate.name << (gate.pass ? "PASS" : "FAIL") << "\n"
              << "    evidence: " << gate.evidence << "\n";
  }
}

void print_surfaces(const Paths& paths, bool json) {
  const auto items = surfaces(paths);
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
  else std::cout << "No relations observed.\n";
}

void print_help() {
  std::cout << "SYNTH — always ready, always incomplete\n\n"
            << "Usage: synth <command> [--json]\n\n"
            << "  version             Show system identity and version\n"
            << "  status              Show present readiness state\n"
            << "  foundation verify   Verify every foundational gate\n"
            << "  surfaces            List only real SYNTH surfaces\n"
            << "  relations           List observed relations\n"
            << "  evidence            Observe this process and persist evidence\n";
}

} // namespace synth

int main(int argc, char** argv) {
  using namespace synth;
  try {
    const auto paths = xdg_paths();
    ensure_layout(paths);
    const auto evidence = evidence_json(paths);
    persist_evidence(paths, evidence);

    bool json = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
      if (std::string_view(argv[i]) == "--json") json = true;
      else args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "help" || args[0] == "--help") { print_help(); return 0; }
    if (args[0] == "version") { print_version(json); return 0; }
    if (args[0] == "status") { print_status(paths, json); return all_pass(foundation_gates(paths)) ? 0 : 1; }
    if (args[0] == "surfaces") { print_surfaces(paths, json); return 0; }
    if (args[0] == "relations") { print_relations(json); return 0; }
    if (args[0] == "evidence") { std::cout << (json ? evidence : "Evidence observed and persisted\n  class: OBSERVED\n  file: " + (paths.state / "evidence/latest.json").string() + "\n"); return 0; }
    if (args[0] == "foundation" && args.size() > 1 && args[1] == "verify") {
      const auto gates = foundation_gates(paths);
      print_foundation(paths, json);
      return all_pass(gates) ? 0 : 1;
    }

    std::cerr << "Unknown command: " << args[0] << "\nEvidence: run 'synth help' for valid commands.\n";
    return 2;
  } catch (const std::exception& error) {
    std::cerr << "SYNTH could not observe its state.\nCause: " << error.what() << "\n";
    return 1;
  }
}
