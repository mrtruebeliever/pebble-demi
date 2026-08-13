#!/bin/bash
# Works around the WSL2 qemu-pebble timer-IRQ stall: without -icount the
# emulator's timer interrupts stop firing after a minute or two, which freezes
# the clock, kills app timers, and makes installs and screenshots time out
# while buttons and rendering still appear to work.
#
# pebble-tool has no hook for extra qemu arguments, only the PEBBLE_QEMU_PATH
# environment variable, so the flag has to be injected by standing in for the
# binary. Export this before starting the emulator:
#
#   export PEBBLE_QEMU_PATH="$PWD/tools/qemu-icount-wrapper.sh"
#   pebble install --emulator emery
set -eu

QEMU="${PEBBLE_SDK_HOME:-$HOME/.pebble-sdk}/SDKs/current/toolchain/bin/qemu-pebble"
if [ ! -x "$QEMU" ]; then
  echo "qemu-icount-wrapper: no qemu-pebble at $QEMU" >&2
  echo "Set PEBBLE_SDK_HOME if your SDK lives somewhere other than ~/.pebble-sdk." >&2
  exit 1
fi

exec "$QEMU" -icount shift=auto,align=off,sleep=on "$@"
