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
	$(BUILD)/flip_paths.o \
	$(BUILD)/flip_log.o \
	$(BUILD)/flip_exit_protocol.o \
	$(BUILD)/flip_session_registry.o \
	$(BUILD)/flip_session_adapter.o \
	$(BUILD)/flip_system.o \
	$(BUILD)/flip_session.o \
	$(BUILD)/flip_recovery.o \
	$(BUILD)/flip_save.o \
	$(BUILD)/flip_state.o \
	$(BUILD)/flip_core.o \
	$(BUILD)/flip_power.o

FRONTEND_OBJS := \
	$(BUILD)/flip_frontend.o \
	$(BUILD)/flip_save_safety_ui.o \
	$(BUILD)/flip_save_safety_launch.o \
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
	$(BUILD)/flip_bios.o \
	$(BUILD)/flip_paths.o \
	$(BUILD)/flip_log.o \
	$(BUILD)/flip_core.o \
	$(BUILD)/flip_core_host.o \
	$(BUILD)/flip_state.o \
	$(BUILD)/flip_save.o \
	$(BUILD)/flip_recovery.o \
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

.PHONY: all frontend gb verify test-controls test-timing test-audio test-audio-resampler test-paths test-log test-bios test-settings test-osd test-power test-library-cache test-exit-protocol test-lifecycle test-core test-core-host test-state test-state-manager test-state-recovery-health test-save test-save-manager test-recovery test-session-registry test-session-save-safety test-perf-diag test-telemetry test-architecture clean clean-objects

all: frontend gb

frontend: $(BUILD)/flip-frontend

gb: $(BUILD)/flip-gb

verify:
	@log=/tmp/flipos-verify.log; \
	if ( \
		$(MAKE) test-controls && \
		$(MAKE) test-timing && \
		$(MAKE) test-audio && \
		$(MAKE) test-audio-resampler && \
		$(MAKE) test-paths && \
		$(MAKE) test-log && \
		$(MAKE) test-bios && \
		$(MAKE) test-settings && \
		$(MAKE) test-osd && \
		$(MAKE) test-power && \
		$(MAKE) test-library-cache && \
		$(MAKE) test-exit-protocol && \
		$(MAKE) test-lifecycle && \
		$(MAKE) test-core && \
		$(MAKE) test-core-host && \
		$(MAKE) test-state && \
		$(MAKE) test-state-manager && \
		$(MAKE) test-state-recovery-health && \
		$(MAKE) test-save && \
		$(MAKE) test-save-manager && \
		$(MAKE) test-recovery && \
		$(MAKE) test-session-registry && \
		$(MAKE) test-session-save-safety && \
		$(MAKE) test-perf-diag && \
		$(MAKE) test-telemetry && \
		$(MAKE) test-architecture && \
		$(MAKE) all && \
		git diff --check \
	) >$$log 2>&1; then \
		echo "VERIFY: PASS"; \
	else \
		echo "VERIFY: FAIL"; \
		tail -40 $$log; \
		exit 1; \
	fi

test-controls: $(BUILD)/test-flip-controls
	$(BUILD)/test-flip-controls

test-timing: $(BUILD)/test-flip-timing
	$(BUILD)/test-flip-timing

test-audio: $(BUILD)/test-flip-audio
	$(BUILD)/test-flip-audio

test-audio-resampler: $(BUILD)/test-flip-audio-resampler
	$(BUILD)/test-flip-audio-resampler

test-paths: $(BUILD)/test-flip-paths
	$(BUILD)/test-flip-paths

test-log: $(BUILD)/test-flip-log
	$(BUILD)/test-flip-log

test-bios: $(BUILD)/test-flip-bios
	$(BUILD)/test-flip-bios

test-settings: $(BUILD)/test-flip-settings
	$(BUILD)/test-flip-settings

test-osd: $(BUILD)/test-flip-osd
	$(BUILD)/test-flip-osd

test-power: $(BUILD)/test-flip-power
	$(BUILD)/test-flip-power

