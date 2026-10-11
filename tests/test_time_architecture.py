from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/runtime/flip_time.c").read_text()
header = (ROOT / "include/flip_time.h").read_text()
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()

for token in (
    '"/dev/rtc0"',
    'RTC_SET_TIME',
    'gmtime_r',
    'clock_settime(CLOCK_REALTIME',
    'mktime(&value)',
    '"/usr/share/zoneinfo"',
    'setenv("TZ", timezone, 1)',
    'tzset()',
    '"/etc/init.d/S49ntp"',
    '"-q"',
    '"-g"',
    'flip_time_write_rtc()',
):
    assert token in source, token

for zone in (
    '"Europe/London"',
    '"America/New_York"',
    '"Asia/Tokyo"',
    '"Australia/Sydney"',
    '"UTC"',
):
    assert zone in source, zone

for token in (
    'flip_time_set_timezone',
    'flip_time_set_automatic',
    'flip_time_sync_ntp',
    'flip_time_set_manual',
):
    assert token in header, token

assert '"DATE & TIME"' in frontend
assert '"AUTOMATIC TIME   %s"' in frontend
assert '"SYNC TIME NOW"' in frontend
assert '"SET DATE & TIME MANUALLY"' in frontend
assert '"LEFT/RIGHT TIME ZONE   A SELECT   B BACK"' in frontend
assert '"A SAVE   B CANCEL"' in frontend

print("time architecture tests passed")

assert 'apply_ntp_service_state' in source
assert '"/var/run/ntpd.pid"' in source
assert '(void)flip_time_set_automatic(time_status.automatic)' in frontend
assert 'status.automatic' in source

assert 'RTC_RD_TIME' in source
assert 'timegm(&utc)' in source
assert 'restore_system_from_rtc()' in source
assert 'run_process_timeout(sync_argv, 12000)' in source
assert 'kill(pid, SIGTERM)' in source
assert 'kill(pid, SIGKILL)' in source
