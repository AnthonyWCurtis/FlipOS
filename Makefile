CROSS ?= aarch64-linux-gnu-

CC  := $(CROSS)gcc
CXX := $(CROSS)g++
HOSTCC ?= gcc

CPPFLAGS := -Iinclude
CFLAGS   := -O2 -Wall -Wextra -MMD -MP
CXXFLAGS := -O2 -Wall -Wextra -MMD -MP
TELEMETRY_WARN := -Wno-format-truncation

BUILD := build

COMMON_OBJS := \
	$(BUILD)/flip_audio.o \
	$(BUILD)/flip_audio_buffer.o \
	$(BUILD)/flip_audio_resampler.o \
	$(BUILD)/flip_settings.o \
	$(BUILD)/flip_hotkeys.o \
	$(BUILD)/flip_osd.o \
	$(BUILD)/flip_library.o \
	$(BUILD)/flip_input.o \
	$(BUILD)/flip_controls.o \
	$(BUILD)/flip_drm.o \
	$(BUILD)/flip_draw.o

RUNTIME_OBJS := \
	$(BUILD)/flip_session_adapter.o \
	$(BUILD)/flip_system.o \
	$(BUILD)/flip_session.o \
	$(BUILD)/flip_power.o

FRONTEND_OBJS := \
	$(BUILD)/flip_frontend.o \
	$(RUNTIME_OBJS) \
	$(COMMON_OBJS)

GB_LIBRETRO_OBJS := \
	$(BUILD)/libretro_file_stream.o \
	$(BUILD)/libretro_file_path.o \
	$(BUILD)/libretro_file_path_io.o \
	$(BUILD)/libretro_stdstring.o \
	$(BUILD)/libretro_encoding_utf.o \
	$(BUILD)/libretro_compat_strl.o \
	$(BUILD)/libretro_rtime.o \
	$(BUILD)/libretro_vfs.o

GB_OBJS := \
	$(BUILD)/flip_gb.o \
	$(BUILD)/flip_gambatte_core.o \
	$(BUILD)/flip_core.o \
	$(BUILD)/flip_core_host.o \
	$(BUILD)/flip_state.o \
	$(BUILD)/flip_perf_diag.o \
	$(BUILD)/flip_telemetry.o \
	$(BUILD)/flip_audio.o \
	$(BUILD)/flip_audio_buffer.o \
	$(BUILD)/flip_audio_resampler.o \
	$(BUILD)/flip_settings.o \
	$(BUILD)/flip_hotkeys.o \
	$(BUILD)/flip_osd.o \
	$(BUILD)/flip_video.o \
	$(BUILD)/flip_power.o \
	$(BUILD)/flip_input.o \
	$(BUILD)/flip_controls.o \
	$(BUILD)/flip_timing.o \
	$(BUILD)/flip_lifecycle.o \
	$(BUILD)/flip_drm.o \
	$(BUILD)/flip_draw.o \
	$(GB_LIBRETRO_OBJS)