test-library-cache: $(BUILD)/test-flip-library-cache
	$(BUILD)/test-flip-library-cache

test-exit-protocol: $(BUILD)/test-flip-exit-protocol
	$(BUILD)/test-flip-exit-protocol

test-lifecycle: $(BUILD)/test-flip-lifecycle
	$(BUILD)/test-flip-lifecycle

test-core: $(BUILD)/test-flip-core
	$(BUILD)/test-flip-core

test-core-host: $(BUILD)/test-flip-core-host
	$(BUILD)/test-flip-core-host

test-state: $(BUILD)/test-flip-state
	$(BUILD)/test-flip-state

test-state-manager: $(BUILD)/test-flip-state-manager
	$(BUILD)/test-flip-state-manager

test-state-recovery-health: $(BUILD)/test-flip-state-recovery-health
	$(BUILD)/test-flip-state-recovery-health

test-save: $(BUILD)/test-flip-save
	$(BUILD)/test-flip-save

test-save-manager: $(BUILD)/test-flip-save-manager
	$(BUILD)/test-flip-save-manager

test-recovery: $(BUILD)/test-flip-recovery $(BUILD)/test-flip-recovery-transactions
	$(BUILD)/test-flip-recovery
	$(BUILD)/test-flip-recovery-transactions

test-session-registry: $(BUILD)/test-flip-session-registry
	$(BUILD)/test-flip-session-registry

test-session-save-safety: $(BUILD)/test-flip-session-adapter-save-safety
	$(BUILD)/test-flip-session-adapter-save-safety

test-perf-diag: $(BUILD)/test-flip-perf-diag
	$(BUILD)/test-flip-perf-diag

test-telemetry: $(BUILD)/test-flip-telemetry
	$(BUILD)/test-flip-telemetry

test-architecture:
	python3 tests/test_flip_gb_architecture.py
	python3 tests/test_session_adapter_architecture.py
	python3 tests/test_paths_architecture.py
	python3 tests/test_logging_architecture.py
	python3 tests/test_bios_architecture.py
	python3 tests/test_settings_architecture.py
	python3 tests/test_osd_architecture.py
	python3 tests/test_power_architecture.py
	python3 tests/test_library_cache_architecture.py
	python3 tests/test_save_manager_architecture.py
	python3 tests/test_exit_protocol_architecture.py
	python3 tests/test_session_registry_architecture.py
	python3 tests/test_core_host_diagnostics_architecture.py
	python3 tests/test_audio_callback_diagnostics_architecture.py
	python3 tests/test_audio_rate_conversion_architecture.py
	python3 tests/test_state_manager_architecture.py
	python3 tests/test_gambatte_save_memory_architecture.py
	python3 tests/test_recovery_architecture.py
	python3 tests/test_frontend_save_safety_architecture.py
	python3 tests/test_save_safety_management_architecture.py
	python3 tests/test_verify_target_architecture.py

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test-flip-paths: tests/test_flip_paths.c src/runtime/flip_paths.c include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_paths.c src/runtime/flip_paths.c \
		-o $@

$(BUILD)/test-flip-log: tests/test_flip_log.c src/runtime/flip_log.c src/runtime/flip_paths.c include/flip_log.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_LOG_ROOT='"/tmp/flipos-log-service-test/logs"' -Iinclude \
		tests/test_flip_log.c src/runtime/flip_log.c src/runtime/flip_paths.c \
		-o $@

$(BUILD)/test-flip-bios: tests/test_flip_bios.c src/runtime/flip_bios.c src/runtime/flip_paths.c include/flip_bios.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_SYSTEM_ROOT='"/tmp/flipos-bios-manager-test"' -Iinclude \
		tests/test_flip_bios.c src/runtime/flip_bios.c src/runtime/flip_paths.c \
		-o $@

