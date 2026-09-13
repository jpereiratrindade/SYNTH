#include "realization.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

#include <openssl/evp.h>

namespace synth::realization {
namespace {

constexpr std::string_view manifest_schema = "urn:synth:schema:artifact-manifest:0.1.0";

struct CommandResult {
  int exit_code{};
  std::string output;
};

struct ManifestRecord {
  json manifest;
  fs::path manifest_path;
  fs::path source_root;
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

void validate_surface_declaration(const json& surface) {
  if (!surface.is_object() || surface.value("id", "").empty()) throw std::runtime_error("surface declaration requires a non-empty id");
}

void validate_manifest(const json& manifest) {
  try {
    if (!manifest.is_object() || manifest.value("$schema", "") != manifest_schema ||
        manifest.value("schema_version", "") != "0.1.0" || !valid_identity(manifest.value("identity", "")) ||
        manifest.value("version", "").empty() || manifest.value("description", "").empty()) {
      throw std::runtime_error("manifest identity, version or schema is invalid");
    }
    const auto& artifact = manifest.at("artifact");
    if (artifact.value("media_type", "") != "application/x-tar" ||
        !valid_relative_path(artifact.value("path", "")) || !valid_sha256(artifact.value("sha256", ""))) {
      throw std::runtime_error("artifact path, media type or SHA-256 is invalid");
    }
    if (!manifest.at("provenance").is_object() || manifest.at("provenance").value("license", "").empty()) {
      throw std::runtime_error("artifact provenance is invalid");
    }
    for (const auto& surface : manifest.at("provides").at("surfaces")) validate_surface_declaration(surface);
    for (const auto& surface : manifest.at("requires").at("surfaces")) validate_surface_declaration(surface);
    const auto& realization = manifest.at("realization");
    if (!valid_relative_path(realization.value("entrypoint", "")) || !realization.at("arguments").is_array() ||
        !realization.at("environment_allowed").is_array()) throw std::runtime_error("realization declaration is invalid");
    const auto& readiness = manifest.at("readiness");
    if (readiness.value("type", "") != "command" || !valid_relative_path(readiness.value("entrypoint", "")) ||
        !readiness.at("arguments").is_array() || readiness.value("timeout_ms", 0) <= 0) {
      throw std::runtime_error("readiness declaration is invalid");
    }
    if (manifest.contains("resources") && !manifest.at("resources").is_object()) throw std::runtime_error("resource envelope is invalid");
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

std::vector<ManifestRecord> manifests_from(const fs::path& root) {
  std::vector<ManifestRecord> records;
  for (const auto& entry : fs::directory_iterator(root / "manifests")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
    auto manifest = read_json(entry.path());
    validate_manifest(manifest);
    records.push_back({std::move(manifest), entry.path(), root});
  }
  std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
    return left.manifest.value("identity", "") < right.manifest.value("identity", "");
  });
  return records;
}

ManifestRecord resolve(const fs::path& source, std::string_view identity) {
  std::optional<ManifestRecord> match;
  for (auto& record : manifests_from(source)) {
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

fs::path artifact_path(const ManifestRecord& record) {
  const auto candidate = record.source_root / record.manifest.at("artifact").at("path").get<std::string>();
  if (!fs::is_regular_file(candidate) || !path_within(record.source_root, candidate)) {
    throw std::runtime_error("artifact escapes or is absent from local source");
  }
  return fs::canonical(candidate);
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

InstallResult install(const ManifestRecord& record, const Roots& roots) {
  const auto identity = record.manifest.at("identity").get<std::string>();
  const auto version = record.manifest.at("version").get<std::string>();
  const auto expected_digest = record.manifest.at("artifact").at("sha256").get<std::string>();
  const auto installation_path = roots.state / "installations" / (identity + ".json");
  if (fs::is_regular_file(installation_path)) {
    const auto existing = read_json(installation_path);
    if (existing.value("version", "") == version && existing.value("digest", "") == expected_digest) return {existing, true};
    throw std::runtime_error("identity is already installed with different content; upgrade is not implemented");
  }

  const auto artifact = artifact_path(record);
  const auto actual_digest = sha256(artifact);
  if (actual_digest != expected_digest) {
    record_transaction(roots, "install", identity, "REJECTED", {{"reason", "SHA-256 mismatch"}, {"expected", expected_digest}, {"observed", actual_digest}});
    throw std::runtime_error("artifact digest mismatch; candidate REJECTED and active state unchanged");
  }
  validate_tar(artifact);

  const auto store_root = roots.data / "store";
  ensure_directory(store_root);
  const auto destination = store_root / expected_digest;
  if (!fs::exists(destination)) {
    const auto stage = store_root / (".stage-" + operation_id("install"));
    try {
      ensure_directory(stage / "payload");
      const auto extraction = run_capture({"tar", "--extract", "--file", artifact.string(), "--directory", (stage / "payload").string(),
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
  }

  const json installation{
    {"identity", identity}, {"version", version}, {"digest", expected_digest},
    {"store_path", destination.string()}, {"manifest", record.manifest},
    {"source", record.source_root.string()}, {"installed_at", timestamp()}, {"status", "INSTALLED"}
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
  const auto records = manifests_from(source_path(*source_option));
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
  const auto record = resolve(source_path(*source_option), args[1]);
  if (as_json) std::cout << manifest_summary(record.manifest).dump() << '\n';
  else print_manifest(record.manifest);
  return 0;
}

int install_command(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  if (args.size() < 2) throw std::runtime_error("usage: synth install <identity> --source <source>");
  const auto source_option = option_value(args, "--source");
  if (!source_option) throw std::runtime_error("install requires --source <source>");
  const auto record = resolve(source_path(*source_option), args[1]);
  const auto result = install(record, roots);
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

bool handles(const std::vector<std::string>& args) {
  if (args.empty()) return false;
  return args[0] == "search" || args[0] == "info" || args[0] == "install" || args[0] == "installed" ||
         args[0] == "activate" || args[0] == "deactivate" || args[0] == "remove";
}

int dispatch(const std::vector<std::string>& args, bool as_json, const Roots& roots) {
  try {
    if (args[0] == "search") return search_command(args, as_json);
    if (args[0] == "info") return info_command(args, as_json);
    if (args[0] == "install") return install_command(args, as_json, roots);
    if (args[0] == "installed") return installed_command(as_json, roots);
    throw std::runtime_error(args[0] + " is defined but not implemented in this intermediate commit");
  } catch (const std::exception& error) {
    if (as_json) std::cout << json{{"status", "REJECTED"}, {"error", error.what()}}.dump() << '\n';
    else std::cerr << "Realization operation failed.\nCause: " << error.what() << '\n';
    return 1;
  }
}

json active_surfaces(const Roots&) { return json::array(); }
json observed_relations(const Roots&) { return json::array(); }

} // namespace synth::realization
