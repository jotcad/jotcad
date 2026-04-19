#!/bin/bash

# JotCAD Start All Script
# This script orchestrates the VFS Hub, C++ Ops Node, Export Node, and the Interactive UX.

# Cleanup on exit
trap 'kill 0' EXIT

echo "[1/4] Starting VFS Hub on http://localhost:9090..."
PORT=9090 NEIGHBORS="http://localhost:9091,http://localhost:9092" npm run start:vfs &

# Wait for Hub to be ready
sleep 2

echo "[3/4] Starting C++ Geometry Ops Node on http://localhost:9091..."
PEER_ID=geo-ops-node PORT=9091 NEIGHBORS="http://localhost:9090/vfs,http://localhost:9092" ./geo/bin/ops &

echo "[4/4] Starting Export Node on http://localhost:9092..."
PEER_ID=export-node PORT=9092 NEIGHBORS="http://localhost:9090/vfs,http://localhost:9091" node geo/export_service.js &

echo "[5/5] Starting Interactive UX on http://localhost:3030..."
cd ux && npm run dev &

echo "-------------------------------------------------------"
echo "All systems operational."
echo "- Hub: http://localhost:9090/vfs"
echo "- Ops: http://localhost:9091"
echo "- Export: http://localhost:9092"
echo "- UX:  http://localhost:3030"
echo "-------------------------------------------------------"

# Keep script alive
wait