$(BUILD)/test-flip-settings: tests/test_flip_settings.c src/runtime/flip_settings.c include/flip_settings.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_POSIX_C_SOURCE=200809L 		-DFLIP_STORAGE_ROOT='"/tmp/flipos-settings-service-test"' -Iinclude 		tests/test_flip_settings.c src/runtime/flip_settings.c 		-o $@

$(BUILD)/test-flip-osd: tests/test_flip_osd.c tests/test_flip_osd_draw_stubs.c src/runtime/flip_osd.c include/flip_osd.h include/flip_draw.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_POSIX_C_SOURCE=200809L -Iinclude 		tests/test_flip_osd.c tests/test_flip_osd_draw_stubs.c src/runtime/flip_osd.c 		-o $@

$(BUILD)/test-flip-power: tests/test_flip_power.c src/runtime/flip_power.c include/flip_power.h include/flip_input.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L 		-DFLIP_BACKLIGHT_BRIGHTNESS='"/tmp/flipos-power-service-test/brightness"' 		-Iinclude tests/test_flip_power.c src/runtime/flip_power.c 		-o $@

$(BUILD)/test-flip-library-cache: tests/test_flip_library_cache.c src/runtime/flip_library.c include/flip_library.h include/flip_paths.h include/flip_system.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_POSIX_C_SOURCE=200809L 		-DFLIP_STORAGE_ROOT='"/tmp/flipos-library-service-test"' -Iinclude 		tests/test_flip_library_cache.c src/runtime/flip_library.c 		-o $@

$(BUILD)/test-flip-exit-protocol: tests/test_flip_exit_protocol.c src/runtime/flip_exit_protocol.c include/flip_exit_protocol.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_exit_protocol.c src/runtime/flip_exit_protocol.c \
		-o $@

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

$(BUILD)/test-flip-lifecycle: tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_log.c src/runtime/flip_paths.c src/runtime/flip_timing.c include/flip_lifecycle.h include/flip_log.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_log.c src/runtime/flip_paths.c src/runtime/flip_timing.c \
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

$(BUILD)/test-flip-state-manager: tests/test_flip_state_manager.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_POSIX_C_SOURCE=200809L 		-DFLIP_STATE_ROOT='"/tmp/flipos-state-manager-test"' -Iinclude 		tests/test_flip_state_manager.c src/runtime/flip_state.c src/runtime/flip_core.c 		-o $@

$(BUILD)/test-flip-state-recovery-health: tests/test_flip_state_recovery_health.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_STATE_ROOT='"/tmp/flipos-state-test"' -Iinclude \
		tests/test_flip_state_recovery_health.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-save: tests/test_flip_save.c src/runtime/flip_save.c src/runtime/flip_core.c include/flip_save.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_SAVE_ROOT='"/tmp/flipos-save-test"' -Iinclude \
		tests/test_flip_save.c src/runtime/flip_save.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-save-manager: tests/test_flip_save_manager.c src/runtime/flip_save.c src/runtime/flip_core.c include/flip_save.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_SAVE_ROOT='"/tmp/flipos-save-manager-test"' -Iinclude \
		tests/test_flip_save_manager.c src/runtime/flip_save.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-recovery: tests/test_flip_recovery.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_recovery.h include/flip_save.h include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_SAVE_ROOT='"/tmp/flipos-save-test"' \
		-DFLIP_STATE_ROOT='"/tmp/flipos-state-test"' \
		-DFLIP_SESSION_RECOVERY_ROOT='"/tmp/flipos-recovery-test"' -Iinclude \
		tests/test_flip_recovery.c src/runtime/flip_recovery.c \
		src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-recovery-transactions: tests/test_flip_recovery_transactions.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_recovery.h include/flip_save.h include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_SAVE_ROOT='"/tmp/flipos-save-test"' \
		-DFLIP_STATE_ROOT='"/tmp/flipos-state-test"' \
		-DFLIP_SESSION_RECOVERY_ROOT='"/tmp/flipos-recovery-test"' -Iinclude \
		tests/test_flip_recovery_transactions.c src/runtime/flip_recovery.c \
		src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-session-adapter-save-safety: tests/test_flip_session_adapter_save_safety.c src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_session_adapter.h include/flip_recovery.h include/flip_save.h include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_session_adapter_save_safety.c \
		src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c \
		src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-session-registry: tests/test_flip_session_registry.c src/runtime/flip_session_registry.c include/flip_session_registry.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-DFLIP_SESSION_REGISTRY_ROOT='"/tmp/flipos-session-registry-test"' -Iinclude \
		tests/test_flip_session_registry.c src/runtime/flip_session_registry.c \
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

