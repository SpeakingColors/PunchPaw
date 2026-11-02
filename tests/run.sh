#!/bin/sh
# Build and run the core tests (no JUCE needed).
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined -o /tmp/pp_test tests/test_core.cpp
/tmp/pp_test
