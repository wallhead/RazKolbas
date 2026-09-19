"""Run the real NR experiment in bounded child processes; requires local exact DLLs.

This is opt-in runtime validation, not a CI mock or a Skyrim test. Preserves the
original failure, tests post-init exception/retirement failures, then positive creation.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--nr", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    base = [str(args.executable.resolve(strict=True)), str(args.nr.resolve(strict=True))]
    feature = ["--caller-shim", "--feature-core", str(args.core.resolve(strict=True))]
    cases = [
        ("original", [], None, 5, ["RAW_INIT_RESULT=0xbad00002"], ["FEATURE_CREATION_AND_RETIREMENT=PASS"]),
        ("exception", feature, "throw_after_init", 10,
         ["RAW_INIT_RESULT=0x00000001", "FEATURE_EXPERIMENT_EXCEPTION=INJECTED_AFTER_INIT"],
         ["FEATURE_CREATION_AND_RETIREMENT=PASS", "PATCH_RESTORED"]),
        ("missing-release", feature, "omit_one_release", 10,
         ["INJECTED_OMITTED_RELEASE", "FEATURE_EXPERIMENT_STOP=RESOURCE_CALLBACK_RETIREMENT_UNBALANCED"],
         ["FEATURE_CREATION_AND_RETIREMENT=PASS", "PATCH_RESTORED"]),
        ("create", feature, None, 0,
         ["RAW_FEATURE_CREATE=0x1; handle_nonnull=1", "FEATURE_CREATION_AND_RETIREMENT=PASS",
          "RAW_PARAMETER_DESTROY=0x1", "RAW_SHUTDOWN_RESULT=0x1", "PATCH_RESTORED=nr.caller-name.experiment"],
         ["INJECTED_", "FEATURE_EXPERIMENT_STOP", "FEATURE_EXPERIMENT_EXCEPTION"]),
    ]
    args.output.mkdir(parents=True, exist_ok=True)
    failures = 0
    for name, extra, fault, expected_exit, required, forbidden in cases:
        environment = dict(os.environ)
        environment.pop("RAZKOLBAS_NR_PROBE_FAULT", None)
        if fault:
            environment["RAZKOLBAS_NR_PROBE_FAULT"] = fault
        try:
            result = subprocess.run(base + extra, capture_output=True, text=True,
                                    env=environment, timeout=45)
            log = result.stdout + result.stderr
            passed = (result.returncode == expected_exit and all(x in log for x in required)
                      and not any(x in log for x in forbidden))
            log += f"\nPROCESS_EXIT={result.returncode}; EXPECTED_EXIT={expected_exit}\n"
        except subprocess.TimeoutExpired as error:
            passed = False
            log = (error.stdout or b"").decode(errors="replace")
            log += (error.stderr or b"").decode(errors="replace") + "\nTIMEOUT=45s; child terminated\n"
        (args.output / (name + ".txt")).write_text(log, encoding="utf-8")
        print(f"{name}: {'PASS' if passed else 'FAIL'}", flush=True)
        failures += not passed
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
