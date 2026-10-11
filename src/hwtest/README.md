# FlipOS Hardware / Validation Tools

Low-level Miyoo Flip / RK3566 development helpers used for hardware bring-up,
performance capture, Wi-Fi diagnostics, and CPU Optimizer safety/recovery work.

These sources are not a separate end-user application. The active tools are
built explicitly by the top-level Makefile.

## Active tooling

- `flip_cpu_policy_tool.c` — CPU policy inspection/control helper.
- `flip_optimizer_guard_tool.c` — optimizer recovery guard.
- `flip_optimizer_supervisor_tool.c` — optimizer recovery supervisor.
- `flip_optimizer_deadman.c` — userspace deadman.
- `flip_optimizer_calibration_tool.c` — guarded voltage calibration.
- `flip_optimizer_candidate_tool.c` — candidate stress/validation runner.
- `flip_optimizer_progress.c` — optimizer progress display.
- `flip_optimizer_validation_progress.c` — final validation progress display.
- `flip_optimizer_runtime_tool.c` — runtime profile helper.
- `flip_optimizer_session_tool.c` — optimizer session controller.
- `flip_wifi_tool.c` — Wi-Fi diagnostic/control helper.
- `flip_perf_capture.c` — performance capture support.
- `flip_drm.c` / `flip_draw.c` — shared direct DRM/KMS drawing support.

## Target

- Miyoo Flip
- Rockchip RK3566
- AArch64 Linux
- 640x480 display

Hardware-critical optimizer tools are intentionally kept separate from the
normal frontend/runtime code so recovery and validation paths can remain small,
auditable, and independently testable.
