#include "realization.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include <openssl/evp.h>

namespace synth::realization {
namespace {

constexpr std::string_view manifest_schema = "urn:synth:schema:artifact-manifest:0.1.0";
constexpr std::string_view witness_schema = "urn:synth:schema:realization-witness:0.1.0";
constexpr std::string_view human_web_surface = "interface.human.web.v1";

void validate_semantic_surface_contract(const json& surface) {
  if (surface.value("id", "") != human_web_surface) return;
  if (surface.value("kind", "") != "http" || surface.value("media_type", "") != "text/html") {
    throw std::runtime_error("interface.human.web.v1 requires kind=http and media_type=text/html");
  }
}

class OperationFailure : public std::runtime_error {
 public:
  OperationFailure(std::string status, std::string message)
      : std::runtime_error(std::move(message)), status_(std::move(status)) {}
  const std::string& status() const { return status_; }

 private:
  std::string status_;
};

struct CommandResult {
  int exit_code{};
  std::string output;
};

struct ManifestRecord {
  json manifest;
};

std::string timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto value = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  gmtime_r(&value, &utc);
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

std::string operation_id(std::string_view prefix) {
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "-" + std::to_string(::getpid()) + "-" + std::to_string(ticks);
}

fs::path current_executable() {
  std::array<char, 4096> buffer{};
  const auto size = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (size < 0) throw std::runtime_error("cannot resolve current SYNTH executable");
  return fs::path(std::string(buffer.data(), static_cast<std::size_t>(size)));
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
  try {
    return json::parse(input);
  } catch (const json::exception& error) {
    throw std::runtime_error("invalid JSON in " + path.string() + ": " + error.what());
  }
}

void write_json_atomic(const fs::path& path, const json& value) {
  ensure_directory(path.parent_path());
  const auto temporary = path.string() + ".tmp-" + std::to_string(::getpid());
  {
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write JSON: " + temporary);
    output << value.dump(2) << '\n';
    if (!output) throw std::runtime_error("cannot persist JSON: " + temporary);
  }
  fs::rename(temporary, path);
}

bool path_within(const fs::path& root, const fs::path& candidate) {
  const auto normalized_root = fs::weakly_canonical(root);
  const auto normalized_candidate = fs::weakly_canonical(candidate);
  auto root_part = normalized_root.begin();
  auto candidate_part = normalized_candidate.begin();
  for (; root_part != normalized_root.end(); ++root_part, ++candidate_part) {
    if (candidate_part == normalized_candidate.end() || *candidate_part != *root_part) return false;
  }
  return true;
}

CommandResult run_capture(const std::vector<std::string>& arguments) {
  if (arguments.empty()) throw std::runtime_error("cannot run an empty command");
  int output_pipe[2]{};
  if (::pipe2(output_pipe, O_CLOEXEC) != 0) throw std::runtime_error("cannot create process pipe");
  const pid_t child = ::fork();
  if (child < 0) {
    ::close(output_pipe[0]);
    ::close(output_pipe[1]);
    throw std::runtime_error("cannot fork process");
  }
  if (child == 0) {
    ::dup2(output_pipe[1], STDOUT_FILENO);
    ::dup2(output_pipe[1], STDERR_FILENO);
    ::close(output_pipe[0]);
    ::close(output_pipe[1]);
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
    _exit(127);
  }
  ::close(output_pipe[1]);
  std::string output;
  std::array<char, 4096> buffer{};
  while (true) {
    const auto count = ::read(output_pipe[0], buffer.data(), buffer.size());
    if (count > 0) output.append(buffer.data(), static_cast<std::size_t>(count));
    else if (count == 0) break;
    else if (errno != EINTR) break;
  }
  ::close(output_pipe[0]);
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
  const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  return {exit_code, std::move(output)};
}

std::string sha256(const fs::path& path) {
  using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
    throw std::runtime_error("OpenSSL could not initialize SHA-256");
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot acquire artifact: " + path.string());
  std::array<char, 64 * 1024> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (input.gcount() > 0 && EVP_DigestUpdate(context.get(), buffer.data(), static_cast<std::size_t>(input.gcount())) != 1) {
      throw std::runtime_error("OpenSSL could not update SHA-256");
    }
  }
  if (!input.eof()) throw std::runtime_error("cannot read artifact: " + path.string());
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1) {
    throw std::runtime_error("OpenSSL could not finalize SHA-256");
  }
  std::ostringstream rendered;
  rendered << std::hex << std::setfill('0');
  for (unsigned int index = 0; index < size; ++index) rendered << std::setw(2) << static_cast<unsigned int>(digest[index]);
  return rendered.str();
}

bool valid_relative_path(std::string_view value) {
  const fs::path path(value);
  if (value.empty() || path.is_absolute()) return false;
  return std::none_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; });
}

bool valid_identity(std::string_view value) {
  if (value.empty() || value.front() < 'a' || value.front() > 'z') return false;
  return std::all_of(value.begin(), value.end(), [](const char character) {
    return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
           character == '.' || character == '-';
  });
}

bool valid_sha256(std::string_view value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
  });
}

bool non_empty_string(const json& value) {
  return value.is_string() && !value.get_ref<const std::string&>().empty();
}

bool string_value(const json& value) {
  return value.is_string();
}

void require_object_shape(const json& value, const std::set<std::string>& required,
                          const std::set<std::string>& optional = {}) {
  if (!value.is_object()) throw std::runtime_error("expected a JSON object");
  for (const auto& field : required) {
    if (!value.contains(field)) throw std::runtime_error("required field is absent: " + field);
  }
  for (const auto& [field, ignored] : value.items()) {
    (void)ignored;
    if (!required.contains(field) && !optional.contains(field)) {
      throw std::runtime_error("unexpected field: " + field);
    }
  }
}

void require_string_array(const json& value, std::string_view field) {
  if (!value.is_array() || !std::all_of(value.begin(), value.end(), string_value)) {
    throw std::runtime_error(std::string(field) + " must be an array of strings");
  }
}

bool valid_observed_at(std::string_view value) {
  if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
      value[13] != ':' || value[16] != ':' || value[19] != 'Z') return false;
  for (const auto index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U, 11U, 12U, 14U, 15U, 17U, 18U}) {
    if (value[index] < '0' || value[index] > '9') return false;
  }
  const auto number = [&value](const std::size_t offset, const std::size_t size) {
    return std::stoi(std::string(value.substr(offset, size)));
  };
  const auto month = number(5, 2);
  const auto day = number(8, 2);
  const auto hour = number(11, 2);
  const auto minute = number(14, 2);
  const auto second = number(17, 2);
  const auto year = number(0, 4);
  if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 59) return false;
  std::array<int, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) days[1] = 29;
  return day >= 1 && day <= days[static_cast<std::size_t>(month - 1)];
}

void validate_surface_declaration(const json& surface) {
  require_object_shape(surface, {"id"}, {"kind", "media_type"});
  if (!non_empty_string(surface.at("id"))) throw std::runtime_error("surface declaration requires a non-empty id");
  for (const auto* field : {"kind", "media_type"}) {
    if (surface.contains(field) && !non_empty_string(surface.at(field))) {
      throw std::runtime_error(std::string("surface declaration has invalid ") + field);
    }
  }
  validate_semantic_surface_contract(surface);
}

