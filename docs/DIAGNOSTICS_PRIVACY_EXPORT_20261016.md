# Off-device metrics-only diagnostic export

16 October 2026. Host-side engineering candidate; not installed on Miyoo Flip.

## Purpose

The published FlipOS launcher writes diagnostics into the device's frontend.log. Raw logs may contain ROM names, paths and other private information. The helper tools/diagnostics/flip_diagnostics_export.py extracts only strictly allowlisted numeric metrics into a local JSON file, with a count of error/failure-marker lines. No free-form log text or arbitrary key/value field is copied.

## Local usage (Python 3 on the developer PC)

After the user has chosen to copy the log from their own device, run:

    python3 tools/diagnostics/flip_diagnostics_export.py --input /path/to/frontend.log --output ./flipos-report.json --consent

Then open and review flipos-report.json before deciding whether to share it. There is no upload, telemetry server, background collection or automatic sending. The --consent flag is required for every invocation.

## Boundary and safeguards

- Runs on the developer host, not the Miyoo firmware; does not add a device service or modify src/runtime/flip_log.c.
- Only explicitly selected filenames frontend.log, frontend.previous.log, gpu.log, emulator.log, battery.log and error.log may be read.
- Inputs are opened read-only, without following final-component symlinks. Non-regular, FIFO and oversized inputs fail. At most six logs, final 256 KiB per log, 60 metric events per source, and 128 KiB of JSON output.
- Only numeric fields from FLIP_PERF, FLIP_CORE_HOST_FPS, FLIP_AUDIO_PERF, FLIP_GPU_SERVICE_PERF, FLIP_CPU_DYNAMIC and FLIP_TELEMETRY are included. Their names and numeric values are validated; unrecognised text is discarded.
- A report is written only to a new path, never over an existing output, with owner-only file permissions on Unix.
- Counts are approximate and may be affected by truncation. Raw stack traces, configuration details, ROM names and Wi-Fi diagnostics are deliberately absent.
- This minimises exposure but is not a guarantee of anonymity. Review any output before sharing.

## Verification and remaining acceptance

Run:

    python3 -m unittest discover -s tests/diagnostics -p 'test_*.py' -v

The tests use only disposable synthetic input files. The GitHub PR CI runs them alongside the regular FlipOS verification. A future on-device opt-in Send Logs feature would require a separate consent interface, in-device bounded logging, preview, revocation policy, transport security, backend design, actual-device acceptance and explicit authorisation before upload.
