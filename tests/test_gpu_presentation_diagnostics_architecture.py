#!/usr/bin/env python3
"""Guard non-invasive GPU presentation timing instrumentation and KMS ordering."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "include/flip_gpu_context.h").read_text()
context = (ROOT / "src/runtime/flip_gpu_context.c").read_text()
service = (ROOT / "src/runtime/flip_gpu_service.c").read_text()
scanout = (ROOT / "src/runtime/flip_gpu_scanout.c").read_text()

for token in (
    "uint64_t draw_cpu_us;",
    "uint64_t present_us;",
    "flip_gpu_context_last_render_timing(",
):
    assert token in header, f"missing context metric ABI: {token}"

for token in (
    "gpu_monotonic_us(",
    "draw_started_us = gpu_monotonic_us()",
    "draw_finished_us = gpu_monotonic_us()",
    "present_finished_us = gpu_monotonic_us()",
    "context->last_render_timing.draw_cpu_us",
    "context->last_render_timing.present_us",
):
    assert token in context, f"missing separated GPU timing: {token}"

render = context[context.index("int flip_gpu_context_render("):]
assert render.index("draw_started_us = gpu_monotonic_us()") < render.index("context->glDrawArrays(")
assert render.index("context->glDrawArrays(") < render.index("draw_finished_us = gpu_monotonic_us()")
assert render.index("draw_finished_us = gpu_monotonic_us()") < render.index("context->eglSwapBuffers(")
assert render.index("context->eglSwapBuffers(") < render.index("flip_gpu_scanout_present(")
assert render.index("flip_gpu_scanout_present(") < render.index("present_finished_us = gpu_monotonic_us()")
assert "context->glFinish();" in render  # offscreen mode must preserve existing behavior

for token in (
    "draw_cpu_avg_us=%llu",
    "swap_present_avg_us=%llu",
    "swap_present_max_us=%llu",
    "render_avg_us=%llu",
    "render_max_us=%llu",
    "flip_gpu_context_last_render_timing(gpu, &stage_timing)",
):
    assert token in service, f"missing GPU-service summary/compatibility: {token}"

# Do not change the existing event-driven flip completion policy to busy-spinning
# or a guessed vblank delay merely to instrument timing.
for token in (
    "DRM_MODE_PAGE_FLIP_EVENT",
    "scanout->drmModePageFlip(",
    "scanout->drmHandleEvent(",
    "while (pending)",
    "scanout->gbm_surface_release_buffer(",
):
    assert token in scanout, f"scanout completion contract changed: {token}"

print("GPU submission/presentation timing architecture passed")
