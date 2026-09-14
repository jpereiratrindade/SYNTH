#!/usr/bin/env bash
set -euo pipefail

synth_bin="$1"
source_root="$2"
fixture_source="$source_root/tests/fixtures/local-source"
test_root="$(mktemp -d)"

cleanup() {
  while IFS= read -r active_record; do
    pid="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("pid", 0))' "$active_record" 2>/dev/null || true)"
    if [[ "$pid" =~ ^[1-9][0-9]*$ ]]; then
      kill -TERM -- "-$pid" 2>/dev/null || true
      kill -TERM "$pid" 2>/dev/null || true
    fi
  done < <(find "$test_root" -path '*/realizations/*/active.json' -type f 2>/dev/null || true)
  sleep 0.2
  chmod -R u+w "$test_root" 2>/dev/null || true
  rm -rf "$test_root"
}
trap cleanup EXIT

use_scenario() {
  local name="$1"
  export XDG_CONFIG_HOME="$test_root/$name/config"
  export XDG_DATA_HOME="$test_root/$name/data"
  export XDG_STATE_HOME="$test_root/$name/state"
  export XDG_RUNTIME_DIR="$test_root/$name/runtime"
}

fingerprint() {
  local root="$1"
  if ! test -e "$root"; then
    printf 'absent\n'
    return
  fi
  {
    find "$root" -mindepth 1 -printf '%P %y %s\n'
    find "$root" -type f -exec sha256sum {} +
  } | sort
}

expect_failure() {
  local output_file="$1"
  shift
  if "$@" >"$output_file" 2>&1; then
    printf 'command unexpectedly succeeded: %s\n' "$*" >&2
    return 1
  fi
}

repack_artifact() {
  local source="$1"
  local payload="$2"
  tar --create --file "$source/artifacts/synth-web-0.1.0.tar" --directory "$payload" .
  local digest
  digest="$(sha256sum "$source/artifacts/synth-web-0.1.0.tar" | cut -d' ' -f1)"
  python3 - "$source/manifests/synth-web.json" "$digest" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["artifact"]["sha256"] = sys.argv[2]
path.write_text(json.dumps(value), encoding="utf-8")
PY
}

manifest="$fixture_source/manifests/synth-web.json"
artifact="$fixture_source/artifacts/synth-web-0.1.0.tar"

# Both public interchange documents are actual Draft 2020-12 schemas, and the
# controlled manifest conforms to its schema.
python3 - "$source_root/schemas/artifact-manifest.schema.json" \
  "$source_root/schemas/realization-witness.schema.json" "$manifest" <<'PY'
import json
import pathlib
import sys

from jsonschema.validators import validator_for

manifest_schema_path, witness_schema_path, manifest_path = map(pathlib.Path, sys.argv[1:])
manifest_schema = json.loads(manifest_schema_path.read_text(encoding="utf-8"))
witness_schema = json.loads(witness_schema_path.read_text(encoding="utf-8"))
for schema in (manifest_schema, witness_schema):
    validator = validator_for(schema)
    validator.check_schema(schema)
    assert validator.META_SCHEMA["$id"] == "https://json-schema.org/draft/2020-12/schema"
validator_for(manifest_schema)(manifest_schema).validate(json.loads(manifest_path.read_text(encoding="utf-8")))
PY
echo "REALIZATION_MANIFEST_SCHEMA        PASS"

expected_digest="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["artifact"]["sha256"])' "$manifest")"
observed_digest="$(sha256sum "$artifact" | cut -d' ' -f1)"
test "$expected_digest" = "$observed_digest"
echo "ARTIFACT_INTEGRITY                 PASS"

# Invalid manifests and digests are rejected without installation or impact on
# core readiness.
invalid_source="$test_root/invalid-source"
cp -a "$fixture_source" "$invalid_source"
python3 - "$invalid_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
del value["identity"]
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario invalid
expect_failure "$test_root/invalid.log" "$synth_bin" install synth-web --source "$invalid_source"
grep -F "manifest" "$test_root/invalid.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/installations/synth-web.json"

strict_manifest_source="$test_root/strict-manifest-source"
cp -a "$fixture_source" "$strict_manifest_source"
python3 - "$strict_manifest_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["unexpected"] = True
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario strict-manifest
expect_failure "$test_root/strict-manifest.log" "$synth_bin" install synth-web --source "$strict_manifest_source"
grep -F "unexpected field" "$test_root/strict-manifest.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/installations/synth-web.json"
echo "MANIFEST_SCHEMA_RUNTIME_ENFORCED  PASS"

