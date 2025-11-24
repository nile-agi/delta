#!/bin/bash
echo "Testing Model Switch Flow..."
echo ""

# Test 1: Check if delta-server builds
echo "Step 1: Building delta-server..."
cd build_macos
if make delta-server 2>&1 | grep -q "Built target delta-server"; then
    echo "✓ Build successful"
else
    echo "✗ Build failed"
    exit 1
fi

# Test 2: Check if the callback mechanism is in place
echo ""
echo "Step 2: Checking callback mechanism..."
if grep -q "set_model_switch_callback" ../src/delta_server_wrapper.cpp && \
   grep -q "restart_llama_server" ../src/delta_server_wrapper.cpp; then
    echo "✓ Callback mechanism found"
else
    echo "✗ Callback mechanism missing"
    exit 1
fi

# Test 3: Check if model API server has the callback
echo ""
echo "Step 3: Checking model API server..."
if grep -q "g_model_switch_callback" ../src/model_api_server.cpp && \
   grep -q "set_model_switch_callback" ../src/model_api_server.h; then
    echo "✓ Model API server callback found"
else
    echo "✗ Model API server callback missing"
    exit 1
fi

echo ""
echo "✓ All checks passed! The implementation should work."
