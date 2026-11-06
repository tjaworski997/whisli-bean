#!/usr/bin/env bash
# Everything that can be checked without the board: the decoder, gestures and the panel's types.
# Usage: scripts/test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

c++ -std=c++17 -Wall -Wextra -Isrc test/decode_test.cpp src/ir_decode.cpp -o "$out/decode_test"
"$out/decode_test" | tail -1
c++ -std=c++17 -Wall -Wextra -Isrc test/gesture_test.cpp -o "$out/gesture_test"
"$out/gesture_test" | tail -1
(cd ui && npx tsc --noEmit)
echo "panel types ok"