bad_digest_source="$test_root/bad-digest-source"
cp -a "$fixture_source" "$bad_digest_source"
python3 - "$bad_digest_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["artifact"]["sha256"] = "0" * 64
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario bad-digest
expect_failure "$test_root/digest.log" "$synth_bin" install synth-web --source "$bad_digest_source"
grep -F "digest mismatch" "$test_root/digest.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/installations/synth-web.json"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "FAILED_CANDIDATE_PRESERVES_SYNTH  PASS"

# Source queries are factual and pure.
use_scenario normal
scenario_root="$test_root/normal"
before_queries="$(fingerprint "$scenario_root")"
"$synth_bin" search synth-web --source "$fixture_source" | grep -F "factual web projection for SYNTH" >/dev/null
"$synth_bin" info synth-web --source "$fixture_source" | grep -F "synth.evidence" >/dev/null
"$synth_bin" installed | grep -F "No artifacts installed." >/dev/null
after_queries="$(fingerprint "$scenario_root")"
test "$before_queries" = "$after_queries"
echo "LOCAL_SOURCE_RESOLUTION           PASS"

# Installation verifies the digest and changes no active realization.
"$synth_bin" install synth-web --source "$fixture_source" | grep -F "Installed successfully." >/dev/null
installation="$XDG_STATE_HOME/synth/installations/synth-web.json"
test -f "$installation"
store_path="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["store_path"])' "$installation")"
test -x "$store_path/payload/bin/synth-web"
test -f "$store_path/manifest.json"
test -f "$store_path/artifact.tar"
test "$(sha256sum "$store_path/artifact.tar" | cut -d' ' -f1)" = "$expected_digest"
echo "ARTIFACT_SNAPSHOT_INTEGRITY       PASS"
test -z "$(find "$store_path" -perm /222 -print -quit)"
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
test ! -e "$XDG_RUNTIME_DIR/synth/active/synth-web"
echo "IMMUTABLE_INSTALL_STORE           PASS"
echo "INSTALL_NO_ACTIVE_MUTATION        PASS"

# Installing the same identity, version and digest is a real no-op.
installation_before="$(sha256sum "$installation")"
store_before="$(fingerprint "$store_path")"
"$synth_bin" install synth-web --source "$fixture_source" | grep -F "No changes required." >/dev/null
installation_after="$(sha256sum "$installation")"
store_after="$(fingerprint "$store_path")"
test "$installation_before" = "$installation_after"
test "$store_before" = "$store_after"
echo "INSTALL_IDEMPOTENCE              PASS"

# A declared relation is not observed before activation. Missing factual
# evidence blocks activation while preserving the installation and core.
"$synth_bin" relations | grep -Fx "No relations observed." >/dev/null
expect_failure "$test_root/missing.log" "$synth_bin" activate synth-web
grep -F "BLOCKED" "$test_root/missing.log" >/dev/null
grep -F "synth.evidence" "$test_root/missing.log" >/dev/null
test -f "$installation"
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "MISSING_REQUIREMENT_BLOCKS        PASS"

# A fresh public evidence surface permits isolated activation. The candidate
# must pass readiness and produce a schema-valid witness before promotion.
"$synth_bin" evidence >/dev/null
"$synth_bin" activate synth-web | grep -F "Activate ... PASS" >/dev/null
active="$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
test -f "$active"
witness="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["witness_path"])' "$active")"
test -f "$witness"
python3 - "$source_root/schemas/realization-witness.schema.json" "$witness" <<'PY'
import json, pathlib, sys
from jsonschema.validators import validator_for
schema = json.loads(pathlib.Path(sys.argv[1]).read_text())
witness = json.loads(pathlib.Path(sys.argv[2]).read_text())
validator_for(schema)(schema, format_checker=None).validate(witness)
assert witness["identity"] == "synth-web"
assert witness["readiness"]["status"] == "READY"
assert any(item["id"] == "synth.evidence" for item in witness["consumed_surfaces"])
PY
python3 - "$active" "$witness" <<'PY'
import hashlib, json, pathlib, sys
active = json.loads(pathlib.Path(sys.argv[1]).read_text())
witness = json.loads(pathlib.Path(sys.argv[2]).read_text())
resolved = active["resolved_surfaces"][0]
consumed = witness["consumed_surfaces"][0]
snapshot = pathlib.Path(resolved["locator"])
assert snapshot.is_file()
assert resolved["source_locator"] != resolved["locator"]
assert hashlib.sha256(snapshot.read_bytes()).hexdigest() == resolved["expected_sha256"]
assert consumed["evidence_sha256"] == resolved["expected_sha256"]
assert consumed["locator"] == resolved["locator"]
PY
"$synth_bin" surfaces | grep -F "synth-web.http" >/dev/null
"$synth_bin" relations | grep -F "consumes -> synth.evidence" >/dev/null
"$synth_bin" relations --json | grep -F '"epistemic_class":"OBSERVED"' >/dev/null
"$synth_bin" relations --json | grep -F '"assertion_mode":"participant_attestation"' >/dev/null
"$synth_bin" relations --json | grep -F '"verification":"MATCH"' >/dev/null
"$synth_bin" relations --json >"$test_root/relations.json"
python3 - "$source_root/schemas/relation.schema.json" "$test_root/relations.json" <<'PY'
import json, pathlib, sys
from jsonschema.validators import validator_for
schema = json.loads(pathlib.Path(sys.argv[1]).read_text())
relations = json.loads(pathlib.Path(sys.argv[2]).read_text())
assert len(relations) == 1
validator_for(schema)(schema).validate(relations[0])
PY
echo "CANDIDATE_ISOLATION               PASS"
echo "READINESS_VERIFICATION            PASS"
echo "RUNTIME_WITNESS                   PASS"
echo "EVIDENCE_SNAPSHOT_PINNED          PASS"
echo "OBSERVED_RELATION_GROUNDED        PASS"