$(BUILD)/flip_save_safety_ui.o: src/frontend/flip_save_safety_ui.c include/flip_save_safety_ui.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_save_safety_launch.o: src/frontend/flip_save_safety_launch.c include/flip_save_safety_ui.h | $(BUILD)
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

$(BUILD)/flip_settings.o: src/runtime/flip_settings.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_hotkeys.o: src/runtime/flip_hotkeys.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_osd.o: src/runtime/flip_osd.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_library.o: src/runtime/flip_library.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_paths.o: src/runtime/flip_paths.c include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_log.o: src/runtime/flip_log.c include/flip_log.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_bios.o: src/runtime/flip_bios.c include/flip_bios.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_exit_protocol.o: src/runtime/flip_exit_protocol.c include/flip_exit_protocol.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_registry.o: src/runtime/flip_session_registry.c include/flip_session_registry.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_adapter.o: src/runtime/flip_session_adapter.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_system.o: src/runtime/flip_system.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session.o: src/runtime/flip_session.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Dflip_session_launch=flip_session_launch_legacy -c $< -o $@

$(BUILD)/flip_recovery.o: src/runtime/flip_recovery.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_power.o: src/runtime/flip_power.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gb.o: src/emulators/flip_gb.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gambatte_core.o: src/emulators/flip_gambatte_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_core.o: src/runtime/flip_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_core_host.o: src/runtime/flip_core_host.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_state.o: src/runtime/flip_state.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_save.o: src/runtime/flip_save.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_perf_diag.o: src/runtime/flip_perf_diag.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_telemetry.o: src/runtime/flip_telemetry.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TELEMETRY_WARN) -c $< -o $@

$(BUILD)/flip_video.o: src/runtime/flip_video.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_timing.o: src/runtime/flip_timing.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_lifecycle.o: src/runtime/flip_lifecycle.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_drm.o: src/hwtest/flip_drm.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_draw.o: src/hwtest/flip_draw.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

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

$(BUILD)/flip-frontend: $(FRONTEND_OBJS)
	$(CC) -static -o $@ $(FRONTEND_OBJS) -lm

$(BUILD)/flip-gb: $(GB_OBJS) $(BUILD)/libgambatte_libretro.a
	$(CXX) -static -o $@ \
		$(GB_OBJS) \
		$(BUILD)/libgambatte_libretro.a \
		-pthread \
		-lm

-include $(DEPS)

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
		$(BUILD)/test-flip-paths \
		$(BUILD)/test-flip-log \
		$(BUILD)/test-flip-bios \
		$(BUILD)/test-flip-library-cache \
		$(BUILD)/test-flip-exit-protocol \
		$(BUILD)/test-flip-lifecycle \
		$(BUILD)/test-flip-core \
		$(BUILD)/test-flip-core-host \
		$(BUILD)/test-flip-state \
		$(BUILD)/test-flip-state-manager \
		$(BUILD)/test-flip-state-recovery-health \
		$(BUILD)/test-flip-save \
		$(BUILD)/test-flip-save-manager \
		$(BUILD)/test-flip-recovery \
		$(BUILD)/test-flip-recovery-transactions \
		$(BUILD)/test-flip-session-registry \
		$(BUILD)/test-flip-session-adapter-save-safety \
		$(BUILD)/test-flip-perf-diag \
		$(BUILD)/test-flip-telemetry
