#!/usr/bin/env bash
set -euo pipefail
# LeakSanitizer cannot run under the managed ptrace runner; these tests allocate no heap.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d /tmp/ennoid-mapping-tests.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
includes=(-IMain -IDrivers/CMSIS/Device/ST/STM32F3xx/Include -IDrivers/CMSIS/Include
    -IDrivers/HWDrivers/Inc -IDrivers/STM32F3xx_HAL_Driver/Inc -IDrivers/SWDrivers/Inc
    -ILibraries/Inc -IModules/Inc -ICubeMX/Inc -IConfig)
flags=(-std=gnu99 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
    -fno-pie -no-pie -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast -DSTM32F303xC -DUSE_HAL_DRIVER -DENNOID_HV_2=1)
gcc "${flags[@]}" "${includes[@]}" Modules/Src/modCellMap.c tests/test_cell_map.c -o "$build_dir/map"
"$build_dir/map"
gcc "${flags[@]}" "${includes[@]}" -Wl,--gc-sections Modules/Src/modCellMap.c \
    Modules/Src/modPowerElectronics.c tests/test_power_electronics.c -lm -o "$build_dir/power"
"$build_dir/power"