# The promoted participant runs from the immutable store and serves the public
# evidence it consumed, without either source checkout.
python3 - "$witness" "$store_path" <<'PY'
import json, pathlib, sys, urllib.request
witness = json.loads(pathlib.Path(sys.argv[1]).read_text())
store = pathlib.Path(sys.argv[2]).resolve()
endpoint = witness["provided_surfaces"][0]["locator"]
with urllib.request.urlopen(endpoint + "/", timeout=2) as response:
    assert b"Observed surfaces" in response.read()
with urllib.request.urlopen(endpoint + "/api/state", timeout=2) as response:
    state = json.load(response)
assert state["identity"] == "SYNTH"
process = pathlib.Path(f"/proc/{witness['process']['pid']}/cmdline").read_bytes().decode(errors="replace")
assert str(store) in process
PY
echo "SYNTH_WEB_INDEPENDENT_RUNTIME     PASS"

# Every query remains pure even while a participant is active.
pure_before="$(fingerprint "$scenario_root")"
"$synth_bin" version >/dev/null
"$synth_bin" help >/dev/null
"$synth_bin" status >/dev/null
"$synth_bin" foundation verify >/dev/null
"$synth_bin" surfaces >/dev/null
"$synth_bin" relations >/dev/null
"$synth_bin" installed >/dev/null
"$synth_bin" search synth-web --source "$fixture_source" >/dev/null
"$synth_bin" info synth-web --source "$fixture_source" >/dev/null
pure_after="$(fingerprint "$scenario_root")"
test "$pure_before" = "$pure_after"
echo "PURE_QUERY_NO_SIDE_EFFECTS        PASS"

# Active removal is blocked; deactivation is safe/idempotent; then installation
# removal succeeds without touching private participant state.
expect_failure "$test_root/remove-active.log" "$synth_bin" remove synth-web
grep -F "synth deactivate synth-web" "$test_root/remove-active.log" >/dev/null
test -f "$active"
"$synth_bin" deactivate synth-web | grep -F "deactivated successfully" >/dev/null
"$synth_bin" relations | grep -Fx "No relations observed." >/dev/null
"$synth_bin" deactivate synth-web | grep -F "No changes required." >/dev/null
"$synth_bin" remove synth-web | grep -F "removed successfully" >/dev/null
"$synth_bin" installed | grep -F "No artifacts installed." >/dev/null
test -d "$store_path"
echo "SAFE_DEACTIVATION                 PASS"
echo "SAFE_REMOVAL                      PASS"

# A participant that never reaches readiness is rejected; it cannot create an
# observed relation or disturb core readiness, and its installation survives.
failing_source="$test_root/failing-source"
cp -a "$fixture_source" "$failing_source"
python3 - "$failing_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["realization"]["arguments"] = ["--fail-readiness"]
value["readiness"]["timeout_ms"] = 500
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario failing-readiness
"$synth_bin" install synth-web --source "$failing_source" >/dev/null
"$synth_bin" evidence >/dev/null
expect_failure "$test_root/readiness.log" "$synth_bin" activate synth-web
grep -F "REJECTED" "$test_root/readiness.log" >/dev/null
grep -F "readiness" "$test_root/readiness.log" >/dev/null
test -f "$XDG_STATE_HOME/synth/installations/synth-web.json"
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
test -z "$(find "$XDG_RUNTIME_DIR/synth/candidates" -mindepth 1 -print -quit 2>/dev/null || true)"
"$synth_bin" relations | grep -Fx "No relations observed." >/dev/null
"$synth_bin" status | grep -F "FOUNDATION_READY       PASS" >/dev/null
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "FAILED_CANDIDATE_PRESERVES_SYNTH  PASS"

