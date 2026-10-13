#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
video = (ROOT / "src/runtime/flip_video.c").read_text()
client = (ROOT / "src/runtime/flip_gpu_client.c").read_text()
service = (ROOT / "src/runtime/flip_gpu_service.c").read_text()
protocol = (ROOT / "include/flip_gpu_protocol.h").read_text()

assert "#define FLIP_VIDEO_GPU_HOST_ENABLED 1" in video
assert "if (FLIP_VIDEO_GPU_HOST_ENABLED &&" in video
assert "VIDEO: opened shared GPU host service=%s" in video
assert "VIDEO: direct CPU/DRM path gpu_host=%s" in video

for token in [
    'FLIP_GPU_READY_FD_ENV "FLIP_GPU_READY_FD"',
    "FLIP_GPU_READY_BYTE",
]:
    assert token in protocol, f"GPU readiness protocol missing: {token}"

for token in [
    "wait_for_service_ready(",
    "connect_ready_service(",
    "pipe(ready_pipe)",
    "FLIP_GPU_CLIENT: service ready",
    "#define FLIP_GPU_READY_POLL_MS 100",
    "#define FLIP_GPU_READY_TIMEOUT_MS 15000",
]:
    assert token in client, f"GPU client readiness handoff missing: {token}"

for token in [
    "signal_parent_ready(",
    "FLIP_GPU_READY_FD_ENV",
    "FLIP_GPU_READY_BYTE",
]:
    assert token in service, f"GPU service readiness handoff missing: {token}"

print("shared GPU startup architecture passed")
