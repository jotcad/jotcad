#!/bin/bash
set -e

echo "[Root] Building C++ Geo Ops..."
(cd geo && ./build.sh)

echo "[Root] Building C++ CID Consistency Test..."
(cd geo/test && make bin/cid_consistency_test)

echo "[Root] Running Active JS Core Tests..."
# Use node --test for core libraries
xvfb-run -s '-ac -screen 0 1280x1024x24' node --test \
  fs/test/cid_reference.test.js \
  geo/test/cid_cpp.test.js \
  jot/test/provider.test.js

# Use vitest for UX components
echo "[Root] Running UX Tests..."
(cd ux && npm test)

echo "[Root] Running C++ Native Consistency Test..."
./geo/test/bin/cid_consistency_test

echo "[Root] Build and Core Verification Complete."
