#!/usr/bin/env bash
set -euo pipefail

synth_bin="$1"
source_root="$2"
build_root="$3"
test_root="$(mktemp -d)"
trap 'rm -rf "$test_root"' EXIT

export XDG_CONFIG_HOME="$test_root/config"
export XDG_DATA_HOME="$test_root/data"
export XDG_STATE_HOME="$test_root/state"
export XDG_RUNTIME_DIR="$test_root/runtime"

fingerprint() {
  {
    find "$test_root" -mindepth 1 -printf '%P %y %s\n'
    find "$test_root" -type f -exec sha256sum {} +
  } | sort
}

# Identity and help are pure even in a completely fresh environment.
"$synth_bin" version | grep -Fx "SYNTH 0.1.0"
"$synth_bin" help | grep -F "without side effects"
test -z "$(find "$test_root" -mindepth 1 -print -quit)"

# All queries observe without persisting or fabricating configuration.
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS"
"$synth_bin" foundation verify | grep -F "FOUNDATION_VERIFY"
"$synth_bin" foundation verify | grep -F "configuration=none"
"$synth_bin" foundation verify --json | grep -F '"epistemic_class":"DERIVED"'
"$synth_bin" surfaces | grep -F "synth.cli"
if "$synth_bin" surfaces | grep -F "synth.evidence"; then
  echo "evidence surface reported before evidence existed" >&2
  exit 1
fi
if "$synth_bin" surfaces | grep -F "synth.web"; then
  echo "unobserved web surface was reported" >&2
  exit 1
fi
"$synth_bin" relations | grep -Fx "No relations observed."
"$synth_bin" status --json | grep -F '"ECOSYSTEM_SYNTH":"NOT_YET_APPLICABLE"'
test -z "$(find "$test_root" -mindepth 1 -print -quit)"

# Only the evidence command creates persistent/ephemeral state.
"$synth_bin" evidence --json | grep -F '"epistemic_class": "OBSERVED"'
"$synth_bin" evidence --json | grep -F '"rss_kib"'
"$synth_bin" evidence --json | grep -F '"max_rss_kib"'
"$synth_bin" evidence --json | grep -F '"configuration": null'
test ! -e "$XDG_CONFIG_HOME/synth"
test -f "$XDG_STATE_HOME/synth/evidence/latest.json"
test -d "$XDG_RUNTIME_DIR/synth"
"$synth_bin" surfaces | grep -F "synth.evidence"

# Repeated read-only commands cannot change realization or evidence.
read_only_before="$(fingerprint)"
evidence_before="$(sha256sum "$XDG_STATE_HOME/synth/evidence/latest.json")"
"$synth_bin" version >/dev/null
"$synth_bin" help >/dev/null
"$synth_bin" status >/dev/null
"$synth_bin" foundation verify >/dev/null
"$synth_bin" surfaces >/dev/null
"$synth_bin" relations >/dev/null
read_only_after="$(fingerprint)"
evidence_after_queries="$(sha256sum "$XDG_STATE_HOME/synth/evidence/latest.json")"
test "$read_only_before" = "$read_only_after"
test "$evidence_before" = "$evidence_after_queries"

# A fresh explicit observation changes evidence but still not configuration.
"$synth_bin" evidence >/dev/null
evidence_after_observation="$(sha256sum "$XDG_STATE_HOME/synth/evidence/latest.json")"
test "$evidence_before" != "$evidence_after_observation"
test ! -e "$XDG_CONFIG_HOME/synth"

# The implementation delegates JSON parsing and exposes actual Draft 2020-12 schemas.
grep -F '#include <nlohmann/json.hpp>' "$source_root/src/main.cpp" >/dev/null
if grep -F 'class JsonParser' "$source_root/src/main.cpp"; then
  echo "hand-written JSON parser remains" >&2
  exit 1
fi
if grep -E '\{"[A-Z0-9_]+", true' "$source_root/src/main.cpp"; then
  echo "self-certifying gate remains" >&2
  exit 1
fi
grep -F '"$schema": "https://json-schema.org/draft/2020-12/schema"' "$source_root/schemas/surface.schema.json" >/dev/null
grep -F '"properties"' "$source_root/schemas/relation.schema.json" >/dev/null
python3 - "$source_root/schemas/surface.schema.json" "$source_root/schemas/relation.schema.json" <<'PY'
import json
import pathlib
import sys

from jsonschema.validators import validator_for

for schema_path in map(pathlib.Path, sys.argv[1:]):
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    validator = validator_for(schema)
    validator.check_schema(schema)
    if validator.META_SCHEMA.get("$id") != "https://json-schema.org/draft/2020-12/schema":
        raise SystemExit(f"{schema_path} did not select JSON Schema Draft 2020-12")
PY

# The installed realization carries its normative data and works away from the checkout.
install_root="$test_root/install"
cmake --install "$build_root" --prefix "$install_root" >/dev/null
installed_synth="$install_root/bin/synth"
test -x "$installed_synth"
test -f "$install_root/share/synth/SYNTH-FOUNDATION-001-v0.4.0.md"
test -f "$install_root/share/synth/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md"
test -f "$install_root/share/synth/schemas/relation.schema.json"
if strings "$installed_synth" | grep -F "$source_root"; then
  echo "installed binary contains a source-tree dependency" >&2
  exit 1
fi

unset SYNTH_DATA_DIR
export XDG_CONFIG_HOME="$test_root/installed-config"
export XDG_DATA_HOME="$test_root/installed-data"
export XDG_STATE_HOME="$test_root/installed-state"
export XDG_RUNTIME_DIR="$test_root/installed-runtime"
(
  cd "$test_root"
  "$installed_synth" version | grep -Fx "SYNTH 0.1.0"
  "$installed_synth" foundation verify | grep -F "CONTEXTLAB_DOCUMENT_COMPAT"
  "$installed_synth" status | grep -F "SYSTEM_SYNTH_READY     PASS"
)
test ! -e "$XDG_CONFIG_HOME/synth"
test ! -e "$XDG_DATA_HOME/synth"
test ! -e "$XDG_STATE_HOME/synth"
test ! -e "$XDG_RUNTIME_DIR/synth"

# Prove that schema and ContextLab checks parse content instead of accepting filenames or strings.
relation_schema="$install_root/share/synth/schemas/relation.schema.json"
cp "$relation_schema" "$relation_schema.backup"
sed -i 's#urn:synth:schema:relation:0.1.0#urn:synth:schema:relation:BROKEN#' "$relation_schema"
schema_failure="$("$installed_synth" foundation verify 2>&1 || true)"
printf '%s\n' "$schema_failure" | grep -E "RELATION_MODEL_GENERIC[[:space:]]+FAIL"
if "$installed_synth" foundation verify >/dev/null 2>&1; then
  echo "invalid relation schema was accepted" >&2
  exit 1
fi
mv "$relation_schema.backup" "$relation_schema"

foundation_document="$install_root/share/synth/SYNTH-FOUNDATION-001-v0.4.0.md"
sed -i 's/"id": "SYNTH-FOUNDATION-001"/"id": "BROKEN-FOUNDATION"/' "$foundation_document"
context_failure="$("$installed_synth" foundation verify 2>&1 || true)"
printf '%s\n' "$context_failure" | grep -E "CONTEXTLAB_DOCUMENT_COMPAT[[:space:]]+FAIL"
if "$installed_synth" foundation verify >/dev/null 2>&1; then
  echo "invalid ContextLab metadata was accepted" >&2
  exit 1
fi

echo "All foundational hardening checks passed."
