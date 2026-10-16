#!/bin/sh
# FlipOS RK817 identity and power_supply inventory. READ-ONLY; NO PMIC WRITES.
# Run manually; stdout is a local report and is never uploaded.
set -eu

ROOT=$(printenv FLIP_CHARGE_PROBE_ROOT 2>/dev/null || :)
case "$ROOT" in
    "") ;;
    /*) ;;
    *) echo "FLIP_CHARGE_PROBE_ROOT must be absolute" >&2; exit 2 ;;
esac

field()
{
    label=$1
    relative=$2
    p="$ROOT$relative"
    if [ -f "$p" ] && [ -r "$p" ]; then
        # Fixed allowlisted paths, first line only, bounded printable ASCII.
        value=$(LC_ALL=C head -n 1 "$p" 2>/dev/null |
            LC_ALL=C cut -c 1-80 |
            LC_ALL=C tr -cd '[:print:]' || :)
        if [ -n "$value" ]; then
            printf '%s=%s\n' "$label" "$value"
            return
        fi
    fi
    printf '%s=unavailable\n' "$label"
}

printf 'FLIP_CHARGE_READONLY_INVENTORY_V1\n'
if [ -z "$ROOT" ]; then
    printf 'source=live_device\n'
    printf 'kernel_release=%s\n' "$(uname -r 2>/dev/null || echo unavailable)"
else
    printf 'source=synthetic_fixture\n'
    printf 'kernel_release=not_collected_in_fixture\n'
fi
printf 'register_writes=none\n'
printf 'network_uploads=none\n'

field module_limit_ma /sys/module/flip_rk817_charger/parameters/current_limit_ma
field module_stock_ma /sys/module/flip_rk817_charger/parameters/stock_current_ma
field battery_status /sys/class/power_supply/battery/status
field battery_temp_deci_c /sys/class/power_supply/battery/temp
field battery_current_ua /sys/class/power_supply/battery/current_now
field battery_voltage_uv /sys/class/power_supply/battery/voltage_now
field battery_capacity_percent /sys/class/power_supply/battery/capacity
field ac_online /sys/class/power_supply/ac/online
field usb_online /sys/class/power_supply/usb/online

for name in rk817-charger rk817-battery rk817_battery; do
    if [ -d "$ROOT/sys/bus/platform/drivers/$name" ]; then
        printf 'platform_driver_%s=present\n' "$name"
    fi
done

if [ -r "$ROOT/proc/modules" ]; then
    awk '($1 == "flip_rk817_charger" ||
           $1 == "rk817_charger" ||
           $1 == "rk817_battery") {
        printf "loaded_module_%s=yes\n", $1
    }' "$ROOT/proc/modules"
fi

module="$ROOT/mnt/SDCARD/flipos-dev/lib/flip_rk817_charger.ko"
if [ -f "$module" ] && [ -r "$module" ] && command -v sha256sum >/dev/null 2>&1; then
    hash=$(sha256sum "$module" 2>/dev/null | awk '{print $1}' || :)
    case "$hash" in
        *[!0-9a-f]*|"") printf 'sd_module_sha256=unavailable\n' ;;
        *) if [ "$(printf '%s' "$hash" | wc -c)" -eq 64 ]; then
               printf 'sd_module_sha256=%s\n' "$hash"
           else
               printf 'sd_module_sha256=unavailable\n'
           fi ;;
    esac
else
    printf 'sd_module_sha256=unavailable\n'
fi

printf 'note=requested_charge_cap_is_not_measured_battery_current\n'
printf 'note=driver_presence_does_not_prove_binding_or_safety\n'