void validate_manifest(const json& manifest) {
  try {
    require_object_shape(manifest, {
      "$schema", "schema_version", "identity", "version", "description", "artifact", "provenance",
      "provides", "requires", "realization", "readiness", "resources"
    });
    if (!manifest.is_object() || manifest.value("$schema", "") != manifest_schema ||
        manifest.value("schema_version", "") != "0.1.0" || !valid_identity(manifest.value("identity", "")) ||
        manifest.value("version", "").empty() || manifest.value("description", "").empty()) {
      throw std::runtime_error("manifest identity, version or schema is invalid");
    }
    const auto& artifact = manifest.at("artifact");
    require_object_shape(artifact, {"path", "media_type", "sha256"});
    if (artifact.value("media_type", "") != "application/x-tar" ||
        !valid_relative_path(artifact.value("path", "")) || !valid_sha256(artifact.value("sha256", ""))) {
      throw std::runtime_error("artifact path, media type or SHA-256 is invalid");
    }
    const auto& provenance = manifest.at("provenance");
    require_object_shape(provenance, {"source", "revision", "license"});
    if (!non_empty_string(provenance.at("source")) || !non_empty_string(provenance.at("revision")) ||
        !non_empty_string(provenance.at("license"))) {
      throw std::runtime_error("artifact provenance is invalid");
    }
    std::set<std::string> surface_ids;
    for (const auto* set_name : {"provides", "requires"}) {
      const auto& surface_set = manifest.at(set_name);
      require_object_shape(surface_set, {"surfaces"});
      if (!surface_set.at("surfaces").is_array()) throw std::runtime_error("surface set must be an array");
      surface_ids.clear();
      for (const auto& surface : surface_set.at("surfaces")) {
        validate_surface_declaration(surface);
        if (!surface_ids.insert(surface.at("id").get<std::string>()).second) {
          throw std::runtime_error(std::string(set_name) + " contains a duplicate surface id");
        }
      }
    }
    const auto& realization = manifest.at("realization");
    require_object_shape(realization, {"entrypoint", "arguments", "environment_allowed"});
    if (!valid_relative_path(realization.value("entrypoint", "")) || !realization.at("arguments").is_array() ||
        !realization.at("environment_allowed").is_array()) throw std::runtime_error("realization declaration is invalid");
    const std::set<std::string> supported_environment{
      "SYNTH_REALIZATION_ID", "SYNTH_WITNESS_PATH", "SYNTH_RESOLVED_SURFACES_PATH",
      "SYNTH_ECOSYSTEM_STREAM"
    };
    require_string_array(realization.at("arguments"), "realization.arguments");
    std::set<std::string> requested_environment;
    for (const auto& name : realization.at("environment_allowed")) {
      if (!name.is_string() || !supported_environment.contains(name.get<std::string>())) {
        throw std::runtime_error("realization requests an unsupported environment variable");
      }
      if (!requested_environment.insert(name.get<std::string>()).second) {
        throw std::runtime_error("realization requests a duplicate environment variable");
      }
    }
    const auto& readiness = manifest.at("readiness");
    require_object_shape(readiness, {"type", "entrypoint", "arguments", "timeout_ms"});
    if (readiness.value("type", "") != "command" || !valid_relative_path(readiness.value("entrypoint", "")) ||
        !readiness.at("arguments").is_array() || !readiness.at("timeout_ms").is_number_integer() ||
        readiness.at("timeout_ms").get<int>() < 100 || readiness.at("timeout_ms").get<int>() > 60000) {
      throw std::runtime_error("readiness declaration is invalid");
    }
    require_string_array(readiness.at("arguments"), "readiness.arguments");
    const auto& resources = manifest.at("resources");
    require_object_shape(resources, {}, {"activation_rss_mib_max"});
    if (resources.contains("activation_rss_mib_max") &&
        (!resources.at("activation_rss_mib_max").is_number_integer() || resources.at("activation_rss_mib_max").get<long>() < 1)) {
      throw std::runtime_error("activation RSS admission threshold is invalid");
    }
  } catch (const json::exception& error) {
    throw std::runtime_error("manifest does not conform to the required model: " + std::string(error.what()));
  }
}

fs::path source_path(std::string value) {
  constexpr std::string_view file_prefix = "file://";
  if (value.starts_with(file_prefix)) value.erase(0, file_prefix.size());
  const fs::path path(value);
  if (!fs::is_directory(path / "manifests")) throw std::runtime_error("local source has no manifests directory: " + path.string());
  return fs::canonical(path);
}

class ArtifactSource {
 public:
  virtual ~ArtifactSource() = default;
  virtual std::vector<ManifestRecord> manifests() const = 0;
  virtual fs::path acquire(const ManifestRecord& record) const = 0;
  virtual std::string locator() const = 0;
};

class LocalFileSource final : public ArtifactSource {
 public:
  explicit LocalFileSource(std::string location) : root_(source_path(std::move(location))) {}

  std::vector<ManifestRecord> manifests() const override {
    std::vector<ManifestRecord> records;
    for (const auto& entry : fs::directory_iterator(root_ / "manifests")) {
      if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
      auto manifest = read_json(entry.path());
      try {
        validate_manifest(manifest);
      } catch (const std::exception& error) {
        throw std::runtime_error("manifest does not conform to the required model: " + std::string(error.what()));
      }
      records.push_back({std::move(manifest)});
    }
    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
      return left.manifest.value("identity", "") < right.manifest.value("identity", "");
    });
    return records;
  }

  fs::path acquire(const ManifestRecord& record) const override {
    const auto candidate = root_ / record.manifest.at("artifact").at("path").get<std::string>();
    if (!fs::is_regular_file(candidate) || !path_within(root_, candidate)) {
      throw std::runtime_error("artifact escapes or is absent from local source");
    }
    return fs::canonical(candidate);
  }

  std::string locator() const override { return root_.string(); }

 private:
  fs::path root_;
};

std::unique_ptr<ArtifactSource> open_source(std::string location) {
  return std::make_unique<LocalFileSource>(std::move(location));
}

ManifestRecord resolve(const ArtifactSource& source, std::string_view identity) {
  std::optional<ManifestRecord> match;
  for (auto& record : source.manifests()) {
    if (record.manifest.value("identity", "") != identity) continue;
    if (match) throw std::runtime_error("source contains multiple manifests for identity: " + std::string(identity));
    match = std::move(record);
  }
  if (!match) throw std::runtime_error("identity not found in local source: " + std::string(identity));
  return std::move(*match);
}

std::optional<std::string> option_value(const std::vector<std::string>& args, std::string_view option) {
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (args[index] == option) {
      if (index + 1 >= args.size()) throw std::runtime_error("missing value for " + std::string(option));
      return args[index + 1];
    }
  }
  return std::nullopt;
}

