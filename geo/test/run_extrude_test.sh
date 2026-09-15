#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
make -C "$DIR" bin/extrude_polyloop_test
"$DIR/bin/extrude_polyloop_test"
