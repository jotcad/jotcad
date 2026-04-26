#!/bin/bash
# cleanup_ports.sh - Surgically kill processes on test ports using env vars

UX_PORT=${TEST_UX_PORT:-3030}
OPS_PORT=${TEST_OPS_PORT:-9092}
CPP_PORT=${TEST_CPP_PORT:-20101}
JS_PORT=${TEST_JS_PORT:-20102}

echo "[Cleanup] Freeing ports: UX:$UX_PORT, OPS:$OPS_PORT, CPP:$CPP_PORT, JS:$JS_PORT..."

# Use fuser to kill processes on specified ports
fuser -k $UX_PORT/tcp $OPS_PORT/tcp $CPP_PORT/tcp $JS_PORT/tcp 2>/dev/null || true

# Wait a moment for ports to settle
sleep 1
