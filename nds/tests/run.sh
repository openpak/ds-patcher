#!/bin/sh
# Host test of the ROM patch/backup/restore flow (synthetic ROMs, no dumps needed).
set -e
cd "$(dirname "$0")"
d=$(mktemp -d "${TMPDIR:-.}/ndstest.XXXXXX")
python3 -I mkrom.py "$d/game.nds"
python3 -I mkrom.py "$d/bare.nds" nowfc
g++ -std=c++17 -Wall -o "$d/flow" flow_test.cpp ../source/ndspatch.cpp ../source/blz.cpp
"$d/flow" "$d/game.nds" "$d/bare.nds"
rm -rf "$d"
