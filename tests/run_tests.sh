#!/usr/bin/env bash
# Host-side protocol tests. Run from anywhere; requires a C11 compiler.
set -euo pipefail

cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
mkdir -p .build
"$CC" -std=c11 -Wall -Wextra -O2 -Iinclude \
  tests/test_protocol.c \
  src/protocol/n2k_codec.c \
  src/protocol/ac42_protocol.c \
  -lm -o .build/test_protocol
./.build/test_protocol
