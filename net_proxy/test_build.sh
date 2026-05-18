#!/bin/bash

echo "========================================="
echo "    Net Proxy - Build & Test Script"
echo "========================================="

cd "$(dirname "$0")"

echo ""
echo "[1] Cleaning previous build..."
make clean 2>/dev/null

echo ""
echo "[2] Building net_proxy..."
make

if [ $? -ne 0 ]; then
    echo ""
    echo "[FAIL] Build failed!"
    exit 1
fi

echo ""
echo "[3] Binary info..."
ls -la net_proxy
file net_proxy 2>/dev/null || echo "file command not available"

echo ""
echo "[4] Static analysis (basic)..."
# Check for obvious issues
echo "Checking for TODO/FIXME comments:"
grep -n "TODO\|FIXME\|XXX" net_proxy.c && echo "Found TODO/FIXME" || echo "No TODO/FIXME found"

echo ""
echo "Checking for unsafe functions:"
grep -n "strcpy\|sprintf" net_proxy.c && echo "Found unsafe functions" || echo "No unsafe functions found"

echo ""
echo "========================================="
echo "    Build Complete"
echo "========================================="
echo ""
echo "To run the server:"
echo "  cd net_proxy && ./net_proxy"
echo ""
echo "Note: Server requires IPs 192.168.3.10 and 10.10.111.10"
echo "      to be configured on the system."
echo ""
echo "To test locally (without proper IPs configured):"
echo "  sudo ./net_proxy  # may fail to bind"
echo ""