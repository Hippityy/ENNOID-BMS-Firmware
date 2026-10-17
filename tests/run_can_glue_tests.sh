#!/usr/bin/env bash
set -euo pipefail
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d /tmp/ennoid-can-glue-tests.XXXXXX)
trap 'rm -rf "$build_dir"' EXIT
includes=(-IMain -IDrivers/CMSIS/Device/ST/STM32F3xx/Include -IDrivers/CMSIS/Include
    -IDrivers/HWDrivers/Inc -IDrivers/STM32F3xx_HAL_Driver/Inc -IDrivers/SWDrivers/Inc
    -ILibraries/Inc -IModules/Inc -ICubeMX/Inc -IConfig)
gcc -std=gnu99 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
    -fno-pie -no-pie -ffunction-sections -fdata-sections -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -DSTM32F303xC -DUSE_HAL_DRIVER -DENNOID_HV_2=1 \
    "${includes[@]}" -Wl,--gc-sections tests/test_can_glue.c Modules/Src/modCANTransport.c \
    Libraries/Src/libBuffer.c Libraries/Src/libCRC.c -lm -o "$build_dir/glue"
"$build_dir/glue"