void record_transaction(const Roots& roots, std::string_view operation, std::string_view identity,
                        std::string_view status, const json& evidence = json::object()) {
  const auto id = operation_id(operation);
  write_json_atomic(roots.state / "transactions" / (id + ".json"), {
    {"transaction_id", id}, {"operation", operation}, {"identity", identity},
    {"status", status}, {"observed_at", timestamp()}, {"evidence", evidence}
  });
}

void validate_tar(const fs::path& artifact) {
  const auto names = run_capture({"tar", "--list", "--file", artifact.string()});
  if (names.exit_code != 0) throw std::runtime_error("artifact is not a readable tar archive: " + names.output);
  std::istringstream name_lines(names.output);
  std::string member;
  while (std::getline(name_lines, member)) {
    if (!member.empty() && member.back() == '\r') member.pop_back();
    if (!valid_relative_path(member)) throw std::runtime_error("artifact contains an unsafe path: " + member);
  }
  const auto verbose = run_capture({"tar", "--list", "--verbose", "--file", artifact.string()});
  if (verbose.exit_code != 0) throw std::runtime_error("artifact members cannot be inspected: " + verbose.output);
  std::istringstream verbose_lines(verbose.output);
  while (std::getline(verbose_lines, member)) {
    if (!member.empty() && member.front() != '-' && member.front() != 'd') {
      throw std::runtime_error("artifact links and special files are not allowed");
    }
  }
}

void make_store_read_only(const fs::path& root) {
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    fs::permissions(entry.path(), fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
  }
  fs::permissions(root, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove);
}

struct InstallResult {
  json installation;
  bool no_op{};
};

InstallResult install(const ManifestRecord& record, const ArtifactSource& source, const Roots& roots) {
  const auto identity = record.manifest.at("identity").get<std::string>();
  const auto version = record.manifest.at("version").get<std::string>();
  const auto expected_digest = record.manifest.at("artifact").at("sha256").get<std::string>();
  const auto installation_path = roots.state / "installations" / (identity + ".json");
  if (fs::is_regular_file(installation_path)) {
    const auto existing = read_json(installation_path);
    if (existing.value("version", "") == version && existing.value("digest", "") == expected_digest) return {existing, true};
    throw std::runtime_error("identity is already installed with different content; upgrade is not implemented");
  }

  const auto store_root = roots.data / "store";
  ensure_directory(store_root);
  const auto destination = store_root / expected_digest;
  if (!fs::exists(destination)) {
    const auto stage = store_root / (".stage-" + operation_id("install"));
    try {
      ensure_directory(stage);
      const auto artifact_snapshot = stage / "artifact.tar";
      const auto artifact = source.acquire(record);
      if (!fs::copy_file(artifact, artifact_snapshot, fs::copy_options::none)) {
        throw std::runtime_error("artifact acquisition did not create a stable snapshot");
      }
      const auto actual_digest = sha256(artifact_snapshot);
      if (actual_digest != expected_digest) {
        record_transaction(roots, "install", identity, "REJECTED", {{"reason", "SHA-256 mismatch"}, {"expected", expected_digest}, {"observed", actual_digest}});
        throw std::runtime_error("artifact digest mismatch; candidate REJECTED and active state unchanged");
      }
      validate_tar(artifact_snapshot);
      ensure_directory(stage / "payload");
      const auto extraction = run_capture({"tar", "--extract", "--file", artifact_snapshot.string(), "--directory", (stage / "payload").string(),
                                           "--no-same-owner", "--no-same-permissions"});
      if (extraction.exit_code != 0) throw std::runtime_error("artifact extraction failed: " + extraction.output);
      write_json_atomic(stage / "manifest.json", record.manifest);
      make_store_read_only(stage);
      fs::rename(stage, destination);
    } catch (...) {
      std::error_code ignored;
      fs::permissions(stage, fs::perms::owner_write, fs::perm_options::add, ignored);
      fs::remove_all(stage, ignored);
      throw;
    }
  } else {
    const auto artifact_snapshot = destination / "artifact.tar";
    if (!fs::is_regular_file(artifact_snapshot) || sha256(artifact_snapshot) != expected_digest) {
      throw std::runtime_error("existing digest store entry cannot be verified");
    }
  }

  const json installation{
    {"identity", identity}, {"version", version}, {"digest", expected_digest},
    {"store_path", destination.string()}, {"manifest", record.manifest},
    {"source", source.locator()}, {"installed_at", timestamp()}, {"status", "INSTALLED"}
  };
  write_json_atomic(installation_path, installation);
  record_transaction(roots, "install", identity, "INSTALLED", {{"digest", expected_digest}, {"active_state", "unchanged"}});
  return {installation, false};
}

json installed_records(const Roots& roots) {
  json records = json::array();
  const auto directory = roots.state / "installations";
  if (!fs::is_directory(directory)) return records;
  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(directory)) if (entry.is_regular_file() && entry.path().extension() == ".json") paths.push_back(entry.path());
  std::sort(paths.begin(), paths.end());
  for (const auto& path : paths) records.push_back(read_json(path));
  return records;
}

fs::path active_record_path(const Roots& roots, std::string_view identity) {
  return roots.state / "realizations" / std::string(identity) / "active.json";
}

bool process_alive(const pid_t pid) {
  return pid > 0 && (::kill(pid, 0) == 0 || errno == EPERM);
}

