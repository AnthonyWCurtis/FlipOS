# Phase 7K Performance Profiles Service Design

Date: 2026-10-04

## Goal

Create a shared, hardware-backed Performance Profiles service without prematurely defining the later Phase 10 per-system performance policy.

## Verified hardware controls

Miyoo Flip exposes one CPU cpufreq policy:

- governors: interactive, conservative, ondemand, userspace, powersave, performance, schedutil
- verified current governor: ondemand
- frequency range: 408000-1992000 kHz

The Mali GPU devfreq node exposes:

- governors including simple_ondemand, powersave and performance
- verified current governor: simple_ondemand
- frequency range: 200000000-900000000 Hz

Phase 7K changes governors only. It does not change min/max frequency limits.

## Profiles

BALANCED:
- CPU: ondemand
- GPU: simple_ondemand

PERFORMANCE:
- CPU: performance
- GPU: performance

POWERSAVE:
- CPU: powersave
- GPU: powersave

A profile is considered supported only if both required governors are listed by the real hardware.

## Safety and rollback

Before applying a profile, the service snapshots both current governors.

CPU is written first, then GPU.

If the GPU write fails after CPU changed, CPU is restored to its previous value before the call fails.

The public snapshot/restore API also allows callers and hardware tests to return to the exact pre-test governor pair.

## Deferred policy

Phase 7K deliberately does not:

- change CPU/GPU min or max frequency;
- undervolt;
- touch PMIC registers;
- automatically select profiles by emulator/system;
- persist a user profile setting;
- define thermal or battery-driven switching policy.

Those belong to Phase 10 Performance / Power once real workload data justifies them.

## Verification

Host tests cover:

- profile naming and current-governor query;
- supported governor detection;
- Balanced/Performance/Powersave application;
- explicit restore;
- unsupported profiles remaining non-destructive;
- partial-write CPU rollback.

Hardware acceptance applies each verified governor pair briefly and restores the exact starting pair.
