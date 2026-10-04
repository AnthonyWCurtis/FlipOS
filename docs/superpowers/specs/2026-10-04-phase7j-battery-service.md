# Phase 7J Formal Battery Service Design

Date: 2026-10-04

## Goal

Create one read-only Battery service for Miyoo Flip power-supply state and make Telemetry consume it instead of maintaining a separate battery sysfs probe.

## Hardware source

The verified RK3566 device exposes battery state beneath:

`/sys/class/power_supply/battery`

Observed useful fields include `capacity`, `status`, `voltage_now`, `current_now`, and `temp`.

## Service contract

`flip_battery_read()` returns a snapshot with independent validity flags for:

- percentage (0-100)
- charging state
- voltage in microvolts
- current in microamps
- temperature in tenths of a degree Celsius

Supported charging states are UNKNOWN, CHARGING, DISCHARGING, NOT_CHARGING and FULL.

A readable service root with missing or malformed optional values is not a fatal error. Invalid fields remain unavailable while valid fields are still returned.

The service is read-only. It never writes power-supply sysfs.

## Ownership

Battery owns parsing and validation of battery sysfs.

Telemetry consumes Battery and retains its existing public `battery_percent/battery_valid` overlay contract. Telemetry no longer discovers or stores a private battery path.

This leaves the richer Battery snapshot available to frontend policy and future performance/power work without coupling those consumers to Linux sysfs layout.

## Safety

- no NAND/MTD access
- no `/usr/miyoo` changes
- no charger or PMIC writes
- no shutdown policy changes
- no guessed time-remaining calculation in this phase

## Verification

Host tests cover complete snapshots, missing fields, malformed/out-of-range fields and status names.

Architecture checks require Telemetry to consume Battery and prohibit its former private battery discovery.

Hardware acceptance compares the Battery service snapshot with raw sysfs values and verifies normal gameplay telemetry remains valid.
