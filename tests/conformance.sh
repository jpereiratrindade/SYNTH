#!/usr/bin/env bash
set -euo pipefail

synth_bin="$1"
source_root="$2"
test_root="$(mktemp -d)"
trap 'rm -rf "$test_root"' EXIT

export XDG_CONFIG_HOME="$test_root/config"
export XDG_STATE_HOME="$test_root/state"
export XDG_RUNTIME_DIR="$test_root/runtime"

"$synth_bin" version | grep -F "SYNTH 0.1.0"
"$synth_bin" status | grep -F "SYSTEM_SYNTH_READY     PASS"
"$synth_bin" foundation verify | grep -F "NO_FAKE_RELATIONS"
"$synth_bin" surfaces | grep -F "synth.evidence"
"$synth_bin" relations | grep -Fx "No relations observed."
"$synth_bin" evidence --json | grep -F '"epistemic_class": "OBSERVED"'
"$synth_bin" status --json | grep -F '"ECOSYSTEM_SYNTH":"NOT_YET_APPLICABLE"'

test -f "$XDG_CONFIG_HOME/synth/system.conf"
test -f "$XDG_STATE_HOME/synth/evidence/latest.json"
test -d "$XDG_RUNTIME_DIR/synth"

before="$(find "$test_root" -printf '%P\n' | sort)"
"$synth_bin" status >/dev/null
after="$(find "$test_root" -printf '%P\n' | sort)"
test "$before" = "$after"

grep -F 'context-metadata+json' "$source_root/docs/SYNTH-FOUNDATION-001-v0.4.0.md" >/dev/null
echo "All foundational conformance checks passed."
