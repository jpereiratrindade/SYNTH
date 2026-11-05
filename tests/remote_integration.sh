#!/usr/bin/env bash
set -Eeuo pipefail
synth="$1"
source_root="$2"
root="$(mktemp -d)"
export XDG_CONFIG_HOME="$root/config" XDG_DATA_HOME="$root/data" XDG_STATE_HOME="$root/state" XDG_RUNTIME_DIR="$root/runtime"
export SYNTH_DATA_DIR="$source_root"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"

cat >"$root/manifest.json" <<'JSON'
{"$schema":"urn:synth:schema:remote-participant:0.1.0","schema_version":"0.1.0","identity":"remote-test","version":"1.0.0","description":"controlled remote","endpoint":"http://127.0.0.1:9999/","identity_assurance":"UNVERIFIED","verification_method":"http-content-marker","provides":{"surfaces":[{"id":"interface.human.web.v1","kind":"http","media_type":"text/html"}]}}
JSON
"$synth" remote register "$root/manifest.json" --json | grep -F '"status":"REGISTERED"' >/dev/null

cat >"$root/available.json" <<'JSON'
{"$schema":"urn:synth:schema:remote-observation:0.1.0","schema_version":"0.1.0","identity":"remote-test","endpoint":"http://127.0.0.1:9999/","availability":"AVAILABLE","identity_assurance":"UNVERIFIED","verification_method":"http-content-marker","observer":"controlled-test","observed_at":"2098-01-01T00:00:00Z","expires_at":"2099-01-01T00:00:00Z","detail":"controlled observation","observed_surfaces":[{"id":"interface.human.web.v1","owner":"remote-test","kind":"http","locator":"http://127.0.0.1:9999/","direction":"inbound","media_type":"text/html","observability":"remote-observer-attestation","metadata":"controlled"}]}
JSON
"$synth" remote attest "$root/available.json" --json | grep -F '"status":"AVAILABLE"' >/dev/null
"$synth" ecosystem --json >"$root/ecosystem.json"
python3 - "$root/ecosystem.json" "$source_root/schemas/ecosystem-projection.schema.json" <<'PY'
import json, pathlib, sys
from jsonschema.validators import validator_for
value=json.loads(pathlib.Path(sys.argv[1]).read_text())
schema=json.loads(pathlib.Path(sys.argv[2]).read_text())
validator_for(schema)(schema).validate(value)
p=next(p for p in value['participants'] if p['identity']=='remote-test')
assert p['state']=='AVAILABLE' and p['participant_kind']=='REMOTE'
assert p['identity_assurance']=='UNVERIFIED' and p['realization_id'] is None
provider=next(p for s in value['surfaces'] if s['id']=='interface.human.web.v1' for p in s['providers'] if p['identity']=='remote-test')
assert provider['state']=='AVAILABLE' and provider['identity_assurance']=='UNVERIFIED'
PY

python3 - "$root/available.json" "$root/expired.json" <<'PY'
import json, pathlib, sys
value=json.loads(pathlib.Path(sys.argv[1]).read_text())
value['observed_at']='2000-01-01T00:00:00Z'; value['expires_at']='2000-01-01T00:00:01Z'
pathlib.Path(sys.argv[2]).write_text(json.dumps(value))
PY
"$synth" remote attest "$root/expired.json" --json >/dev/null
"$synth" ecosystem --json | python3 -c 'import json,sys; p=next(x for x in json.load(sys.stdin)["participants"] if x["identity"]=="remote-test"); assert p["state"]=="REGISTERED" and not p["observed_surfaces"]'

python3 - "$root/available.json" "$root/mismatch.json" <<'PY'
import json, pathlib, sys
value=json.loads(pathlib.Path(sys.argv[1]).read_text()); value['endpoint']='http://127.0.0.1:9998/'
pathlib.Path(sys.argv[2]).write_text(json.dumps(value))
PY
if "$synth" remote attest "$root/mismatch.json" --json >/dev/null; then
  echo 'mismatched endpoint accepted' >&2; exit 1
fi
echo 'REMOTE_PARTICIPANT_MODEL PASS'