# A candidate may pass its own readiness command and still be rejected when its
# witness does not match the installed identity.
invalid_witness_source="$test_root/invalid-witness-source"
cp -a "$fixture_source" "$invalid_witness_source"
invalid_witness_payload="$test_root/invalid-witness-payload"
mkdir -p "$invalid_witness_payload"
tar --extract --file "$artifact" --directory "$invalid_witness_payload"
sed -i 's/IDENTITY = "synth-web"/IDENTITY = "wrong-web"/' "$invalid_witness_payload/bin/synth-web"
tar --create --file "$invalid_witness_source/artifacts/synth-web-0.1.0.tar" --directory "$invalid_witness_payload" .
invalid_witness_digest="$(sha256sum "$invalid_witness_source/artifacts/synth-web-0.1.0.tar" | cut -d' ' -f1)"
python3 - "$invalid_witness_source/manifests/synth-web.json" "$invalid_witness_digest" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["artifact"]["sha256"] = sys.argv[2]
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario invalid-witness
"$synth_bin" install synth-web --source "$invalid_witness_source" >/dev/null
"$synth_bin" evidence >/dev/null
expect_failure "$test_root/invalid-witness.log" "$synth_bin" activate synth-web
grep -F "REJECTED" "$test_root/invalid-witness.log" >/dev/null
grep -F "witness identity" "$test_root/invalid-witness.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
"$synth_bin" relations | grep -Fx "No relations observed." >/dev/null
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "RUNTIME_WITNESS_REJECTION         PASS"

# A syntactically valid but false evidence digest is rejected. The relation is
# grounded in a digest pinned by SYNTH, not merely in participant syntax.
false_digest_source="$test_root/false-digest-source"
cp -a "$fixture_source" "$false_digest_source"
false_digest_payload="$test_root/false-digest-payload"
mkdir -p "$false_digest_payload"
tar --extract --file "$artifact" --directory "$false_digest_payload"
sed -i 's/hashlib.sha256(evidence_bytes).hexdigest()/"a" * 64/' "$false_digest_payload/bin/synth-web"
repack_artifact "$false_digest_source" "$false_digest_payload"
use_scenario false-witness-digest
"$synth_bin" install synth-web --source "$false_digest_source" >/dev/null
"$synth_bin" evidence >/dev/null
expect_failure "$test_root/false-digest-witness.log" "$synth_bin" activate synth-web
grep -F "attestation does not match" "$test_root/false-digest-witness.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "EVIDENCE_DIGEST_BOUND             PASS"

# A witness missing a field required by the published schema is rejected by the
# runtime before promotion, not discovered later by foundation verification.
incomplete_witness_source="$test_root/incomplete-witness-source"
cp -a "$fixture_source" "$incomplete_witness_source"
incomplete_witness_payload="$test_root/incomplete-witness-payload"
mkdir -p "$incomplete_witness_payload"
tar --extract --file "$artifact" --directory "$incomplete_witness_payload"
sed -i '/"owner": IDENTITY,/d' "$incomplete_witness_payload/bin/synth-web"
repack_artifact "$incomplete_witness_source" "$incomplete_witness_payload"
use_scenario incomplete-witness
"$synth_bin" install synth-web --source "$incomplete_witness_source" >/dev/null
"$synth_bin" evidence >/dev/null
expect_failure "$test_root/incomplete-witness.log" "$synth_bin" activate synth-web
grep -F "owner" "$test_root/incomplete-witness.log" >/dev/null
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "WITNESS_SCHEMA_RUNTIME_ENFORCED   PASS"