std::optional<unsigned long long> process_start_ticks(const pid_t pid) {
  std::ifstream input("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  if (!std::getline(input, line)) return std::nullopt;
  const auto command_end = line.rfind(')');
  if (command_end == std::string::npos || command_end + 2 >= line.size()) return std::nullopt;
  std::istringstream fields(line.substr(command_end + 2));
  std::string field;
  for (int index = 0; index <= 19; ++index) {
    if (!(fields >> field)) return std::nullopt;
  }
  try {
    return std::stoull(field);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool active_process_matches(const json& active) {
  const auto pid = active.value("pid", 0);
  const auto observed_start = process_start_ticks(pid);
  return process_alive(pid) && observed_start && *observed_start == active.value("process_start_ticks", 0ULL);
}

std::optional<json> evidence_surface(const Roots& roots) {
  const auto path = roots.state / "evidence/latest.json";
  if (!fs::is_regular_file(path)) return std::nullopt;
  try {
    const auto evidence = read_json(path);
    if (evidence.value("identity", "") != "SYNTH" || evidence.value("epistemic_class", "") != "OBSERVED") return std::nullopt;
    const auto& surfaces = evidence.at("observed_surfaces");
    const auto observed = std::find_if(surfaces.begin(), surfaces.end(), [](const auto& surface) {
      return surface.value("id", "") == "synth.evidence";
    });
    if (observed == surfaces.end()) return std::nullopt;
    return std::optional<json>{json{
      {"id", "synth.evidence"}, {"owner", "SYNTH"}, {"kind", "file"},
      {"locator", path.string()}, {"direction", "outbound"}, {"media_type", "application/json"},
      {"observability", "self-observed"}, {"metadata", "validated public evidence document"}
    }};
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

json available_surfaces(const Roots& roots) {
  json surfaces = active_surfaces(roots);
  if (const auto evidence = evidence_surface(roots)) surfaces.push_back(*evidence);
  return surfaces;
}

bool compatible_surface(const json& requirement, const json& surface) {
  if (surface.value("id", "") != requirement.value("id", "")) return false;
  for (const auto* property : {"kind", "media_type"}) {
    if (requirement.contains(property) && surface.value(property, "") != requirement.at(property).get<std::string>()) {
      return false;
    }
  }
  return true;
}

json pin_file_surface(json surface, const Roots& roots, std::string_view identity,
                      std::string_view realization_id, const std::size_t index) {
  if (surface.value("kind", "") != "file") {
    throw OperationFailure("BLOCKED", "v0.1.0 can only pin file requirements: " + surface.value("id", ""));
  }
  const fs::path source = surface.at("locator").get<std::string>();
  if (!fs::is_regular_file(source)) {
    throw OperationFailure("BLOCKED", "resolved file surface is no longer available: " + surface.value("id", ""));
  }
  const auto snapshot_root = roots.state / "realizations" / std::string(identity) / "resolved" / std::string(realization_id);
  ensure_directory(snapshot_root);
  const auto snapshot = snapshot_root / (std::to_string(index) + ".snapshot");
  const auto temporary = snapshot.string() + ".tmp-" + std::to_string(::getpid());
  if (!fs::copy_file(source, temporary, fs::copy_options::none)) {
    throw std::runtime_error("cannot snapshot resolved surface: " + source.string());
  }
  fs::rename(temporary, snapshot);
  fs::permissions(snapshot, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write,
                  fs::perm_options::remove);
  surface["source_locator"] = source.string();
  surface["locator"] = snapshot.string();
  surface["expected_sha256"] = sha256(snapshot);
  surface["resolution"] = "RESOLVED";
  return surface;
}

json resolve_requirements(const json& manifest, const Roots& roots, std::string_view realization_id) {
  const auto available = available_surfaces(roots);
  json resolved = json::array();
  const auto identity = manifest.at("identity").get<std::string>();
  const auto snapshot_root = roots.state / "realizations" / identity / "resolved" / std::string(realization_id);
  try {
    std::size_t index = 0;
    for (const auto& requirement : manifest.at("requires").at("surfaces")) {
      const auto id = requirement.at("id").get<std::string>();
      std::vector<json> matches;
      std::copy_if(available.begin(), available.end(), std::back_inserter(matches), [&requirement](const auto& surface) {
        return compatible_surface(requirement, surface);
      });
      if (matches.empty()) {
        throw OperationFailure("BLOCKED", "required compatible surface is not observed: " + id +
          "; produce or activate that public surface before retrying");
      }
      if (matches.size() != 1) {
        throw OperationFailure("BLOCKED", "required surface is ambiguous: " + id);
      }
      resolved.push_back(pin_file_surface(std::move(matches.front()), roots, identity, realization_id, index++));
    }
    return resolved;
  } catch (...) {
    std::error_code ignored;
    fs::remove_all(snapshot_root, ignored);
    throw;
  }
}

fs::path payload_entry(const json& installation, std::string_view relative) {
  if (!valid_relative_path(relative)) throw std::runtime_error("artifact entrypoint is not a safe relative path");
  const auto payload = fs::path(installation.at("store_path").get<std::string>()) / "payload";
  const auto candidate = payload / relative;
  if (!path_within(payload, candidate) || !fs::is_regular_file(candidate) || ::access(candidate.c_str(), X_OK) != 0) {
    throw std::runtime_error("artifact entrypoint is absent or not executable: " + candidate.string());
  }
  return fs::canonical(candidate);
}

std::vector<std::string> candidate_environment(const json& manifest, std::string_view realization_id,
                                                const fs::path& witness_path, const fs::path& resolved_path,
                                                const Roots& roots) {
  const json ecosystem_stream{
    {"$schema", "urn:synth:capability:ecosystem-stream:0.1.0"},
    {"surface", "synth.ecosystem.stream.v1"},
    {"media_type", "application/x-ndjson"},
    {"argv", json::array({current_executable().string(), "ecosystem", "--watch", "--json"})},
    {"environment", {
      {"XDG_DATA_HOME", roots.data.parent_path().string()},
      {"XDG_STATE_HOME", roots.state.parent_path().string()},
      {"XDG_RUNTIME_DIR", roots.runtime.parent_path().string()},
      {"SYNTH_DATA_DIR", roots.resources.string()}
    }}
  };
  const std::map<std::string, std::string> available{
    {"SYNTH_REALIZATION_ID", std::string(realization_id)},
    {"SYNTH_WITNESS_PATH", witness_path.string()},
    {"SYNTH_RESOLVED_SURFACES_PATH", resolved_path.string()},
    {"SYNTH_ECOSYSTEM_STREAM", ecosystem_stream.dump()}
  };
  std::vector<std::string> environment{"PATH=/usr/bin:/bin", "LANG=C.UTF-8", "PYTHONDONTWRITEBYTECODE=1"};
  for (const auto& declared : manifest.at("realization").at("environment_allowed")) {
    const auto name = declared.get<std::string>();
    environment.push_back(name + "=" + available.at(name));
  }
  return environment;
}

std::vector<char*> process_vector(std::vector<std::string>& values) {
  std::vector<char*> result;
  result.reserve(values.size() + 1);
  for (auto& value : values) result.push_back(value.data());
  result.push_back(nullptr);
  return result;
}

pid_t spawn_candidate(const fs::path& executable, const std::vector<std::string>& declared_arguments,
                      const fs::path& working_directory, std::vector<std::string> environment,
                      const fs::path& log_path) {
  const pid_t child = ::fork();
  if (child < 0) throw std::runtime_error("cannot fork candidate process");
  if (child == 0) {
    if (::setsid() < 0 || ::chdir(working_directory.c_str()) != 0) _exit(126);
    const int log = ::open(log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (log < 0) _exit(126);
    ::dup2(log, STDOUT_FILENO);
    ::dup2(log, STDERR_FILENO);
    ::close(log);
    std::vector<std::string> arguments{executable.string()};
    arguments.insert(arguments.end(), declared_arguments.begin(), declared_arguments.end());
    auto argv = process_vector(arguments);
    auto envp = process_vector(environment);
    ::execve(executable.c_str(), argv.data(), envp.data());
    _exit(127);
  }
  return child;
}

void stop_readiness_command(const pid_t pid) {
  if (pid <= 0) return;
  ::kill(-pid, SIGTERM);
  ::kill(pid, SIGTERM);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
  int status = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto observed = ::waitpid(pid, &status, WNOHANG);
    if (observed == pid || (observed < 0 && errno == ECHILD)) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ::kill(-pid, SIGKILL);
  ::kill(pid, SIGKILL);
  while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
}

int run_candidate_command(const fs::path& executable, const std::vector<std::string>& declared_arguments,
                          const fs::path& working_directory, std::vector<std::string> environment,
                          const fs::path& log_path, const std::chrono::steady_clock::time_point deadline) {
  if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("candidate readiness timed out");
  const pid_t child = ::fork();
  if (child < 0) throw std::runtime_error("cannot fork readiness command");
  if (child == 0) {
    if (::setsid() < 0 || ::chdir(working_directory.c_str()) != 0) _exit(126);
    const int log = ::open(log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0600);
    if (log < 0) _exit(126);
    ::dup2(log, STDOUT_FILENO);
    ::dup2(log, STDERR_FILENO);
    ::close(log);
    std::vector<std::string> arguments{executable.string()};
    arguments.insert(arguments.end(), declared_arguments.begin(), declared_arguments.end());
    auto argv = process_vector(arguments);
    auto envp = process_vector(environment);
    ::execve(executable.c_str(), argv.data(), envp.data());
    _exit(127);
  }
  while (true) {
    if (std::chrono::steady_clock::now() >= deadline) {
      stop_readiness_command(child);
      throw std::runtime_error("candidate readiness timed out");
    }
    int status = 0;
    const auto observed = ::waitpid(child, &status, WNOHANG);
    if (observed == child) return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (observed < 0 && errno != EINTR) {
      stop_readiness_command(child);
      throw std::runtime_error("cannot observe readiness command");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void stop_process(const pid_t pid) {
  if (pid <= 0) return;
  ::kill(-pid, SIGTERM);
  ::kill(pid, SIGTERM);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (process_alive(pid) && std::chrono::steady_clock::now() < deadline) {
    int status = 0;
    ::waitpid(pid, &status, WNOHANG);
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  if (process_alive(pid)) {
    ::kill(-pid, SIGKILL);
    ::kill(pid, SIGKILL);
  }
  int status = 0;
  while (::waitpid(pid, &status, WNOHANG) < 0 && errno == EINTR) {}
}

long process_rss_kib(const pid_t pid) {
  std::ifstream status("/proc/" + std::to_string(pid) + "/status");
  std::string key;
  while (status >> key) {
    if (key == "VmRSS:") {
      long value = 0;
      status >> value;
      return value;
    }
    std::string ignored;
    std::getline(status, ignored);
  }
  return 0;
}

void validate_witness_schema_document(const Roots& roots) {
  const auto schema = read_json(roots.resources / "schemas/realization-witness.schema.json");
  if (schema.value("$schema", "") != "https://json-schema.org/draft/2020-12/schema" ||
      schema.value("$id", "") != witness_schema || schema.value("type", "") != "object") {
    throw std::runtime_error("realization witness schema is invalid");
  }
}

void validate_observed_surface(const json& surface) {
  require_object_shape(surface, {"id", "owner", "kind", "locator", "direction", "media_type", "observability", "metadata"});
  for (const auto* field : {"id", "owner", "kind", "locator", "direction", "media_type", "observability"}) {
    if (!non_empty_string(surface.at(field))) throw std::runtime_error(std::string("provided surface has invalid ") + field);
  }
  if (!non_empty_string(surface.at("metadata"))) throw std::runtime_error("provided surface metadata must be a non-empty string");
  const auto direction = surface.at("direction").get<std::string>();
  if (direction != "inbound" && direction != "outbound" && direction != "bidirectional") {
    throw std::runtime_error("provided surface direction is invalid");
  }
  if (surface.at("observability") != "runtime-witness") {
    throw std::runtime_error("provided surface observability is invalid");
  }
  validate_semantic_surface_contract(surface);
}

void validate_consumed_surface(const json& surface) {
  require_object_shape(surface, {"id", "locator", "observed_at", "evidence_sha256"});
  if (!non_empty_string(surface.at("id")) || !non_empty_string(surface.at("locator")) ||
      !surface.at("observed_at").is_string() || !valid_observed_at(surface.at("observed_at").get<std::string>()) ||
      !surface.at("evidence_sha256").is_string() || !valid_sha256(surface.at("evidence_sha256").get<std::string>())) {
    throw std::runtime_error("consumed surface is invalid");
  }
}

json validate_witness(const fs::path& witness_path, const json& installation, std::string_view realization_id,
                      const pid_t pid, const json& resolved, const Roots& roots) {
  validate_witness_schema_document(roots);
  const auto witness = read_json(witness_path);
  const auto& manifest = installation.at("manifest");
  try {
    require_object_shape(witness, {
      "$schema", "schema_version", "identity", "version", "realization_id", "process", "provided_surfaces",
      "consumed_surfaces", "readiness", "observed_at"
    });
    require_object_shape(witness.at("process"), {"pid"});
    require_object_shape(witness.at("readiness"), {"status", "mechanism", "checked_at"});
    if (witness.value("$schema", "") != witness_schema || witness.value("schema_version", "") != "0.1.0" ||
        witness.value("identity", "") != manifest.at("identity").get<std::string>() ||
        witness.value("version", "") != manifest.at("version").get<std::string>() ||
        witness.value("realization_id", "") != realization_id || !witness.at("process").at("pid").is_number_integer() ||
        witness.at("process").at("pid").get<pid_t>() != pid || witness.at("readiness").value("status", "") != "READY" ||
        !non_empty_string(witness.at("readiness").at("mechanism")) ||
        !witness.at("readiness").at("checked_at").is_string() ||
        !valid_observed_at(witness.at("readiness").at("checked_at").get<std::string>()) ||
        !witness.at("observed_at").is_string() || !valid_observed_at(witness.at("observed_at").get<std::string>())) {
      throw std::runtime_error("witness identity, process or readiness does not match the candidate");
    }
    const auto& provided = witness.at("provided_surfaces");
    const auto& consumed = witness.at("consumed_surfaces");
    if (!provided.is_array() || !consumed.is_array()) throw std::runtime_error("witness surfaces are not arrays");

    std::set<std::string> provided_ids;
    for (const auto& surface : provided) {
      validate_observed_surface(surface);
      if (!provided_ids.insert(surface.at("id").get<std::string>()).second) {
        throw std::runtime_error("witness contains a duplicate provided surface");
      }
    }
    std::set<std::string> consumed_ids;
    for (const auto& surface : consumed) {
      validate_consumed_surface(surface);
      if (!consumed_ids.insert(surface.at("id").get<std::string>()).second) {
        throw std::runtime_error("witness contains a duplicate consumed surface");
      }
    }

    if (provided.size() != manifest.at("provides").at("surfaces").size()) {
      throw std::runtime_error("witness provided surface set differs from the manifest");
    }

    for (const auto& declaration : manifest.at("provides").at("surfaces")) {
      const auto id = declaration.at("id").get<std::string>();
      const auto match = std::find_if(provided.begin(), provided.end(), [&id, &declaration, &manifest](const auto& surface) {
        if (surface.value("id", "") != id || surface.value("owner", "") != manifest.at("identity").get<std::string>()) return false;
        for (const auto* property : {"kind", "media_type"}) {
          if (declaration.contains(property) &&
              surface.value(property, "") != declaration.at(property).template get<std::string>()) return false;
        }
        return true;
      });
      if (match == provided.end()) throw std::runtime_error("witness does not observe declared provided surface: " + id);
    }
    if (consumed.size() != resolved.size()) throw std::runtime_error("witness consumed surface set differs from resolution");
    for (const auto& resolution : resolved) {
      const auto id = resolution.at("id").get<std::string>();
      const auto locator = resolution.at("locator").get<std::string>();
      const auto expected_digest = resolution.at("expected_sha256").get<std::string>();
      if (!valid_sha256(expected_digest) || !fs::is_regular_file(locator) || sha256(locator) != expected_digest) {
        throw std::runtime_error("resolved evidence snapshot is not intact: " + id);
      }
      const auto match = std::find_if(consumed.begin(), consumed.end(), [&id, &locator, &expected_digest](const auto& surface) {
        return surface.value("id", "") == id && surface.value("locator", "") == locator &&
               surface.value("evidence_sha256", "") == expected_digest;
      });
      if (match == consumed.end()) throw std::runtime_error("witness attestation does not match resolved evidence: " + id);
    }
  } catch (const json::exception& error) {
    throw std::runtime_error("witness does not conform to the required model: " + std::string(error.what()));
  }
  return witness;
}

json active_records(const Roots& roots) {
  json records = json::array();
  const auto directory = roots.state / "realizations";
  if (!fs::is_directory(directory)) return records;
  for (const auto& participant : fs::directory_iterator(directory)) {
    const auto active = participant.path() / "active.json";
    if (!fs::is_regular_file(active)) continue;
    try {
      const auto record = read_json(active);
      if (active_process_matches(record)) records.push_back(record);
    } catch (const std::exception&) {
    }
  }
  return records;
}

int activate_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 2) throw std::runtime_error("usage: synth activate <identity>");
  const auto identity = args[1];
  if (!valid_identity(identity)) throw std::runtime_error("invalid realization identity");
  const auto installation_path = roots.state / "installations" / (identity + ".json");
  if (!fs::is_regular_file(installation_path)) throw OperationFailure("BLOCKED", "identity is not installed: " + identity);
  const auto active_path = active_record_path(roots, identity);
  if (fs::is_regular_file(active_path)) {
    const auto active = read_json(active_path);
    if (active_process_matches(active)) {
      if (as_json) std::cout << json{{"status", "NO_OP"}, {"identity", identity}, {"reason", "already active"}}.dump() << '\n';
      else std::cout << identity << " is already active.\nNo changes required.\n";
      return 0;
    }
    throw OperationFailure("BLOCKED", "a stale active record exists; run synth deactivate " + identity + " before retrying");
  }

  const auto installation = read_json(installation_path);
  const auto& manifest = installation.at("manifest");
  const auto realization_id = operation_id(identity);
  json resolved;
  try {
    resolved = resolve_requirements(manifest, roots, realization_id);
  } catch (const OperationFailure& failure) {
    record_transaction(roots, "activate", identity, failure.status(), {{"reason", failure.what()}});
    throw;
  }

  const auto candidate = roots.runtime / "candidates" / realization_id;
  const auto active_runtime = roots.runtime / "active" / identity;
  const auto resolution_root = roots.state / "realizations" / identity / "resolved" / realization_id;
  const auto witness_path = candidate / "witness.json";
  const auto resolved_path = candidate / "resolved-surfaces.json";
  const auto candidate_log = candidate / "candidate.log";
  const auto readiness_log = candidate / "readiness.log";
  const auto realization_state = roots.state / "realizations" / identity;
  const auto preserved_witness = realization_state / ("witness-" + realization_id + ".json");
  ensure_directory(candidate);
  write_json_atomic(resolved_path, {{"epistemic_class", "RESOLVED"}, {"surfaces", resolved}});
  const auto environment = candidate_environment(manifest, realization_id, witness_path, resolved_path, roots);
  const auto executable = payload_entry(installation, manifest.at("realization").at("entrypoint").get<std::string>());
  const auto readiness_executable = payload_entry(installation, manifest.at("readiness").at("entrypoint").get<std::string>());
  const auto arguments = manifest.at("realization").at("arguments").get<std::vector<std::string>>();
  const auto readiness_arguments = manifest.at("readiness").at("arguments").get<std::vector<std::string>>();
  pid_t pid = 0;
  bool active_record_written = false;
  bool runtime_promoted = false;
  bool witness_preserved = false;
  try {
    pid = spawn_candidate(executable, arguments, candidate, environment, candidate_log);
    const auto start_ticks = process_start_ticks(pid);
    if (!start_ticks) throw std::runtime_error("candidate process identity could not be observed");
    const auto timeout = std::chrono::milliseconds(manifest.at("readiness").value("timeout_ms", 0));
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool ready = false;
    while (std::chrono::steady_clock::now() < deadline) {
      if (!process_alive(pid)) throw std::runtime_error("candidate exited before readiness");
      if (run_candidate_command(readiness_executable, readiness_arguments, candidate, environment, readiness_log, deadline) == 0) {
        ready = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!ready) throw std::runtime_error("candidate readiness timed out");
    const auto rss_kib = process_rss_kib(pid);
    if (manifest.at("resources").contains("activation_rss_mib_max")) {
      const auto limit_kib = manifest.at("resources").at("activation_rss_mib_max").get<long>() * 1024;
      if (rss_kib <= 0 || rss_kib > limit_kib) throw std::runtime_error("candidate exceeded or could not prove its activation RSS threshold");
    }
    const auto witness = validate_witness(witness_path, installation, realization_id, pid, resolved, roots);
    write_json_atomic(preserved_witness, witness);
    witness_preserved = true;
    const auto witness_digest = sha256(preserved_witness);
    ensure_directory(active_runtime.parent_path());
    fs::rename(candidate, active_runtime);
    runtime_promoted = true;
    const json active{
      {"identity", identity}, {"version", installation.at("version")}, {"digest", installation.at("digest")},
      {"realization_id", realization_id}, {"pid", pid}, {"process_start_ticks", *start_ticks},
      {"runtime_path", active_runtime.string()}, {"witness_path", preserved_witness.string()},
      {"witness_sha256", witness_digest}, {"resolved_surfaces", resolved}, {"activated_at", timestamp()}
    };
    write_json_atomic(active_path, active);
    active_record_written = true;
    record_transaction(roots, "activate", identity, "ACTIVE", {{"realization_id", realization_id}, {"pid", pid}, {"rss_kib", rss_kib}, {"witness", preserved_witness.string()}});
    if (as_json) std::cout << json{{"status", "ACTIVE"}, {"candidate", "VERIFIED"}, {"realization", active}}.dump() << '\n';
    else std::cout << "Candidate\n  " << identity << ' ' << installation.at("version").get<std::string>()
                   << "\n\nResolve requirements ... PASS\nStart isolated realization ... PASS\nReadiness ... PASS\nWitness ... PASS\nActivate ... PASS\n";
    return 0;
  } catch (const std::exception& error) {
    stop_process(pid);
    std::error_code ignored;
    fs::remove_all(candidate, ignored);
    if (runtime_promoted) fs::remove_all(active_runtime, ignored);
    fs::remove_all(resolution_root, ignored);
    if (witness_preserved) fs::remove(preserved_witness, ignored);
    if (active_record_written) fs::remove(active_path, ignored);
    record_transaction(roots, "activate", identity, "REJECTED", {{"reason", error.what()}});
    throw OperationFailure("REJECTED", std::string(error.what()) + "; active state unchanged");
  }
}

int deactivate_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 2) throw std::runtime_error("usage: synth deactivate <identity>");
  const auto identity = args[1];
  if (!valid_identity(identity)) throw std::runtime_error("invalid realization identity");
  const auto active_path = active_record_path(roots, identity);
  if (!fs::is_regular_file(active_path)) {
    if (as_json) std::cout << json{{"status", "NO_OP"}, {"identity", identity}, {"reason", "not active"}}.dump() << '\n';
    else std::cout << identity << " is not active.\nNo changes required.\n";
    return 0;
  }
  const auto active = read_json(active_path);
  const auto expected_runtime = roots.runtime / "active" / identity;
  if (fs::path(active.value("runtime_path", "")) != expected_runtime) {
    throw std::runtime_error("active realization record contains an unsafe runtime path");
  }
  if (active_process_matches(active)) stop_process(active.value("pid", 0));
  std::error_code ignored;
  fs::remove_all(expected_runtime, ignored);
  if (!fs::remove(active_path)) throw std::runtime_error("cannot remove active realization record");
  record_transaction(roots, "deactivate", identity, "DEACTIVATED", {{"realization_id", active.value("realization_id", "")}});
  if (as_json) std::cout << json{{"status", "DEACTIVATED"}, {"identity", identity}}.dump() << '\n';
  else std::cout << identity << " deactivated successfully.\n";
  return 0;
}

int remove_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() != 2) throw std::runtime_error("usage: synth remove <identity>");
  const auto identity = args[1];
  if (!valid_identity(identity)) throw std::runtime_error("invalid realization identity");
  if (fs::is_regular_file(active_record_path(roots, identity))) {
    throw OperationFailure("BLOCKED", identity + " is active; run 'synth deactivate " + identity + "' before removal");
  }
  const auto installation_path = roots.state / "installations" / (identity + ".json");
  if (!fs::is_regular_file(installation_path)) {
    if (as_json) std::cout << json{{"status", "NO_OP"}, {"identity", identity}, {"reason", "not installed"}}.dump() << '\n';
    else std::cout << identity << " is not installed.\nNo changes required.\n";
    return 0;
  }
  const auto installation = read_json(installation_path);
  if (!fs::remove(installation_path)) throw std::runtime_error("cannot remove installation record");
  record_transaction(roots, "remove", identity, "REMOVED", {{"digest", installation.value("digest", "")}, {"artifact_store", "preserved"}});
  if (as_json) std::cout << json{{"status", "REMOVED"}, {"identity", identity}, {"artifact_store", "preserved"}}.dump() << '\n';
  else std::cout << identity << " removed successfully.\nImmutable artifact remains available for future garbage collection.\n";
  return 0;
}

json manifest_summary(const json& manifest) {
  return {
    {"identity", manifest.at("identity")}, {"version", manifest.at("version")},
    {"description", manifest.at("description")}, {"provides", manifest.at("provides")},
    {"requires", manifest.at("requires")}, {"provenance", manifest.at("provenance")},
    {"artifact", manifest.at("artifact")}
  };
}

void print_manifest(const json& manifest) {
  std::cout << "Identity\n  " << manifest.at("identity").get<std::string>()
            << "\n\nVersion\n  " << manifest.at("version").get<std::string>()
            << "\n\nDescription\n  " << manifest.at("description").get<std::string>() << "\n\nProvides\n";
  for (const auto& surface : manifest.at("provides").at("surfaces")) std::cout << "  " << surface.at("id").get<std::string>() << '\n';
  std::cout << "\nRequires\n";
  for (const auto& surface : manifest.at("requires").at("surfaces")) std::cout << "  " << surface.at("id").get<std::string>() << '\n';
  std::cout << "\nProvenance\n  " << manifest.at("provenance").dump() << '\n';
}

int search_command(const std::vector<std::string>& args, bool as_json) {
  if (args.size() < 2) throw std::runtime_error("usage: synth search <query> --source <source>");
  const auto source_option = option_value(args, "--source");
  if (!source_option) throw std::runtime_error("search requires --source <source>");
  const auto source = open_source(*source_option);
  const auto records = source->manifests();
  json matches = json::array();
  for (const auto& record : records) {
    const auto identity = record.manifest.value("identity", "");
    const auto description = record.manifest.value("description", "");
    if (identity.find(args[1]) != std::string::npos || description.find(args[1]) != std::string::npos) matches.push_back(manifest_summary(record.manifest));
  }
  if (as_json) std::cout << matches.dump() << '\n';
  else if (matches.empty()) std::cout << "No artifacts found.\n";
  else for (const auto& match : matches) {
    std::cout << match.at("identity").get<std::string>() << "\n  version: " << match.at("version").get<std::string>()
              << "\n  description: " << match.at("description").get<std::string>() << "\n\n";
  }
  return 0;
}

int info_command(const std::vector<std::string>& args, bool as_json) {
  if (args.size() < 2) throw std::runtime_error("usage: synth info <identity> --source <source>");
  const auto source_option = option_value(args, "--source");
  if (!source_option) throw std::runtime_error("info requires --source <source>");
  const auto source = open_source(*source_option);
  const auto record = resolve(*source, args[1]);
  if (as_json) std::cout << manifest_summary(record.manifest).dump() << '\n';
  else print_manifest(record.manifest);
  return 0;
}

int install_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() < 2) throw std::runtime_error("usage: synth install <identity> --source <source>");
  const auto source_option = option_value(args, "--source");
  if (!source_option) throw std::runtime_error("install requires --source <source>");
  const auto source = open_source(*source_option);
  const auto record = resolve(*source, args[1]);
  const auto result = install(record, *source, roots);
  if (as_json) {
    std::cout << json{{"status", result.no_op ? "NO_OP" : "INSTALLED"}, {"active_state", "unchanged"}, {"installation", result.installation}}.dump() << '\n';
  } else if (result.no_op) {
    std::cout << args[1] << " is already installed with the same version and digest.\nNo changes required.\n";
  } else {
    std::cout << "Plan\n\n  Acquire\n    " << args[1] << ' ' << result.installation.at("version").get<std::string>()
              << "\n\n  Verify\n    SHA-256 " << result.installation.at("digest").get<std::string>()
              << "\n\n  Install\n    immutable store entry\n\n  Active state\n    unchanged\n\nInstalled successfully.\nNot active.\n";
  }
  return 0;
}

int installed_command(bool as_json, const Roots& roots) {
  const auto records = installed_records(roots);
  if (as_json) std::cout << records.dump() << '\n';
  else if (records.empty()) std::cout << "No artifacts installed.\n";
  else for (const auto& record : records) {
    std::cout << record.at("identity").get<std::string>() << "  " << record.at("version").get<std::string>()
              << "  [" << record.at("status").get<std::string>() << "]\n";
  }
  return 0;
}

} // namespace

MutationLock::MutationLock(const Roots& roots) {
  ensure_directory(roots.runtime);
  const auto path = roots.runtime / "transaction.lock";
  descriptor_ = ::open(path.c_str(), O_CREAT | O_RDONLY | O_CLOEXEC, 0600);
  if (descriptor_ < 0) throw std::runtime_error("cannot open mutation lock: " + path.string());
  while (::flock(descriptor_, LOCK_EX) != 0) {
    if (errno == EINTR) continue;
    ::close(descriptor_);
    descriptor_ = -1;
    throw std::runtime_error("cannot acquire mutation lock: " + path.string());
  }
}

MutationLock::~MutationLock() {
  if (descriptor_ >= 0) ::close(descriptor_);
}

bool handles(const std::vector<std::string>& args) {
  if (args.empty()) return false;
  return args[0] == "search" || args[0] == "info" || args[0] == "install" || args[0] == "installed" ||
         args[0] == "activate" || args[0] == "deactivate" || args[0] == "remove";
}

int dispatch(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  try {
    if (args[0] == "search") return search_command(args, as_json);
    if (args[0] == "info") return info_command(args, as_json);
    if (args[0] == "installed") return installed_command(as_json, roots);
    MutationLock lock(roots);
    if (args[0] == "install") return install_command(args, as_json, roots);
    if (args[0] == "activate") return activate_command(args, as_json, roots);
    if (args[0] == "deactivate") return deactivate_command(args, as_json, roots);
    if (args[0] == "remove") return remove_command(args, as_json, roots);
    throw std::runtime_error("unknown realization command");
  } catch (const OperationFailure& error) {
    if (as_json) std::cout << json{{"status", error.status()}, {"error", error.what()}}.dump() << '\n';
    else std::cerr << "Realization operation " << error.status() << ".\nCause: " << error.what() << '\n';
    return 1;
  } catch (const std::exception& error) {
    if (as_json) std::cout << json{{"status", "REJECTED"}, {"error", error.what()}}.dump() << '\n';
    else std::cerr << "Realization operation failed.\nCause: " << error.what() << '\n';
    return 1;
  }
}

json active_surfaces(const Roots& roots) {
  json surfaces = json::array();
  for (const auto& active : active_records(roots)) {
    try {
      const fs::path witness_path = active.at("witness_path").get<std::string>();
      if (sha256(witness_path) != active.value("witness_sha256", "")) continue;
      const auto witness = read_json(witness_path);
      if (witness.value("realization_id", "") != active.value("realization_id", "") ||
          witness.at("readiness").value("status", "") != "READY") continue;
      for (const auto& surface : witness.at("provided_surfaces")) {
        validate_observed_surface(surface);
        surfaces.push_back(surface);
      }
    } catch (const std::exception&) {
    }
  }
  return surfaces;
}

json observed_relations(const Roots& roots) {
  json relations = json::array();
  for (const auto& active : active_records(roots)) {
    try {
      const auto witness_path = active.at("witness_path").get<std::string>();
      if (sha256(witness_path) != active.value("witness_sha256", "")) continue;
      const auto witness = read_json(witness_path);
      if (witness.value("realization_id", "") != active.value("realization_id", "") ||
          witness.at("readiness").value("status", "") != "READY") continue;
      for (const auto& consumed : witness.at("consumed_surfaces")) {
        validate_consumed_surface(consumed);
        const auto id = consumed.at("id").get<std::string>();
        const auto& resolved = active.at("resolved_surfaces");
        const auto resolution = std::find_if(resolved.begin(), resolved.end(), [&id, &consumed](const auto& surface) {
          return surface.value("id", "") == id && surface.value("locator", "") == consumed.value("locator", "");
        });
        if (resolution == resolved.end()) continue;
        const auto expected_digest = resolution->value("expected_sha256", "");
        const auto attested_digest = consumed.value("evidence_sha256", "");
        const auto locator = resolution->value("locator", "");
        if (!valid_sha256(expected_digest) || attested_digest != expected_digest ||
            !fs::is_regular_file(locator) || sha256(locator) != expected_digest) continue;
        relations.push_back({
          {"id", active.at("identity").get<std::string>() + "-consumes-" + id},
          {"source", active.at("identity")}, {"target", resolution->value("owner", "")},
          {"surface", id}, {"status", "ACTIVE"}, {"epistemic_class", "OBSERVED"},
          {"observation", "runtime_witness"}, {"assertion", "consumed_surface"},
          {"assertion_mode", "participant_attestation"}, {"expected_sha256", expected_digest},
          {"attested_sha256", attested_digest}, {"verification", "MATCH"},
          {"evidence_ref", witness_path}, {"observed_at", consumed.at("observed_at")}
        });
      }
    } catch (const std::exception&) {
    }
  }
  return relations;
}

json ecosystem_participants(const Roots& roots) {
  json participants = json::array();
  const auto active = active_records(roots);
  for (const auto& installation : installed_records(roots)) {
    try {
      const auto identity = installation.at("identity").get<std::string>();
      const auto active_match = std::find_if(active.begin(), active.end(), [&identity](const auto& record) {
        return record.value("identity", "") == identity;
      });
      json observed = json::array();
      json realization_id = nullptr;
      json realization_evidence_ref = nullptr;
      std::string state = "INSTALLED";
      if (active_match != active.end()) {
        const fs::path witness_path = active_match->at("witness_path").get<std::string>();
        if (sha256(witness_path) == active_match->value("witness_sha256", "")) {
          const auto witness = read_json(witness_path);
          if (witness.value("realization_id", "") == active_match->value("realization_id", "") &&
              witness.at("readiness").value("status", "") == "READY") {
            for (const auto& surface : witness.at("provided_surfaces")) {
              validate_observed_surface(surface);
              observed.push_back(surface);
            }
            state = "ACTIVE";
            realization_id = active_match->at("realization_id");
            realization_evidence_ref = witness_path.string();
          }
        }
      }

      json declared = json::array();
      for (auto surface : installation.at("manifest").at("provides").at("surfaces")) {
        surface["epistemic_class"] = "DECLARED";
        declared.push_back(std::move(surface));
      }
      participants.push_back({
        {"identity", identity}, {"version", installation.at("version")},
        {"description", installation.at("manifest").at("description")}, {"state", state},
        {"epistemic_class", "OBSERVED"},
        {"registration_evidence_ref", (roots.state / "installations" / (identity + ".json")).string()},
        {"realization_id", realization_id}, {"realization_evidence_ref", realization_evidence_ref},
        {"declared_surfaces", std::move(declared)}, {"observed_surfaces", std::move(observed)}
      });
    } catch (const std::exception&) {
    }
  }
  std::sort(participants.begin(), participants.end(), [](const auto& left, const auto& right) {
    return left.value("identity", "") < right.value("identity", "");
  });
  return participants;
}

} // namespace synth::realization
