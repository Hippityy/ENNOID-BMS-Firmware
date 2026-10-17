#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
export ASAN_OPTIONS=detect_leaks=0
bash tests/run_mapping_tests.sh
python3 tests/protocol/run.py "${1:-../bms-tool/res/config.xml}"
bash tests/storage/run.sh
bash tests/run_can_glue_tests.sh
test_dir=$(mktemp -d /tmp/ennoid-transport-tests.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
gcc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -IModules/Inc Modules/Src/modCANTransport.c tests/test_can_transport.c -o "$test_dir/test"
"$test_dir/test"
