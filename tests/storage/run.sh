#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
output=$(mktemp /tmp/ennoid-storage-test.XXXXXX)
trap 'rm -f "$output"' EXIT HUP INT TERM
cc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -DENNOID_HV_2=1 -Itests/storage/stubs -IDrivers/SWDrivers/Inc -IModules/Inc -IMain \
  tests/storage/test_storage.c Drivers/SWDrivers/Src/driverSWStorageManager.c \
  Modules/Src/modConfig.c Modules/Src/modCellMap.c -lm -o "$output"
# This runner can execute under ptrace; LeakSanitizer cannot. The production
# manager uses no heap. Keep address and undefined-behavior checks enabled.
ASAN_OPTIONS=detect_leaks=0 "$output"
