CROSS ?= aarch64-linux-gnu-

CC  := $(CROSS)gcc
CXX := $(CROSS)g++
HOSTCC ?= gcc

DEVELOPER_BUILD ?= 0
CPPFLAGS := -Iinclude -DFLIPOS_DEVELOPER_BUILD=$(DEVELOPER_BUILD)
CFLAGS   := -O2 -Wall -Wextra -MMD -MP
CXXFLAGS := -O2 -Wall -Wextra -MMD -MP
TELEMETRY_WARN := -Wno-format-truncation

BUILD := build

COMMON_OBJS := \
	$(BUILD)/flip_audio.o \
	$(BUILD)/flip_audio_buffer.o \
	$(BUILD)/flip_audio_queue.o \
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
	$(BUILD)/flip_battery.o \
	$(BUILD)/flip_power_policy.o \
	$(BUILD)/flip_wifi.o \
	$(BUILD)/flip_wifi_profiles.o \
	$(BUILD)/flip_keyboard.o \
	$(BUILD)/flip_setup.o \
	$(BUILD)/flip_time.o \
	$(BUILD)/flip_performance.o \
	$(BUILD)/flip_cpu_policy.o \
	$(BUILD)/flip_cpu_profile.o \
	$(BUILD)/flip_cpu_dynamic.o \
	$(BUILD)/flip_optimizer.o \
	$(BUILD)/flip_exit_protocol.o \
	$(BUILD)/flip_session_persistence.o \
	$(BUILD)/flip_session_registry.o \
	$(BUILD)/flip_session_adapter.o \
	$(BUILD)/flip_system.o \
	$(BUILD)/flip_session.o \
	$(BUILD)/flip_game_stats.o \
	$(BUILD)/flip_recovery.o \
	$(BUILD)/flip_save.o \
	$(BUILD)/flip_state.o \
	$(BUILD)/flip_core.o \
	$(BUILD)/flip_power.o

FRONTEND_OBJS := \
	$(BUILD)/flip_frontend.o \
	$(BUILD)/flip_save_safety_ui.o \
	$(BUILD)/flip_save_safety_launch.o \
	$(BUILD)/flip_bios.o \
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

EMULATOR_OBJS := \
	$(BUILD)/flip_emulator_runner.o \
	$(BUILD)/flip_libretro_core.o \
	$(BUILD)/flip_bios.o \
	$(BUILD)/flip_paths.o \
	$(BUILD)/flip_log.o \
	$(BUILD)/flip_battery.o \
	$(BUILD)/flip_power_policy.o \
	$(BUILD)/flip_performance.o \
	$(BUILD)/flip_cpu_policy.o \
	$(BUILD)/flip_cpu_dynamic.o \
	$(BUILD)/flip_core.o \
	$(BUILD)/flip_core_host.o \
	$(BUILD)/flip_state.o \
	$(BUILD)/flip_save.o \
	$(BUILD)/flip_recovery.o \
	$(BUILD)/flip_perf_diag.o \
	$(BUILD)/flip_telemetry.o \
	$(BUILD)/flip_audio.o \
	$(BUILD)/flip_audio_buffer.o \
	$(BUILD)/flip_audio_queue.o \
	$(BUILD)/flip_audio_resampler.o \
	$(BUILD)/flip_settings.o \
	$(BUILD)/flip_hotkeys.o \
	$(BUILD)/flip_osd.o \
	$(BUILD)/flip_video.o \
	$(BUILD)/flip_video_layout.o \
	$(BUILD)/flip_gpu_client.o \
	$(BUILD)/flip_gpu_protocol.o \
	$(BUILD)/flip_power.o \
	$(BUILD)/flip_input.o \
	$(BUILD)/flip_controls.o \
	$(BUILD)/flip_timing.o \
	$(BUILD)/flip_lifecycle.o \
	$(BUILD)/flip_drm.o \
	$(BUILD)/flip_draw.o

GB_OBJS := \
	$(BUILD)/flip_gb.o \
	$(BUILD)/flip_gambatte_core.o \
	$(EMULATOR_OBJS) \
	$(GB_LIBRETRO_OBJS)

GBA_OBJS := \
	$(BUILD)/flip_gba.o \
	$(BUILD)/flip_mgba_core.o \
	$(EMULATOR_OBJS)

PERF_CAPTURE_OBJS := \
	$(BUILD)/flip_perf_capture.o \
	$(BUILD)/flip_telemetry.o \
	$(BUILD)/flip_battery.o

