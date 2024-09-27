#!/bin/bash

# Execute no service tests
echo "==> Executing libssc no service"
meson test -C _build --verbose --print-errorlogs tests-no-service

# Run SSC Server
echo "==> Running SSC Server Regular Mode"
cd mocking
./ssc-server &
PID=$!
echo "SSC SERVER PID $PID"
sleep 1
cd ..

# Execute general tests
echo "==> Executing libssc tests"
meson test -C _build --verbose --print-errorlogs tests-general

# Stop SSC Server
echo "==> Stopping SSC Server"
sleep 1
kill "$PID"
