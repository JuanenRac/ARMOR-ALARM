#!/usr/bin/env python3
"""Check the messages ARMOR-ALARM's emit_samples prints against ARMOR-COMMON's published contract.

Usage:  emit_samples | python tests/check_samples.py

Each line is a topic and a JSON payload: the state messages (armor/alarm/<node>/state), the commands a server would send to the panel (.../command) and the results the
node answers (.../result). The firmware's serialiser is hand-written C++; this is the proof that what it prints is what the shared contract (schema, topic rule, ranges,
the rules of the whole message) accepts, and that a result answers a command it was given.
"""
import json
import sys
from pathlib import Path

COMMON = Path(__file__).resolve().parents[2] / "ARMOR-COMMON" / "src"
if COMMON.is_dir():
    sys.path.insert(0, str(COMMON))
    from armor_common import ContractError, validate_alarm_command, validate_alarm_message, validate_alarm_result  # noqa: E402
    VALIDATORS = {"state": validate_alarm_message, "command": validate_alarm_command, "result": validate_alarm_result}
else:
    # Honest degradation, not a vendored copy of the contract: ARMOR-COMMON only exists as a sibling checkout, which a single-repo CI checkout never has.
    # The firmware's own host tests still run and still gate the build either way.
    ContractError = None
    VALIDATORS = None


def main() -> int:
    if VALIDATORS is None:
        print(f"ALARM_MESSAGES=SKIPPED no sibling {COMMON} checkout (this check only runs where ARMOR-COMMON is a sibling repo)")
        return 0
    counts = {"state": 0, "command": 0, "result": 0}
    phases = set()
    refusals = set()
    sent = []
    for line in sys.stdin.read().splitlines():
        topic, _, body = line.partition(" ")
        leaf = topic.rsplit("/", 1)[-1]
        payload = json.loads(body)
        try:
            VALIDATORS[leaf](topic, payload)
        except ContractError as error:
            print(f"{topic} violates the contract: {error}\n  {body}", file=sys.stderr)
            return 1
        counts[leaf] += 1
        if leaf == "state":
            phases.add(payload["phase"])
        elif leaf == "command":
            sent.append(payload["command_id"])
        else:
            refusals.add(payload["refusal"])
            if payload["command_id"] not in sent:
                print(f"a result answers a command that was never sent: {body}", file=sys.stderr)
                return 1
    if counts["state"] < 6 or counts["command"] < 5 or counts["result"] < 5 or not {"disarmed", "exit_delay", "armed", "alarm", "entry_delay"} <= phases or not {"none", "zones_open", "not_disarmed"} <= refusals:
        print(f"expected states in every phase and commands accepted and refused in several ways, got {counts}, phases {sorted(phases)}, refusals {sorted(refusals)}", file=sys.stderr)
        return 1
    print(f"ALARM_MESSAGES=PASS {counts['state']} states, {counts['command']} commands and {counts['result']} results accepted by ARMOR-COMMON")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
