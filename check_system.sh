#!/bin/bash

echo "=== System Components Check ==="
echo ""

# Check turnstiled
if pgrep -x "turnstiled" > /dev/null; then
    echo "✓ turnstiled is running"
    ls -la /run/turnstiled/ 2>/dev/null | head -5
else
    echo "✗ turnstiled is not running"
    echo "  Start with: sudo turnstiled"
fi

# Check seatd
if pgrep -x "seatd" > /dev/null; then
    echo "✓ seatd is running"
else
    echo "✗ seatd is not running"
fi

# Check dinit
if pgrep -x "dinit" > /dev/null; then
    echo "✓ dinit is running"
else
    echo "✗ dinit is not running"
fi

# Check dinit-dbus
if pgrep -f "dinit-dbus" > /dev/null; then
    echo "✓ dinit-dbus is running"
else
    echo "✗ dinit-dbus is not running"
fi

# Check turnstile-dbus
if pgrep -f "turnstile-dbus" > /dev/null; then
    echo "✓ turnstile-dbus is running"
else
    echo "✗ turnstile-dbus is not running"
fi

# Check library
if [ -f /usr/lib/x86_64-linux-gnu/libturnstile.so.0.1.11 ]; then
    echo "✓ libturnstile.so found"
else
    echo "✗ libturnstile.so not found"
fi

echo ""
echo "=== Socket Check ==="
ls -la /run/turnstiled/socket 2>/dev/null || echo "Socket not found"

echo ""
echo "=== Sessions Check ==="
ls -la /run/turnstiled/sessions/ 2>/dev/null || echo "No sessions directory"
