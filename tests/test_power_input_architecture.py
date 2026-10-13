from pathlib import Path

source = Path("src/runtime/flip_input.c").read_text()

assert '#define POWER_DEVICE   "/dev/input/event2"' in source
assert 'static int power_fd = -1;' in source
assert 'open(POWER_DEVICE, O_RDONLY | O_NONBLOCK)' in source
assert 'fds[2].fd = power_fd;' in source
assert 'ev.code != KEY_POWER' in source
assert 'state->down[FLIP_BTN_POWER]' in source

print("power input architecture tests passed")
