#!/bin/bash
# Test script for model switching functionality

set -e

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    🧪 TESTING MODEL SWITCHING FUNCTIONALITY                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if server is running
echo "Step 1: Checking if servers are running..."
if ! curl -s http://localhost:8080 > /dev/null 2>&1; then
    echo -e "${RED}✗ Web UI server (port 8080) is not running${NC}"
    echo "   Please start the server with: ./delta server"
    exit 1
fi
echo -e "${GREEN}✓ Web UI server is running${NC}"

if ! curl -s http://localhost:8081/api/models/list > /dev/null 2>&1; then
    echo -e "${RED}✗ Model API server (port 8081) is not running${NC}"
    echo "   Please start the server with: ./delta server"
    exit 1
fi
echo -e "${GREEN}✓ Model API server is running${NC}"
echo ""

# Test 1: Check installed models
echo "Step 2: Testing installed models API..."
INSTALLED_MODELS=$(curl -s http://localhost:8081/api/models/list)
MODEL_COUNT=$(echo "$INSTALLED_MODELS" | jq '. | length' 2>/dev/null || echo "$INSTALLED_MODELS" | grep -o '"name"' | wc -l | tr -d ' ')

if [ "$MODEL_COUNT" -eq "0" ]; then
    echo -e "${YELLOW}⚠ No models installed${NC}"
    echo "   Install models with: delta pull <model-name>"
else
    echo -e "${GREEN}✓ Found $MODEL_COUNT installed model(s)${NC}"
    echo "$INSTALLED_MODELS" | jq -r '.[] | "   - \(.display_name) (\(.name))"' 2>/dev/null || echo "$INSTALLED_MODELS" | grep -o '"display_name":"[^"]*"' | head -5
fi
echo ""

# Test 2: Check /v1/models endpoint
echo "Step 3: Testing /v1/models endpoint (current model)..."
CURRENT_MODEL=$(curl -s http://localhost:8080/v1/models 2>/dev/null || echo "{}")
if echo "$CURRENT_MODEL" | jq -e '.data[0]' > /dev/null 2>&1; then
    MODEL_ID=$(echo "$CURRENT_MODEL" | jq -r '.data[0].id')
    echo -e "${GREEN}✓ Current model: $MODEL_ID${NC}"
else
    echo -e "${YELLOW}⚠ Could not fetch current model from /v1/models${NC}"
fi
echo ""

# Test 3: Check if models have display names
echo "Step 4: Verifying display names..."
HAS_DISPLAY_NAMES=$(echo "$INSTALLED_MODELS" | jq -r '.[0].display_name' 2>/dev/null || echo "")
if [ -n "$HAS_DISPLAY_NAMES" ] && [ "$HAS_DISPLAY_NAMES" != "null" ]; then
    echo -e "${GREEN}✓ Models have display names${NC}"
    echo "   Example: $(echo "$INSTALLED_MODELS" | jq -r '.[0].display_name' 2>/dev/null)"
else
    echo -e "${YELLOW}⚠ Some models may be missing display names${NC}"
fi
echo ""

# Test 4: Test model switch API (if models available)
if [ "$MODEL_COUNT" -gt "1" ]; then
    echo "Step 5: Testing model switch API..."
    FIRST_MODEL=$(echo "$INSTALLED_MODELS" | jq -r '.[0].name' 2>/dev/null || echo "")
    if [ -n "$FIRST_MODEL" ] && [ "$FIRST_MODEL" != "null" ]; then
        SWITCH_RESULT=$(curl -s -X POST http://localhost:8081/api/models/use \
            -H "Content-Type: application/json" \
            -d "{\"model\": \"$FIRST_MODEL\"}" 2>/dev/null || echo "{}")
        
        if echo "$SWITCH_RESULT" | jq -e '.success' > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Model switch API is working${NC}"
            SERVER_RESTARTED=$(echo "$SWITCH_RESULT" | jq -r '.server_restarted' 2>/dev/null || echo "false")
            if [ "$SERVER_RESTARTED" = "true" ]; then
                echo -e "${GREEN}✓ Server restart is working${NC}"
            fi
        else
            echo -e "${YELLOW}⚠ Model switch API returned unexpected response${NC}"
        fi
    fi
else
    echo "Step 5: Skipping model switch test (need at least 2 models)"
fi
echo ""

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║    ✅ API TESTS COMPLETE                                      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""
echo "Next steps:"
echo "1. Open http://localhost:8080 in your browser"
echo "2. Check the model dropdown - should show all installed models"
echo "3. Verify model names are display names (e.g., 'Qwen 3 1.7B') not filenames"
echo "4. Try switching models"
echo "5. Refresh the page after switching - should work without errors"
echo ""