# The readiness subprocess is bounded by the declared deadline even when the
# command itself is still blocked.
slow_readiness_source="$test_root/slow-readiness-source"
cp -a "$fixture_source" "$slow_readiness_source"
slow_readiness_payload="$test_root/slow-readiness-payload"
mkdir -p "$slow_readiness_payload"
tar --extract --file "$artifact" --directory "$slow_readiness_payload"
sed -i '/import pathlib/a import time' "$slow_readiness_payload/bin/readiness"
sed -i '/def main() -> int:/a\    time.sleep(2)' "$slow_readiness_payload/bin/readiness"
repack_artifact "$slow_readiness_source" "$slow_readiness_payload"
python3 - "$slow_readiness_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["readiness"]["timeout_ms"] = 500
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario slow-readiness
"$synth_bin" install synth-web --source "$slow_readiness_source" >/dev/null
"$synth_bin" evidence >/dev/null
started_ms="$(date +%s%3N)"
expect_failure "$test_root/slow-readiness.log" "$synth_bin" activate synth-web
elapsed_ms="$(($(date +%s%3N) - started_ms))"
grep -F "readiness timed out" "$test_root/slow-readiness.log" >/dev/null
test "$elapsed_ms" -lt 1800
test ! -e "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
echo "READINESS_TIMEOUT_ENFORCED        PASS"

# Declared kind/media constraints participate in resolution; a same-id but
# incompatible surface is not silently selected.
incompatible_source="$test_root/incompatible-source"
cp -a "$fixture_source" "$incompatible_source"
python3 - "$incompatible_source/manifests/synth-web.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
value = json.loads(path.read_text())
value["requires"]["surfaces"][0]["kind"] = "http"
path.write_text(json.dumps(value), encoding="utf-8")
PY
use_scenario incompatible-resolution
"$synth_bin" install synth-web --source "$incompatible_source" >/dev/null
"$synth_bin" evidence >/dev/null
expect_failure "$test_root/incompatible.log" "$synth_bin" activate synth-web
grep -F "compatible surface" "$test_root/incompatible.log" >/dev/null
echo "COMPATIBLE_SURFACE_RESOLUTION     PASS"

# Mutations are serialized across processes. Exactly one activation promotes;
# the follower observes the committed realization as a no-op.
use_scenario concurrent
"$synth_bin" install synth-web --source "$fixture_source" >/dev/null
"$synth_bin" evidence >/dev/null
"$synth_bin" activate synth-web >"$test_root/concurrent-1.log" 2>&1 &
first_activation=$!
"$synth_bin" activate synth-web >"$test_root/concurrent-2.log" 2>&1 &
second_activation=$!
set +e
wait "$first_activation"
first_status=$?
wait "$second_activation"
second_status=$?
set -e
test "$first_status" -eq 0
test "$second_status" -eq 0
test "$(grep -l "Activate ... PASS" "$test_root"/concurrent-*.log | wc -l)" -eq 1
test "$(grep -l "already active" "$test_root"/concurrent-*.log | wc -l)" -eq 1
test -f "$XDG_STATE_HOME/synth/realizations/synth-web/active.json"
test -d "$XDG_RUNTIME_DIR/synth/active/synth-web"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS" >/dev/null
"$synth_bin" deactivate synth-web >/dev/null
echo "CONCURRENT_MUTATION_SERIALIZED    PASS"

# A live but unrelated PID cannot turn a stale record into an activation no-op.
use_scenario process-identity
"$synth_bin" install synth-web --source "$fixture_source" >/dev/null
active_dir="$XDG_STATE_HOME/synth/realizations/synth-web"
runtime_dir="$XDG_RUNTIME_DIR/synth/active/synth-web"
mkdir -p "$active_dir" "$runtime_dir"
sleep 30 &
unrelated_pid=$!
python3 - "$active_dir/active.json" "$unrelated_pid" "$runtime_dir" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
path.write_text(json.dumps({
    "identity": "synth-web",
    "pid": int(sys.argv[2]),
    "process_start_ticks": 0,
    "runtime_path": sys.argv[3],
}), encoding="utf-8")
PY
expect_failure "$test_root/process-identity.log" "$synth_bin" activate synth-web
grep -F "stale active record" "$test_root/process-identity.log" >/dev/null
"$synth_bin" deactivate synth-web >/dev/null
kill "$unrelated_pid" 2>/dev/null || true
wait "$unrelated_pid" 2>/dev/null || true
echo "ACTIVE_PROCESS_IDENTITY           PASS"

# CI builds this fixture without any path or build dependency on SYNTH-WEB.
if grep -R -n -E '/SYNTH-WEB|add_subdirectory.*SYNTH-WEB' "$source_root/CMakeLists.txt" "$source_root/src"; then
  echo "SYNTH build depends on the participant source tree" >&2
  exit 1
fi
tar --list --file "$artifact" | grep -Fx './bin/synth-web' >/dev/null
echo "SYNTH_WEB_INDEPENDENT_BUILD       PASS"

test ! -e "$XDG_CONFIG_HOME/synth"
echo "EXISTING_CONFORMANCE              PASS"
echo "CTEST                              PASS"