DEPS := $(wildcard $(BUILD)/*.d)

.PHONY: all frontend gb test-controls test-timing test-audio test-audio-resampler test-lifecycle test-core test-core-host test-state test-perf-diag test-telemetry test-architecture clean clean-objects

all: frontend gb

frontend: $(BUILD)/flip-frontend

gb: $(BUILD)/flip-gb

test-controls: $(BUILD)/test-flip-controls
	$(BUILD)/test-flip-controls

test-timing: $(BUILD)/test-flip-timing
	$(BUILD)/test-flip-timing

test-audio: $(BUILD)/test-flip-audio
	$(BUILD)/test-flip-audio

test-audio-resampler: $(BUILD)/test-flip-audio-resampler
	$(BUILD)/test-flip-audio-resampler

test-lifecycle: $(BUILD)/test-flip-lifecycle
	$(BUILD)/test-flip-lifecycle

test-core: $(BUILD)/test-flip-core
	$(BUILD)/test-flip-core

test-core-host: $(BUILD)/test-flip-core-host
	$(BUILD)/test-flip-core-host

test-state: $(BUILD)/test-flip-state
	$(BUILD)/test-flip-state

test-perf-diag: $(BUILD)/test-flip-perf-diag
	$(BUILD)/test-flip-perf-diag

test-telemetry: $(BUILD)/test-flip-telemetry
	$(BUILD)/test-flip-telemetry

test-architecture:
	python3 tests/test_flip_gb_architecture.py
	python3 tests/test_session_adapter_architecture.py
	python3 tests/test_core_host_diagnostics_architecture.py
	python3 tests/test_audio_callback_diagnostics_architecture.py
	python3 tests/test_audio_rate_conversion_architecture.py
	python3 tests/test_state_manager_architecture.py

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test-flip-controls: tests/test_flip_controls.c src/runtime/flip_controls.c include/flip_controls.h include/flip_input.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_controls.c src/runtime/flip_controls.c \
		-o $@

$(BUILD)/test-flip-timing: tests/test_flip_timing.c src/runtime/flip_timing.c include/flip_timing.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_timing.c src/runtime/flip_timing.c \
		-o $@

$(BUILD)/test-flip-audio: tests/test_flip_audio_buffer.c src/audio/flip_audio_buffer.c src/audio/flip_audio_buffer.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Isrc/audio \
		tests/test_flip_audio_buffer.c src/audio/flip_audio_buffer.c \
		-o $@

$(BUILD)/test-flip-audio-resampler: tests/test_flip_audio_resampler.c src/audio/flip_audio_resampler.c include/flip_audio_resampler.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_audio_resampler.c src/audio/flip_audio_resampler.c \
		-o $@

$(BUILD)/test-flip-lifecycle: tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_timing.c include/flip_lifecycle.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_timing.c \
		-o $@

$(BUILD)/test-flip-core: tests/test_flip_core.c src/runtime/flip_core.c include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_core.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-core-host: tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c src/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c include/flip_core_host.h include/flip_perf_diag.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c \
		src/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c \
		-o $@

$(BUILD)/test-flip-state: tests/test_flip_state.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_STATE_ROOT='"/tmp/flipos-state-test"' -Iinclude \
		tests/test_flip_state.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-perf-diag: tests/test_flip_perf_diag.c src/runtime/flip_perf_diag.c include/flip_perf_diag.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_perf_diag.c src/runtime/flip_perf_diag.c \
		-o $@

$(BUILD)/test-flip-telemetry: tests/test_flip_telemetry.c src/runtime/flip_telemetry.c include/flip_telemetry.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror $(TELEMETRY_WARN) -Iinclude \
		tests/test_flip_telemetry.c src/runtime/flip_telemetry.c \
		-o $@ -lm

$(BUILD)/flip_frontend.o: src/frontend/flip_frontend.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_audio.o: src/audio/flip_audio.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_audio_buffer.o: src/audio/flip_audio_buffer.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_audio_resampler.o: src/audio/flip_audio_resampler.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_input.o: src/runtime/flip_input.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_controls.o: src/runtime/flip_controls.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_core.o: src/runtime/flip_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_core_host.o: src/runtime/flip_core_host.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_state.o: src/runtime/flip_state.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_perf_diag.o: src/runtime/flip_perf_diag.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_telemetry.o: src/runtime/flip_telemetry.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TELEMETRY_WARN) -c $< -o $@

$(BUILD)/flip_timing.o: src/runtime/flip_timing.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_lifecycle.o: src/runtime/flip_lifecycle.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_drm.o: src/hwtest/flip_drm.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_draw.o: src/hwtest/flip_draw.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_power.o: src/runtime/flip_power.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_settings.o: src/runtime/flip_settings.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_hotkeys.o: src/runtime/flip_hotkeys.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_osd.o: src/runtime/flip_osd.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_video.o: src/runtime/flip_video.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_library.o: src/runtime/flip_library.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session.o: src/runtime/flip_session.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_system.o: src/runtime/flip_system.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_adapter.o: src/runtime/flip_session_adapter.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gambatte_core.o: src/emulators/flip_gambatte_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gb.o: src/emulators/flip_gb.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip-frontend: $(FRONTEND_OBJS)
	$(CC) -static -o $@ $^ -lm

$(BUILD)/flip-gb: $(GB_OBJS) $(BUILD)/libgambatte_libretro.a
	$(CXX) -static -o $@ \
		$(GB_OBJS) \
		$(BUILD)/libgambatte_libretro.a \
		-pthread \
		-lm

clean-objects:
	rm -f $(BUILD)/*.o $(BUILD)/*.d

clean:
	rm -f \
		$(BUILD)/*.o \
		$(BUILD)/*.d \
		$(BUILD)/flip-frontend \
		$(BUILD)/flip-gb \
		$(BUILD)/test-flip-controls \
		$(BUILD)/test-flip-timing \
		$(BUILD)/test-flip-audio \
		$(BUILD)/test-flip-audio-resampler \
		$(BUILD)/test-flip-lifecycle \
		$(BUILD)/test-flip-core \
		$(BUILD)/test-flip-core-host \
		$(BUILD)/test-flip-state \
		$(BUILD)/test-flip-perf-diag \
		$(BUILD)/test-flip-telemetry

-include $(DEPS)

$(BUILD)/libretro_file_stream.o: external/gambatte-libretro/libgambatte/libretro-common/streams/file_stream.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_file_path.o: external/gambatte-libretro/libgambatte/libretro-common/file/file_path.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_file_path_io.o: external/gambatte-libretro/libgambatte/libretro-common/file/file_path_io.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_stdstring.o: external/gambatte-libretro/libgambatte/libretro-common/string/stdstring.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_encoding_utf.o: external/gambatte-libretro/libgambatte/libretro-common/encodings/encoding_utf.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_compat_strl.o: external/gambatte-libretro/libgambatte/libretro-common/compat/compat_strl.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_rtime.o: external/gambatte-libretro/libgambatte/libretro-common/time/rtime.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@

$(BUILD)/libretro_vfs.o: external/gambatte-libretro/libgambatte/libretro-common/vfs/vfs_implementation.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iexternal/gambatte-libretro/libgambatte/libretro-common/include -c $< -o $@
