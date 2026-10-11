#!/usr/bin/env python3
from pathlib import Path

source = Path("src/runtime/flip_video.c").read_text()

assert "#define FLIP_VIDEO_GPU_HOST_ENABLED 0" in source
assert "if (FLIP_VIDEO_GPU_HOST_ENABLED &&" in source
assert '"VIDEO: direct CPU/DRM path gpu_host=%s\\n"' in source

print("video Beta startup architecture passed")
