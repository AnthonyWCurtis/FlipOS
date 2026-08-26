from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DRM_H = (ROOT / "include" / "flip_drm.h").read_text()
DRM_C = (ROOT / "src" / "hwtest" / "flip_drm.c").read_text()
VIDEO_C = (ROOT / "src" / "runtime" / "flip_video.c").read_text()


def require(text, needle, label):
    assert needle in text, f"missing {label}: {needle}"


def main():
    require(DRM_H, "last_present_us", "DRM present total timing field")
    require(DRM_H, "last_flip_ioctl_us", "DRM page-flip ioctl timing field")
    require(DRM_H, "last_flip_wait_us", "DRM flip wait timing field")
    require(DRM_H, "last_flip_read_us", "DRM event read timing field")

    require(DRM_C, "drm->last_present_us", "DRM total presentation measurement")
    require(DRM_C, "drm->last_flip_ioctl_us", "DRM ioctl measurement")
    require(DRM_C, "drm->last_flip_wait_us", "DRM wait measurement")
    require(DRM_C, "drm->last_flip_read_us", "DRM event-read measurement")

    require(VIDEO_C, "FLIP_VIDEO_PERF:", "video performance summary")
    require(VIDEO_C, "compose_avg_us=", "composition timing summary")
    require(VIDEO_C, "present_avg_us=", "present timing summary")
    require(VIDEO_C, "flip_ioctl_avg_us=", "page-flip ioctl timing summary")
    require(VIDEO_C, "flip_wait_avg_us=", "page-flip wait timing summary")
    require(VIDEO_C, "flip_read_avg_us=", "page-flip event-read timing summary")

    print("video present diagnostics architecture tests passed")


if __name__ == "__main__":
    main()
