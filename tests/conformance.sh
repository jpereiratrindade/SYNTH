#!/usr/bin/env bash
set -euo pipefail

synth_bin="$1"
source_root="$2"
build_root="$3"
test_root="$(mktemp -d)"
trap 'rm -rf "$test_root"' EXIT

export XDG_CONFIG_HOME="$test_root/config"
export XDG_STATE_HOME="$test_root/state"
export XDG_RUNTIME_DIR="$test_root/runtime"

"$synth_bin" version | grep -Fx "SYNTH 0.1.0"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS"
"$synth_bin" foundation verify | grep -F "FOUNDATION_VERIFY"
"$synth_bin" foundation verify --json | grep -F '"epistemic_class":"DERIVED"'
"$synth_bin" surfaces | grep -F "synth.cli"
"$synth_bin" surfaces | grep -F "synth.evidence"
if "$synth_bin" surfaces | grep -F "synth.web"; then
  echo "unobserved web surface was reported" >&2
  exit 1
fi
"$synth_bin" relations | grep -Fx "No relations observed."
"$synth_bin" evidence --json | grep -F '"epistemic_class": "OBSERVED"'
"$synth_bin" evidence --json | grep -F '"rss_kib"'
"$synth_bin" evidence --json | grep -F '"max_rss_kib"'
"$synth_bin" status --json | grep -F '"ECOSYSTEM_SYNTH":"NOT_YET_APPLICABLE"'

test -f "$XDG_CONFIG_HOME/synth/system.conf"
test -f "$XDG_STATE_HOME/synth/evidence/latest.json"
test -d "$XDG_RUNTIME_DIR/synth"

# A second execution may create fresh evidence, but cannot change configuration
# or the realized filesystem shape.
realization_before="$(find "$XDG_CONFIG_HOME" "$XDG_RUNTIME_DIR" -printf '%P %y\n' | sort)"
config_before="$(sha256sum "$XDG_CONFIG_HOME/synth/system.conf")"
evidence_before="$(sha256sum "$XDG_STATE_HOME/synth/evidence/latest.json")"
"$synth_bin" status >/dev/null
realization_after="$(find "$XDG_CONFIG_HOME" "$XDG_RUNTIME_DIR" -printf '%P %y\n' | sort)"
config_after="$(sha256sum "$XDG_CONFIG_HOME/synth/system.conf")"
evidence_after="$(sha256sum "$XDG_STATE_HOME/synth/evidence/latest.json")"
test "$realization_before" = "$realization_after"
test "$config_before" = "$config_after"
test "$evidence_before" != "$evidence_after"

# The installed realization must carry its own normative data and work away
# from the checkout without SYNTH_DATA_DIR.
install_root="$test_root/install"
cmake --install "$build_root" --prefix "$install_root" >/dev/null
installed_synth="$install_root/bin/synth"
test -x "$installed_synth"
test -f "$install_root/share/synth/SYNTH-FOUNDATION-001-v0.4.0.md"
test -f "$install_root/share/synth/schemas/relation.schema.json"
if strings "$installed_synth" | grep -F "$source_root"; then
  echo "installed binary contains a source-tree dependency" >&2
  exit 1
fi

unset SYNTH_DATA_DIR
export XDG_CONFIG_HOME="$test_root/installed-config"
export XDG_STATE_HOME="$test_root/installed-state"
export XDG_RUNTIME_DIR="$test_root/installed-runtime"
(
  cd "$test_root"
  "$installed_synth" foundation verify | grep -F "CONTEXTLAB_DOCUMENT_COMPAT"
  "$installed_synth" status | grep -F "SYSTEM_SYNTH_READY     PASS"
)

# Prove that ContextLab compatibility is parsed rather than accepted by string
# presence: a syntactically valid but incorrect identity must fail the gate.
sed -i 's/"id": "SYNTH-FOUNDATION-001"/"id": "BROKEN-FOUNDATION"/' \
  "$install_root/share/synth/SYNTH-FOUNDATION-001-v0.4.0.md"
context_failure="$("$installed_synth" foundation verify 2>&1 || true)"
printf '%s\n' "$context_failure" | grep -E "CONTEXTLAB_DOCUMENT_COMPAT[[:space:]]+FAIL"
if "$installed_synth" foundation verify >/dev/null 2>&1; then
  echo "invalid ContextLab metadata was accepted" >&2
  exit 1
fi

echo "All foundational conformance checks passed."