UMTP_SOURCES := \
	$(wildcard external/uMTP-Responder/src/*.c) \
	$(wildcard external/uMTP-Responder/src/mtp_operations/*.c)

DEPS := $(wildcard $(BUILD)/*.d)

.PHONY: all frontend gb gba charge-service mtp-responder gpu-service gpu-sender perf-capture cpu-policy-tool optimizer-guard-tool optimizer-supervisor-tool optimizer-deadman-tool optimizer-calibration-tool optimizer-candidate-tool optimizer-session-tool optimizer-experimental-calibration-tool optimizer-experimental-candidate-tool optimizer-progress-tool optimizer-validation-progress-tool optimizer-runtime-tool wifi-tool opp-voltage-module runtime-profile-module tsadc-recovery-module rk817-charger-module verify test-controls test-timing test-audio test-audio-queue test-audio-resampler test-paths test-log test-battery test-power-policy test-charger test-wifi test-keyboard test-setup test-update-installer test-update-fetch test-release-package test-performance test-cpu-policy test-cpu-profile test-cpu-dynamic test-optimizer test-optimizer-pmu-token test-optimizer-quiesce test-optimizer-panic-policy test-optimizer-guard test-optimizer-supervisor test-optimizer-calibration test-optimizer-calibration-session test-bios test-settings test-osd test-power test-library-cache test-exit-protocol test-lifecycle test-core test-core-host test-video-layout test-gpu-protocol test-gpu-client test-state test-state-manager test-state-recovery-health test-save test-save-manager test-recovery test-session-persistence test-session-registry test-session-adapter test-game-stats test-system-registry test-session-save-safety test-perf-diag test-telemetry test-perf-capture-tools test-architecture clean clean-objects

all: frontend gb gba mtp-responder

frontend: $(BUILD)/flip-frontend

charge-service: $(BUILD)/flip-charge-service

mtp-responder: $(BUILD)/umtprd

gb: $(BUILD)/flip-gb

gba: $(BUILD)/flip-gba

gpu-service: $(BUILD)/flip-gpu-service

gpu-sender: $(BUILD)/flip-gpu-sender

perf-capture: $(BUILD)/flip-perf-capture

cpu-policy-tool: $(BUILD)/flip-cpu-policy

optimizer-guard-tool: $(BUILD)/flip-optimizer-guard

optimizer-supervisor-tool: $(BUILD)/flip-optimizer-supervisor

optimizer-deadman-tool: $(BUILD)/flip-optimizer-deadman

optimizer-calibration-tool: $(BUILD)/flip-optimizer-calibration

optimizer-candidate-tool: $(BUILD)/flip-optimizer-candidate

optimizer-session-tool: $(BUILD)/flip-optimizer-session

optimizer-experimental-calibration-tool: $(BUILD)/flip-optimizer-calibration-experimental

optimizer-experimental-candidate-tool: $(BUILD)/flip-optimizer-candidate-experimental

optimizer-progress-tool: $(BUILD)/flip-optimizer-progress

optimizer-validation-progress-tool: $(BUILD)/flip-optimizer-validation-progress

optimizer-runtime-tool: $(BUILD)/flip-optimizer-runtime

wifi-tool: $(BUILD)/flip-wifi-tool

opp-voltage-module: $(BUILD)/flip_opp_voltage.ko

runtime-profile-module: $(BUILD)/flip_opp_runtime_profile.ko

tsadc-recovery-module: $(BUILD)/flip_tsadc_recovery.ko

rk817-charger-module: $(BUILD)/flip_rk817_charger.ko

verify:
	@log=/tmp/flipos-verify.log; \
	if ( \
		$(MAKE) test-controls && \
		$(MAKE) test-timing && \
		$(MAKE) test-audio && \
		$(MAKE) test-audio-queue && \
		$(MAKE) test-audio-resampler && \
		$(MAKE) test-paths && \
		$(MAKE) test-log && \
		$(MAKE) test-battery && \
		$(MAKE) test-power-policy && \
		$(MAKE) test-charger && \
		$(MAKE) test-wifi && \
		$(MAKE) test-keyboard && \
		$(MAKE) test-setup && \
		$(MAKE) test-update-installer && \
		$(MAKE) test-update-fetch && \
		$(MAKE) test-release-package && \
		$(MAKE) test-performance && \
		$(MAKE) test-cpu-policy && \
		$(MAKE) test-cpu-profile && \
		$(MAKE) test-cpu-dynamic && \
		$(MAKE) test-optimizer && \
		$(MAKE) test-optimizer-pmu-token && \
		$(MAKE) test-optimizer-quiesce && \
		$(MAKE) test-optimizer-panic-policy && \
		$(MAKE) test-playtest-deploy && \
		$(MAKE) test-optimizer-guard && \
		$(MAKE) test-optimizer-supervisor && \
		$(MAKE) test-optimizer-calibration && \
		$(MAKE) test-optimizer-calibration-session && \
		$(MAKE) optimizer-candidate-tool && \
		$(MAKE) optimizer-session-tool && \
		$(MAKE) optimizer-validation-progress-tool && \
		$(MAKE) optimizer-deadman-tool && \
		$(MAKE) test-bios && \
		$(MAKE) test-settings && \
		$(MAKE) test-osd && \
		$(MAKE) test-power && \
		$(MAKE) test-library-cache && \
		$(MAKE) test-exit-protocol && \
		$(MAKE) test-lifecycle && \
		$(MAKE) test-core && \
		$(MAKE) test-core-host && \
		$(MAKE) test-video-layout && \
		$(MAKE) test-gpu-protocol && \
		$(MAKE) test-gpu-client && \
		$(MAKE) test-state && \
		$(MAKE) test-state-manager && \
		$(MAKE) test-state-recovery-health && \
		$(MAKE) test-save && \
		$(MAKE) test-save-manager && \
		$(MAKE) test-recovery && \
		$(MAKE) test-session-persistence && \
		$(MAKE) test-session-registry && \
		$(MAKE) test-session-adapter && \
		$(MAKE) test-game-stats && \
		$(MAKE) test-system-registry && \
		$(MAKE) test-session-save-safety && \
		$(MAKE) test-perf-diag && \
		$(MAKE) test-telemetry && \
		$(MAKE) test-perf-capture-tools && \
		$(MAKE) test-architecture && \
		$(MAKE) gpu-service && \
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

test-audio-queue: $(BUILD)/test-flip-audio-queue
	$(BUILD)/test-flip-audio-queue

test-audio-resampler: $(BUILD)/test-flip-audio-resampler
	$(BUILD)/test-flip-audio-resampler

test-paths: $(BUILD)/test-flip-paths
	$(BUILD)/test-flip-paths

test-log: $(BUILD)/test-flip-log
	$(BUILD)/test-flip-log

test-battery: $(BUILD)/test-flip-battery
	$(BUILD)/test-flip-battery

test-power-policy: $(BUILD)/test-flip-power-policy
	$(BUILD)/test-flip-power-policy

test-charger: $(BUILD)/test-flip-charger
	$(BUILD)/test-flip-charger

test-wifi: $(BUILD)/test-flip-wifi
	$(BUILD)/test-flip-wifi

test-keyboard: $(BUILD)/test-flip-keyboard
	$(BUILD)/test-flip-keyboard

test-setup: $(BUILD)/test-flip-setup
	$(BUILD)/test-flip-setup

test-update-installer:
	python3 tests/test_update_installer.py

test-update-fetch:
	python3 tests/test_update_fetch.py

test-release-package:
	python3 tests/test_release_package.py

test-performance: $(BUILD)/test-flip-performance
	$(BUILD)/test-flip-performance

test-cpu-policy: $(BUILD)/test-flip-cpu-policy
	$(BUILD)/test-flip-cpu-policy

test-cpu-profile: $(BUILD)/test-flip-cpu-profile
	$(BUILD)/test-flip-cpu-profile

test-cpu-dynamic: $(BUILD)/test-flip-cpu-dynamic
	$(BUILD)/test-flip-cpu-dynamic

test-optimizer: $(BUILD)/test-flip-optimizer
	$(BUILD)/test-flip-optimizer

test-optimizer-pmu-token: $(BUILD)/test-flip-optimizer-pmu-token
	$(BUILD)/test-flip-optimizer-pmu-token

test-optimizer-quiesce: $(BUILD)/test-flip-optimizer-quiesce
	$(BUILD)/test-flip-optimizer-quiesce
	python3 tests/test_optimizer_quiesce_architecture.py

test-optimizer-panic-policy: $(BUILD)/test-flip-optimizer-panic-policy
	$(BUILD)/test-flip-optimizer-panic-policy
	python3 tests/test_optimizer_panic_policy_architecture.py

test-playtest-deploy:
	python3 tests/test_playtest_deploy_architecture.py

test-optimizer-guard: $(BUILD)/test-flip-optimizer-guard
	$(BUILD)/test-flip-optimizer-guard

test-optimizer-supervisor: $(BUILD)/test-flip-optimizer-supervisor
	$(BUILD)/test-flip-optimizer-supervisor

test-optimizer-calibration: $(BUILD)/test-flip-optimizer-calibration
	$(BUILD)/test-flip-optimizer-calibration
	python3 tests/test_optimizer_full_range_architecture.py

test-optimizer-calibration-session: $(BUILD)/test-flip-optimizer-calibration-session
	$(BUILD)/test-flip-optimizer-calibration-session

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

test-video-layout: $(BUILD)/test-flip-video-layout
	$(BUILD)/test-flip-video-layout

test-gpu-protocol: $(BUILD)/test-flip-gpu-protocol
	$(BUILD)/test-flip-gpu-protocol

test-gpu-client: $(BUILD)/test-flip-gpu-client
	$(BUILD)/test-flip-gpu-client

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

test-session-persistence: $(BUILD)/test-flip-session-persistence
	$(BUILD)/test-flip-session-persistence

test-session-registry: $(BUILD)/test-flip-session-registry
	$(BUILD)/test-flip-session-registry

test-session-adapter: $(BUILD)/test-flip-session-adapter
	$(BUILD)/test-flip-session-adapter

test-game-stats: $(BUILD)/test-flip-game-stats
	$(BUILD)/test-flip-game-stats

test-system-registry: $(BUILD)/test-flip-system-registry
	$(BUILD)/test-flip-system-registry

test-session-save-safety: $(BUILD)/test-flip-session-adapter-save-safety
	$(BUILD)/test-flip-session-adapter-save-safety

test-perf-diag: $(BUILD)/test-flip-perf-diag
	$(BUILD)/test-flip-perf-diag

test-telemetry: $(BUILD)/test-flip-telemetry
	$(BUILD)/test-flip-telemetry

test-perf-capture-tools:
	python3 tests/test_perf_capture_tools.py

test-architecture:
	python3 tests/test_flip_gb_architecture.py
	python3 tests/test_gba_architecture.py
	python3 tests/test_session_adapter_architecture.py
	python3 tests/test_paths_architecture.py
	python3 tests/test_logging_architecture.py
	python3 tests/test_battery_architecture.py
	python3 tests/test_rk817_charger_module_architecture.py
	python3 tests/test_performance_architecture.py
	python3 tests/test_bios_architecture.py
	python3 tests/test_settings_architecture.py
	python3 tests/test_osd_architecture.py
	python3 tests/test_power_architecture.py
	python3 tests/test_frontend_ux_architecture.py
	python3 tests/test_home_layout_architecture.py
	python3 tests/test_gameswitcher_layout_architecture.py
	python3 tests/test_library_layout_architecture.py
	python3 tests/test_stats_details_layout_architecture.py
	python3 tests/test_settings_layout_architecture.py
	python3 tests/test_wifi_layout_architecture.py
	python3 tests/test_utility_layout_architecture.py
	python3 tests/test_time_layout_architecture.py
	python3 tests/test_first_boot_layout_architecture.py
	python3 tests/test_optimizer_layout_architecture.py
	python3 tests/test_quick_menu_layout_architecture.py
	python3 tests/test_play_options_layout_architecture.py
	python3 tests/test_power_menu_architecture.py
	python3 tests/test_ui_scale_architecture.py
	python3 tests/test_keyboard_case_architecture.py
	python3 tests/test_settings_navigation_architecture.py
	python3 tests/test_first_boot_architecture.py
	python3 tests/test_time_architecture.py
	python3 tests/test_usb_mtp_architecture.py
	python3 tests/test_update_architecture.py
	python3 tests/test_playtest_deploy_architecture.py
	python3 tests/test_library_cache_architecture.py
	python3 tests/test_save_manager_architecture.py
	python3 tests/test_exit_protocol_architecture.py
	python3 tests/test_session_registry_architecture.py
	python3 tests/test_session_persistence_architecture.py
	python3 tests/test_core_host_diagnostics_architecture.py
	python3 tests/test_audio_callback_diagnostics_architecture.py
	python3 tests/test_audio_rate_conversion_architecture.py
	python3 tests/test_state_manager_architecture.py
	python3 tests/test_gambatte_save_memory_architecture.py
	python3 tests/test_recovery_architecture.py
	python3 tests/test_frontend_save_safety_architecture.py
	python3 tests/test_save_safety_management_architecture.py
	python3 tests/test_verify_target_architecture.py
	python3 tests/test_optimizer_guard_boot_architecture.py
	python3 tests/test_optimizer_candidate_architecture.py
	python3 tests/test_opp_voltage_module_architecture.py
	python3 tests/test_opp_runtime_profile_module_architecture.py
	python3 tests/test_optimizer_runtime_architecture.py
	python3 tests/test_tsadc_recovery_module_architecture.py
	python3 tests/test_optimizer_hw_watchdog_architecture.py
	python3 tests/test_optimizer_session_architecture.py
	python3 tests/test_optimizer_ui_architecture.py
	python3 tests/test_play_stats_architecture.py

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

$(BUILD)/test-flip-battery: tests/test_flip_battery.c src/runtime/flip_battery.c include/flip_battery.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_BATTERY_ROOT='"/tmp/flipos-battery-service-test"' \
		-DFLIP_AC_ROOT='"/tmp/flipos-battery-service-test-ac"' \
		-DFLIP_USB_ROOT='"/tmp/flipos-battery-service-test-usb"' -Iinclude \
		tests/test_flip_battery.c src/runtime/flip_battery.c \
		-o $@

$(BUILD)/test-flip-power-policy: tests/test_flip_power_policy.c src/runtime/flip_power_policy.c include/flip_power_policy.h include/flip_battery.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_power_policy.c src/runtime/flip_power_policy.c \
		-o $@

$(BUILD)/test-flip-charger: tests/test_flip_charger.c src/runtime/flip_charger.c src/runtime/flip_power_policy.c include/flip_charger.h include/flip_power_policy.h include/flip_battery.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		-DFLIP_CHARGER_CURRENT_LIMIT_PATH='"/tmp/flipos-charger-test/current_limit_ma"' \
		-DFLIP_CHARGER_STOCK_CURRENT_PATH='"/tmp/flipos-charger-test/stock_current_ma"' \
		tests/test_flip_charger.c src/runtime/flip_charger.c src/runtime/flip_power_policy.c \
		-o $@

$(BUILD)/test-flip-wifi: tests/test_flip_wifi.c src/runtime/flip_wifi.c src/runtime/flip_wifi_profiles.c include/flip_wifi.h include/flip_wifi_profiles.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_WIFI_CONFIG_DIR='"/tmp/flipos-wifi-test/network"' \
		-DFLIP_WIFI_CONFIG_FILE='"/tmp/flipos-wifi-test/network/wpa_supplicant.conf"' \
		-DFLIP_WIFI_WPA_PASSPHRASE='"/tmp/flipos-wifi-test/wpa_passphrase"' \
		-Iinclude tests/test_flip_wifi.c src/runtime/flip_wifi.c src/runtime/flip_wifi_profiles.c \
		-o $@

$(BUILD)/test-flip-keyboard: tests/test_flip_keyboard.c src/runtime/flip_keyboard.c include/flip_keyboard.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_keyboard.c src/runtime/flip_keyboard.c -o $@

$(BUILD)/test-flip-setup: tests/test_flip_setup.c src/runtime/flip_setup.c include/flip_setup.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L \
		-DFLIP_SETUP_ROOT='"/tmp/flipos-setup-test/setup"' \
		-DFLIP_SETUP_COMPLETE_FILE='"/tmp/flipos-setup-test/setup/complete"' \
		-DFLIP_SETUP_PROGRESS_FILE='"/tmp/flipos-setup-test/setup/in-progress"' \
		-Iinclude tests/test_flip_setup.c src/runtime/flip_setup.c -o $@

$(BUILD)/test-flip-performance: tests/test_flip_performance.c src/runtime/flip_performance.c include/flip_performance.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_PERF_CPU_POLICY_ROOT='"/tmp/flipos-performance-test/cpu"' \
		-DFLIP_PERF_GPU_ROOT='"/tmp/flipos-performance-test/gpu"' -Iinclude \
		tests/test_flip_performance.c src/runtime/flip_performance.c \
		-o $@

$(BUILD)/test-flip-cpu-policy: tests/test_flip_cpu_policy.c src/runtime/flip_cpu_policy.c include/flip_cpu_policy.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_CPU_POLICY_ROOT='"/tmp/flipos-cpu-policy-test"' -Iinclude \
		tests/test_flip_cpu_policy.c src/runtime/flip_cpu_policy.c \
		-o $@

$(BUILD)/test-flip-cpu-profile: tests/test_flip_cpu_profile.c src/runtime/flip_cpu_profile.c src/runtime/flip_cpu_policy.c include/flip_cpu_profile.h include/flip_cpu_policy.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_CPU_POLICY_ROOT='"/tmp/flipos-cpu-profile-test"' -Iinclude \
		tests/test_flip_cpu_profile.c src/runtime/flip_cpu_profile.c src/runtime/flip_cpu_policy.c \
		-o $@

$(BUILD)/test-flip-cpu-dynamic: tests/test_flip_cpu_dynamic.c src/runtime/flip_cpu_dynamic.c src/runtime/flip_cpu_policy.c include/flip_cpu_dynamic.h include/flip_cpu_policy.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_CPU_POLICY_ROOT='"/tmp/flipos-cpu-dynamic-test"' -Iinclude \
		tests/test_flip_cpu_dynamic.c src/runtime/flip_cpu_dynamic.c src/runtime/flip_cpu_policy.c \
		-o $@

$(BUILD)/test-flip-optimizer: tests/test_flip_optimizer.c src/runtime/flip_optimizer.c include/flip_optimizer.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_ROOT='"/tmp/flipos-optimizer-test"' -Iinclude \
		tests/test_flip_optimizer.c src/runtime/flip_optimizer.c \
		-lm -o $@

$(BUILD)/test-flip-optimizer-pmu-token: tests/test_flip_optimizer_pmu_token.c src/runtime/flip_optimizer_pmu_token.c include/flip_optimizer_pmu_token.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror 		-D_POSIX_C_SOURCE=200809L 		-DFLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE='"/tmp/flipos-pmu-token-unit-test"' -Iinclude 		tests/test_flip_optimizer_pmu_token.c src/runtime/flip_optimizer_pmu_token.c 		-o $@

$(BUILD)/test-flip-optimizer-quiesce: tests/test_flip_optimizer_quiesce.c src/runtime/flip_optimizer_quiesce.c include/flip_optimizer_quiesce.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L -Iinclude \
		tests/test_flip_optimizer_quiesce.c src/runtime/flip_optimizer_quiesce.c \
		-o $@

$(BUILD)/test-flip-optimizer-panic-policy: tests/test_flip_optimizer_panic_policy.c src/runtime/flip_optimizer_panic_policy.c include/flip_optimizer_panic_policy.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_SYSCTL_ROOT='"/tmp/flipos-optimizer-panic-policy-test"' -Iinclude \
		tests/test_flip_optimizer_panic_policy.c src/runtime/flip_optimizer_panic_policy.c \
		-o $@

$(BUILD)/test-flip-optimizer-guard: tests/test_flip_optimizer_guard.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_GUARD_ROOT='"/tmp/flipos-optimizer-guard-test"' \
		-DFLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE='"/tmp/flipos-optimizer-guard-test/pmu-token"' -Iinclude \
		tests/test_flip_optimizer_guard.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		-o $@

$(BUILD)/test-flip-optimizer-supervisor: tests/test_flip_optimizer_supervisor.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c include/flip_optimizer_supervisor.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_GUARD_ROOT='"/tmp/flipos-optimizer-supervisor-test"' \
		-DFLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE='"/tmp/flipos-optimizer-supervisor-test/pmu-token"' \
		-DFLIP_OPTIMIZER_BOOT_MODE_PATH='"/tmp/flipos-optimizer-boot-mode-test"' -Iinclude \
		tests/test_flip_optimizer_supervisor.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		-o $@

$(BUILD)/test-flip-optimizer-calibration: tests/test_flip_optimizer_calibration.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer.c include/flip_optimizer_calibration.h include/flip_optimizer.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_CALIBRATION_ROOT='"/tmp/flipos-optimizer-calibration-test"' -Iinclude \
		tests/test_flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer.c \
		-lm -o $@

$(BUILD)/test-flip-optimizer-calibration-session: tests/test_flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c include/flip_optimizer_calibration_session.h include/flip_optimizer_calibration.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_OPTIMIZER_CALIBRATION_ROOT='"/tmp/flipos-optimizer-calibration-session-test"' \
		-DFLIP_OPTIMIZER_GUARD_ROOT='"/tmp/flipos-optimizer-calibration-session-test"' \
		-DFLIP_OPTIMIZER_PMU_TOKEN_TEST_FILE='"/tmp/flipos-optimizer-calibration-session-test/pmu-token"' -Iinclude \
		tests/test_flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c \
		-lm -o $@

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
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
		-DFLIP_BACKLIGHT_BRIGHTNESS='"/tmp/flipos-power-service-test/brightness"' \
		-DFLIP_POWER_MEM_SLEEP='"/tmp/flipos-power-service-test/mem_sleep"' \
		-DFLIP_POWER_STATE='"/tmp/flipos-power-service-test/state"' \
		-Iinclude tests/test_flip_power.c src/runtime/flip_power.c \
		-o $@

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

$(BUILD)/test-flip-audio-queue: tests/test_flip_audio_queue.c src/audio/flip_audio_queue.c src/audio/flip_audio_queue.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Isrc/audio \
		tests/test_flip_audio_queue.c src/audio/flip_audio_queue.c \
		-o $@

$(BUILD)/test-flip-audio-resampler: tests/test_flip_audio_resampler.c src/audio/flip_audio_resampler.c include/flip_audio_resampler.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_audio_resampler.c src/audio/flip_audio_resampler.c \
		-o $@

$(BUILD)/test-flip-game-stats: tests/test_flip_game_stats.c src/runtime/flip_game_stats.c include/flip_game_stats.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_GAME_STATS_ROOT='"/tmp/flipos-game-stats-test"' -Iinclude \
		tests/test_flip_game_stats.c src/runtime/flip_game_stats.c \
		-o $@

$(BUILD)/test-flip-lifecycle: tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_log.c src/runtime/flip_paths.c src/runtime/flip_timing.c include/flip_lifecycle.h include/flip_log.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_log.c src/runtime/flip_paths.c src/runtime/flip_timing.c \
		-o $@

$(BUILD)/test-flip-core: tests/test_flip_core.c src/runtime/flip_core.c include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_core.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-core-host: tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c src/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c src/runtime/flip_cpu_dynamic.c src/runtime/flip_cpu_policy.c src/runtime/flip_battery.c src/runtime/flip_power_policy.c include/flip_core_host.h include/flip_perf_diag.h include/flip_cpu_dynamic.h include/flip_cpu_policy.h include/flip_battery.h include/flip_power_policy.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -Iinclude \
		tests/test_flip_core_host.c tests/test_flip_core_host_telemetry_stubs.c \
		src/runtime/flip_core.c src/runtime/flip_core_host.c src/runtime/flip_perf_diag.c \
		src/runtime/flip_cpu_dynamic.c src/runtime/flip_cpu_policy.c \
		src/runtime/flip_battery.c src/runtime/flip_power_policy.c \
		-o $@

$(BUILD)/test-flip-video-layout: tests/test_flip_video_layout.c src/runtime/flip_video_layout.c include/flip_video_layout.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_video_layout.c src/runtime/flip_video_layout.c \
		-o $@

$(BUILD)/test-flip-gpu-protocol: tests/test_flip_gpu_protocol.c src/runtime/flip_gpu_protocol.c include/flip_gpu_protocol.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_gpu_protocol.c src/runtime/flip_gpu_protocol.c \
		-o $@

$(BUILD)/test-flip-gpu-fake-service: tests/test_flip_gpu_fake_service.c include/flip_gpu_protocol.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_gpu_fake_service.c \
		-o $@

$(BUILD)/test-flip-gpu-client: tests/test_flip_gpu_client.c src/runtime/flip_gpu_client.c src/runtime/flip_gpu_protocol.c include/flip_gpu_client.h include/flip_gpu_protocol.h $(BUILD)/test-flip-gpu-fake-service | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_gpu_client.c src/runtime/flip_gpu_client.c src/runtime/flip_gpu_protocol.c \
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

$(BUILD)/test-flip-session-adapter: tests/test_flip_session_adapter.c src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_session_adapter.c src/runtime/flip_session_adapter.c \
		src/runtime/flip_recovery.c src/runtime/flip_save.c \
		src/runtime/flip_state.c src/runtime/flip_core.c -o $@

$(BUILD)/test-flip-system-registry: tests/test_flip_system_registry.c src/runtime/flip_system.c src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_system_registry.c src/runtime/flip_system.c \
		src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c \
		src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c -o $@

$(BUILD)/test-flip-session-adapter-save-safety: tests/test_flip_session_adapter_save_safety.c src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c include/flip_session_adapter.h include/flip_recovery.h include/flip_save.h include/flip_state.h include/flip_core.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_session_adapter_save_safety.c \
		src/runtime/flip_session_adapter.c src/runtime/flip_recovery.c \
		src/runtime/flip_save.c src/runtime/flip_state.c src/runtime/flip_core.c \
		-o $@

$(BUILD)/test-flip-session-persistence: tests/test_flip_session_persistence.c src/runtime/flip_session_persistence.c include/flip_session_persistence.h include/flip_paths.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L \
		-DFLIP_STORAGE_ROOT='"/tmp/flipos-session-persistence-test"' -Iinclude \
		tests/test_flip_session_persistence.c src/runtime/flip_session_persistence.c \
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

$(BUILD)/test-flip-telemetry: tests/test_flip_telemetry.c src/runtime/flip_telemetry.c src/runtime/flip_battery.c include/flip_telemetry.h include/flip_battery.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror $(TELEMETRY_WARN) -Iinclude \
		tests/test_flip_telemetry.c src/runtime/flip_telemetry.c src/runtime/flip_battery.c \
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

$(BUILD)/flip_audio_queue.o: src/audio/flip_audio_queue.c src/audio/flip_audio_queue.h | $(BUILD)
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

$(BUILD)/flip_battery.o: src/runtime/flip_battery.c include/flip_battery.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_performance.o: src/runtime/flip_performance.c include/flip_performance.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_cpu_policy.o: src/runtime/flip_cpu_policy.c include/flip_cpu_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

$(BUILD)/flip_cpu_profile.o: src/runtime/flip_cpu_profile.c include/flip_cpu_profile.h include/flip_cpu_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

$(BUILD)/flip_cpu_dynamic.o: src/runtime/flip_cpu_dynamic.c include/flip_cpu_dynamic.h include/flip_cpu_policy.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

$(BUILD)/flip_optimizer.o: src/runtime/flip_optimizer.c include/flip_optimizer.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

$(BUILD)/flip_bios.o: src/runtime/flip_bios.c include/flip_bios.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_exit_protocol.o: src/runtime/flip_exit_protocol.c include/flip_exit_protocol.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_persistence.o: src/runtime/flip_session_persistence.c include/flip_session_persistence.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_registry.o: src/runtime/flip_session_registry.c include/flip_session_registry.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session_adapter.o: src/runtime/flip_session_adapter.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_system.o: src/runtime/flip_system.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_session.o: src/runtime/flip_session.c include/flip_session.h include/flip_session_persistence.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Dflip_session_launch=flip_session_launch_legacy -c $< -o $@

$(BUILD)/flip_game_stats.o: src/runtime/flip_game_stats.c include/flip_game_stats.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -c $< -o $@

$(BUILD)/flip_recovery.o: src/runtime/flip_recovery.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_power_policy.o: src/runtime/flip_power_policy.c include/flip_power_policy.h include/flip_battery.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_charger.o: src/runtime/flip_charger.c include/flip_charger.h include/flip_power_policy.h include/flip_battery.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_wifi.o: src/runtime/flip_wifi.c include/flip_wifi.h include/flip_wifi_profiles.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_wifi_profiles.o: src/runtime/flip_wifi_profiles.c include/flip_wifi_profiles.h include/flip_wifi.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_keyboard.o: src/runtime/flip_keyboard.c include/flip_keyboard.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_setup.o: src/runtime/flip_setup.c include/flip_setup.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_time.o: src/runtime/flip_time.c include/flip_time.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_power.o: src/runtime/flip_power.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gb.o: src/emulators/flip_gb.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gambatte_core.o: src/emulators/flip_gambatte_core.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gba.o: src/emulators/flip_gba.c include/flip_emulator_runner.h include/flip_mgba_core.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_mgba_core.o: src/emulators/flip_mgba_core.c include/flip_mgba_core.h include/flip_libretro_core.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_libretro_core.o: src/emulators/flip_libretro_core.c include/flip_libretro_core.h include/libretro/libretro.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_emulator_runner.o: src/runtime/flip_emulator_runner.c include/flip_emulator_runner.h | $(BUILD)
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

$(BUILD)/flip_video.o: src/runtime/flip_video.c include/flip_video_layout.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_video_layout.o: src/runtime/flip_video_layout.c include/flip_video_layout.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gpu_client.o: src/runtime/flip_gpu_client.c include/flip_gpu_client.h include/flip_gpu_protocol.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_gpu_protocol.o: src/runtime/flip_gpu_protocol.c include/flip_gpu_protocol.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_timing.o: src/runtime/flip_timing.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_lifecycle.o: src/runtime/flip_lifecycle.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_drm.o: src/hwtest/flip_drm.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_draw.o: src/hwtest/flip_draw.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/flip_perf_capture.o: src/hwtest/flip_perf_capture.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Werror -c $< -o $@

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

$(BUILD)/flip-gpu-service: \
		src/runtime/flip_gpu_service.c \
		src/runtime/flip_gpu_context.c \
		src/runtime/flip_gpu_kms.c \
		src/runtime/flip_gpu_scanout.c \
		src/runtime/flip_gpu_protocol.c \
		src/runtime/flip_video_layout.c \
		include/flip_gpu_context.h \
		include/flip_gpu_kms.h \
		include/flip_gpu_scanout.h \
		include/flip_gpu_protocol.h \
		include/flip_video_layout.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/runtime/flip_gpu_service.c \
		src/runtime/flip_gpu_context.c \
		src/runtime/flip_gpu_kms.c \
		src/runtime/flip_gpu_scanout.c \
		src/runtime/flip_gpu_protocol.c \
		src/runtime/flip_video_layout.c \
		-ldl -o $@

$(BUILD)/flip-cpu-policy: src/hwtest/flip_cpu_policy_tool.c $(BUILD)/flip_cpu_policy.o include/flip_cpu_policy.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/hwtest/flip_cpu_policy_tool.c $(BUILD)/flip_cpu_policy.o \
		-static -o $@

$(BUILD)/flip-optimizer-guard: src/hwtest/flip_optimizer_guard_tool.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_guard_tool.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		-static -o $@

$(BUILD)/flip-optimizer-supervisor: src/hwtest/flip_optimizer_supervisor_tool.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c include/flip_optimizer_supervisor.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_supervisor_tool.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		-static -o $@

$(BUILD)/flip-optimizer-deadman: src/hwtest/flip_optimizer_deadman.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
		src/hwtest/flip_optimizer_deadman.c \
		-static -o $@

$(BUILD)/flip-optimizer-calibration: src/hwtest/flip_optimizer_calibration_tool.c src/runtime/flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c include/flip_optimizer_calibration_session.h include/flip_optimizer_calibration.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_calibration_tool.c \
		src/runtime/flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c \
		-static -lm -o $@

$(BUILD)/flip-optimizer-candidate: src/hwtest/flip_optimizer_candidate_tool.c src/runtime/flip_cpu_policy.c src/runtime/flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer_quiesce.c src/runtime/flip_optimizer_panic_policy.c src/runtime/flip_optimizer_hw_watchdog.c src/runtime/flip_optimizer.c include/flip_cpu_policy.h include/flip_optimizer_calibration_session.h include/flip_optimizer_calibration.h include/flip_optimizer_supervisor.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer_quiesce.h include/flip_optimizer_panic_policy.h include/flip_optimizer.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_candidate_tool.c \
		src/runtime/flip_cpu_policy.c \
		src/runtime/flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_supervisor.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		src/runtime/flip_optimizer_quiesce.c src/runtime/flip_optimizer_panic_policy.c \
		src/runtime/flip_optimizer_hw_watchdog.c src/runtime/flip_optimizer.c \
		-static -pthread -lm -o $@

OPTIMIZER_EXPERIMENTAL_DEFS := \
	-DFLIP_OPTIMIZER_EXPERIMENTAL_BUILD=1 \
	-DFLIP_OPTIMIZER_CALIBRATION_RK3566_FLOOR_UV=750000U \
	-DFLIP_OPTIMIZER_CALIBRATION_TRIAL_PREFIX='"xuv"' \
	-DFLIP_OPTIMIZER_CALIBRATION_FILE='"/mnt/SDCARD/flipos-dev/state/optimizer/cpu-calibration-experimental"' \
	-DFLIP_OPTIMIZER_RESULT_FILE='"/mnt/SDCARD/flipos-dev/state/optimizer/cpu-result-experimental"' \
	-DFLIP_OPTIMIZER_REPORT_FILE='"/mnt/SDCARD/flipos-dev/state/optimizer/cpu-report-experimental"' \
	-DFLIP_OPTIMIZER_UI_RETURN_FILE='"/mnt/SDCARD/flipos-dev/state/optimizer/experimental-return"' \
	-DFLIP_OPTIMIZER_AUTO_SESSION_FILE='"/mnt/SDCARD/flipos-dev/state/optimizer/auto-session-experimental"' \
	-DFLIP_OPTIMIZER_AUTO_SESSION_TMP='"/mnt/SDCARD/flipos-dev/state/optimizer/auto-session-experimental.tmp"'

$(BUILD)/flip-optimizer-calibration-experimental: src/hwtest/flip_optimizer_calibration_tool.c src/runtime/flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c include/flip_optimizer_calibration_session.h include/flip_optimizer_calibration.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_POSIX_C_SOURCE=200809L $(OPTIMIZER_EXPERIMENTAL_DEFS) -Iinclude \
		src/hwtest/flip_optimizer_calibration_tool.c \
		src/runtime/flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c \
		-static -lm -o $@

$(BUILD)/flip-optimizer-candidate-experimental: src/hwtest/flip_optimizer_candidate_tool.c src/runtime/flip_cpu_policy.c src/runtime/flip_optimizer_calibration_session.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer_quiesce.c src/runtime/flip_optimizer_panic_policy.c src/runtime/flip_optimizer_hw_watchdog.c src/runtime/flip_optimizer.c include/flip_cpu_policy.h include/flip_optimizer_calibration_session.h include/flip_optimizer_calibration.h include/flip_optimizer_supervisor.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer_quiesce.h include/flip_optimizer_panic_policy.h include/flip_optimizer.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L $(OPTIMIZER_EXPERIMENTAL_DEFS) -Iinclude \
		src/hwtest/flip_optimizer_candidate_tool.c \
		src/runtime/flip_cpu_policy.c \
		src/runtime/flip_optimizer_calibration_session.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_supervisor.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		src/runtime/flip_optimizer_quiesce.c src/runtime/flip_optimizer_panic_policy.c \
		src/runtime/flip_optimizer_hw_watchdog.c src/runtime/flip_optimizer.c \
		-static -pthread -lm -o $@

$(BUILD)/flip-optimizer-progress: src/hwtest/flip_optimizer_progress.c $(BUILD)/flip_drm.o $(BUILD)/flip_draw.o include/flip_drm.h include/flip_draw.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/hwtest/flip_optimizer_progress.c \
		$(BUILD)/flip_drm.o $(BUILD)/flip_draw.o \
		-static -o $@

$(BUILD)/flip-optimizer-validation-progress: src/hwtest/flip_optimizer_validation_progress.c $(BUILD)/flip_drm.o $(BUILD)/flip_draw.o include/flip_drm.h include/flip_draw.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/hwtest/flip_optimizer_validation_progress.c \
		$(BUILD)/flip_drm.o $(BUILD)/flip_draw.o \
		-static -o $@

$(BUILD)/flip-optimizer-runtime: src/hwtest/flip_optimizer_runtime_tool.c src/runtime/flip_cpu_policy.c src/runtime/flip_optimizer.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c include/flip_cpu_policy.h include/flip_optimizer.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_runtime_tool.c \
		src/runtime/flip_cpu_policy.c \
		src/runtime/flip_optimizer.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		-static -lm -o $@

$(BUILD)/flip-optimizer-session: src/hwtest/flip_optimizer_session_tool.c src/runtime/flip_optimizer_calibration.c src/runtime/flip_optimizer_supervisor.c src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c src/runtime/flip_optimizer.c include/flip_optimizer_calibration.h include/flip_optimizer_supervisor.h include/flip_optimizer_guard.h include/flip_optimizer_pmu_token.h include/flip_optimizer.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror \
		-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude \
		src/hwtest/flip_optimizer_session_tool.c \
		src/runtime/flip_optimizer_calibration.c \
		src/runtime/flip_optimizer_supervisor.c \
		src/runtime/flip_optimizer_guard.c src/runtime/flip_optimizer_pmu_token.c \
		src/runtime/flip_optimizer.c \
		-static -lm -o $@

$(BUILD)/flip_opp_voltage.ko: tools/kernel/flip_opp_voltage/flip_opp_voltage.c tools/kernel/flip_opp_voltage/Makefile | $(BUILD)
	@test -n "$(KERNEL_DIR)" || { echo "KERNEL_DIR must point to the prepared matching Rockchip kernel tree" >&2; exit 1; }
	rm -rf $(BUILD)/opp-voltage-module $(BUILD)/kernel-python-shim
	mkdir -p $(BUILD)/opp-voltage-module $(BUILD)/kernel-python-shim
	cp tools/kernel/flip_opp_voltage/flip_opp_voltage.c tools/kernel/flip_opp_voltage/Makefile $(BUILD)/opp-voltage-module/
	ln -sf "$$(command -v python3)" $(BUILD)/kernel-python-shim/python
	PATH="$(CURDIR)/$(BUILD)/kernel-python-shim:$$PATH" \
		$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)/$(BUILD)/opp-voltage-module" \
		ARCH=arm64 CROSS_COMPILE="$(CROSS)" modules
	cp $(BUILD)/opp-voltage-module/flip_opp_voltage.ko $@

$(BUILD)/flip_opp_runtime_profile.ko: tools/kernel/flip_opp_runtime_profile/flip_opp_runtime_profile.c tools/kernel/flip_opp_runtime_profile/Makefile | $(BUILD)
	@test -n "$(KERNEL_DIR)" || { echo "KERNEL_DIR must point to the prepared matching Rockchip kernel tree" >&2; exit 1; }
	rm -rf $(BUILD)/opp-runtime-profile-module $(BUILD)/kernel-python-shim
	mkdir -p $(BUILD)/opp-runtime-profile-module $(BUILD)/kernel-python-shim
	cp tools/kernel/flip_opp_runtime_profile/flip_opp_runtime_profile.c tools/kernel/flip_opp_runtime_profile/Makefile $(BUILD)/opp-runtime-profile-module/
	ln -sf "$$(command -v python3)" $(BUILD)/kernel-python-shim/python
	PATH="$(CURDIR)/$(BUILD)/kernel-python-shim:$$PATH" \
		$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)/$(BUILD)/opp-runtime-profile-module" \
		ARCH=arm64 CROSS_COMPILE="$(CROSS)" modules
	cp $(BUILD)/opp-runtime-profile-module/flip_opp_runtime_profile.ko $@

$(BUILD)/flip_rk817_charger.ko: tools/kernel/flip_rk817_charger/flip_rk817_charger.c tools/kernel/flip_rk817_charger/Makefile | $(BUILD)
	@test -n "$(KERNEL_DIR)" || { echo "KERNEL_DIR must point to the prepared matching Rockchip kernel tree" >&2; exit 1; }
	rm -rf $(BUILD)/rk817-charger-module $(BUILD)/kernel-python-shim
	mkdir -p $(BUILD)/rk817-charger-module $(BUILD)/kernel-python-shim
	cp tools/kernel/flip_rk817_charger/flip_rk817_charger.c tools/kernel/flip_rk817_charger/Makefile $(BUILD)/rk817-charger-module/
	ln -sf "$$(command -v python3)" $(BUILD)/kernel-python-shim/python
	PATH="$(CURDIR)/$(BUILD)/kernel-python-shim:$$PATH" \
		$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)/$(BUILD)/rk817-charger-module" \
		ARCH=arm64 CROSS_COMPILE="$(CROSS)" modules
	cp $(BUILD)/rk817-charger-module/flip_rk817_charger.ko $@

$(BUILD)/flip_tsadc_recovery.ko: tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c tools/kernel/flip_tsadc_recovery/Makefile | $(BUILD)
	@test -n "$(KERNEL_DIR)" || { echo "KERNEL_DIR must point to the prepared matching Rockchip kernel tree" >&2; exit 1; }
	rm -rf $(BUILD)/tsadc-recovery-module $(BUILD)/kernel-python-shim
	mkdir -p $(BUILD)/tsadc-recovery-module $(BUILD)/kernel-python-shim
	cp tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c tools/kernel/flip_tsadc_recovery/Makefile $(BUILD)/tsadc-recovery-module/
	ln -sf "$$(command -v python3)" $(BUILD)/kernel-python-shim/python
	PATH="$(CURDIR)/$(BUILD)/kernel-python-shim:$$PATH" \
		$(MAKE) -C "$(KERNEL_DIR)" M="$(CURDIR)/$(BUILD)/tsadc-recovery-module" \
		ARCH=arm64 CROSS_COMPILE="$(CROSS)" modules
	cp $(BUILD)/tsadc-recovery-module/flip_tsadc_recovery.ko $@

$(BUILD)/flip-perf-capture: $(PERF_CAPTURE_OBJS)
	$(CC) -static -o $@ $(PERF_CAPTURE_OBJS)

$(BUILD)/flip-gpu-sender: \
		src/runtime/flip_gpu_sender.c \
		src/runtime/flip_gpu_client.c \
		src/runtime/flip_gpu_protocol.c \
		include/flip_gpu_client.h \
		include/flip_gpu_protocol.h | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		src/runtime/flip_gpu_sender.c \
		src/runtime/flip_gpu_client.c \
		src/runtime/flip_gpu_protocol.c \
		-o $@

$(BUILD)/flip-wifi-tool: src/hwtest/flip_wifi_tool.c src/runtime/flip_wifi.c src/runtime/flip_wifi_profiles.c include/flip_wifi.h include/flip_wifi_profiles.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -static -Iinclude \
		src/hwtest/flip_wifi_tool.c src/runtime/flip_wifi.c src/runtime/flip_wifi_profiles.c -o $@

$(BUILD)/flip-charge-service: src/runtime/flip_charge_service.c src/runtime/flip_charger.c src/runtime/flip_power_policy.c src/runtime/flip_battery.c src/runtime/flip_log.c src/runtime/flip_paths.c include/flip_charger.h include/flip_power_policy.h include/flip_battery.h include/flip_log.h include/flip_paths.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -static -Iinclude \
		src/runtime/flip_charge_service.c src/runtime/flip_charger.c \
		src/runtime/flip_power_policy.c src/runtime/flip_battery.c \
		src/runtime/flip_log.c src/runtime/flip_paths.c -o $@

$(BUILD)/umtprd: $(UMTP_SOURCES) external/uMTP-Responder/LICENSE | $(BUILD)
	$(CC) -O3 -Wall -DFLIPOS_NO_POSIX_MQUEUE \
		-Iexternal/uMTP-Responder/inc -static \
		$(UMTP_SOURCES) -pthread -lrt -o $@
	$(CROSS)strip $@

$(BUILD)/flip-frontend: $(FRONTEND_OBJS)
	$(CC) -static -o $@ $(FRONTEND_OBJS) -pthread -lm

$(BUILD)/libgambatte_libretro.a: external/gambatte-libretro/Makefile.libretro | $(BUILD)
	$(MAKE) -C external/gambatte-libretro -f Makefile.libretro clean >/dev/null
	$(MAKE) -C external/gambatte-libretro -f Makefile.libretro -j$$(nproc) \
		platform=unix STATIC_LINKING=1 TARGET=$(abspath $@) \
		CC=$(CC) CXX=$(CXX) AR=$(CROSS)ar

$(BUILD)/mgba_libretro.a: external/mgba/CMakeLists.txt external/mgba/FLIPOS_UPSTREAM.txt | $(BUILD)
	cmake -S external/mgba -B $(BUILD)/mgba \
		-DCMAKE_SYSTEM_NAME=Linux \
		-DCMAKE_SYSTEM_PROCESSOR=aarch64 \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_LIBRETRO=ON -DM_CORE_GBA=ON -DM_CORE_GB=OFF \
		-DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
		-DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DBUILD_HEADLESS=OFF \
		-DBUILD_TEST=OFF -DBUILD_SUITE=OFF -DBUILD_PYTHON=OFF \
		-DBUILD_EXAMPLE=OFF -DUSE_DISCORD_RPC=OFF -DUSE_FFMPEG=OFF \
		-DUSE_LIBZIP=OFF -DUSE_SQLITE3=OFF -DUSE_LZMA=OFF \
		-DUSE_ZLIB=OFF -DUSE_PNG=OFF -DUSE_ELF=OFF
	cmake --build $(BUILD)/mgba --target mgba_libretro -j$$(nproc)
	cp $(BUILD)/mgba/mgba_libretro.a $@

$(BUILD)/flip-gb: $(GB_OBJS) $(BUILD)/libgambatte_libretro.a
	$(CXX) -static -o $@ \
		$(GB_OBJS) \
		$(BUILD)/libgambatte_libretro.a \
		-pthread \
		-lm

$(BUILD)/flip-gba: $(GBA_OBJS) $(BUILD)/mgba_libretro.a
	$(CXX) -static -o $@ \
		$(GBA_OBJS) \
		$(BUILD)/mgba_libretro.a \
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
		$(BUILD)/flip-gba \
		$(BUILD)/flip-gpu-service \
		$(BUILD)/flip-gpu-sender \
		$(BUILD)/flip-perf-capture \
		$(BUILD)/flip-cpu-policy \
		$(BUILD)/flip-optimizer-guard \
		$(BUILD)/flip-optimizer-supervisor \
		$(BUILD)/flip-optimizer-deadman \
		$(BUILD)/flip-optimizer-calibration \
		$(BUILD)/flip-optimizer-candidate \
		$(BUILD)/flip-optimizer-calibration-experimental \
		$(BUILD)/flip-optimizer-candidate-experimental \
		$(BUILD)/flip-optimizer-runtime \
		$(BUILD)/flip_opp_voltage.ko \
		$(BUILD)/flip_opp_runtime_profile.ko \
		$(BUILD)/libgambatte_libretro.a \
		$(BUILD)/mgba_libretro.a \
		$(BUILD)/test-flip-controls \
		$(BUILD)/test-flip-timing \
		$(BUILD)/test-flip-audio \
		$(BUILD)/test-flip-audio-queue \
		$(BUILD)/test-flip-audio-resampler \
		$(BUILD)/test-flip-paths \
		$(BUILD)/test-flip-log \
		$(BUILD)/test-flip-battery \
		$(BUILD)/test-flip-performance \
		$(BUILD)/test-flip-cpu-policy \
		$(BUILD)/test-flip-cpu-profile \
		$(BUILD)/test-flip-cpu-dynamic \
		$(BUILD)/test-flip-optimizer \
		$(BUILD)/test-flip-optimizer-pmu-token \
		$(BUILD)/test-flip-optimizer-quiesce \
		$(BUILD)/test-flip-optimizer-panic-policy \
		$(BUILD)/test-flip-optimizer-guard \
		$(BUILD)/test-flip-optimizer-supervisor \
		$(BUILD)/test-flip-optimizer-calibration \
		$(BUILD)/test-flip-optimizer-calibration-session \
		$(BUILD)/test-flip-bios \
		$(BUILD)/test-flip-library-cache \
		$(BUILD)/test-flip-exit-protocol \
		$(BUILD)/test-flip-lifecycle \
		$(BUILD)/test-flip-core \
		$(BUILD)/test-flip-core-host \
		$(BUILD)/test-flip-video-layout \
		$(BUILD)/test-flip-gpu-protocol \
		$(BUILD)/test-flip-gpu-client \
		$(BUILD)/test-flip-gpu-fake-service \
		$(BUILD)/test-flip-state \
		$(BUILD)/test-flip-state-manager \
		$(BUILD)/test-flip-state-recovery-health \
		$(BUILD)/test-flip-save \
		$(BUILD)/test-flip-save-manager \
		$(BUILD)/test-flip-recovery \
		$(BUILD)/test-flip-recovery-transactions \
		$(BUILD)/test-flip-session-persistence \
		$(BUILD)/test-flip-session-registry \
		$(BUILD)/test-flip-session-adapter \
		$(BUILD)/test-flip-system-registry \
		$(BUILD)/test-flip-session-adapter-save-safety \
		$(BUILD)/test-flip-perf-diag \
		$(BUILD)/test-flip-telemetry
	rm -rf $(BUILD)/mgba
	$(MAKE) -C external/gambatte-libretro -f Makefile.libretro clean >/dev/null
