#!/bin/bash

echo "=== Testing Model Switch in Delta Web UI ==="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Check current server model
echo "Step 1: Checking current server model..."
CURRENT_MODEL=$(curl -s http://localhost:8080/v1/models | python3 -c "import sys, json; data = json.load(sys.stdin); print(data['data'][0]['id'] if data.get('data') else 'N/A')" 2>/dev/null)
echo "Current server model: ${GREEN}$CURRENT_MODEL${NC}"
echo ""

# Step 2: Get available models
echo "Step 2: Getting available models from model API..."
AVAILABLE_MODELS=$(curl -s http://localhost:8081/api/models/list)
echo "Available models:"
echo "$AVAILABLE_MODELS" | python3 -c "import sys, json; data = json.load(sys.stdin); [print(f\"  - {m['name']}: {m['display_name']}\") for m in data.get('models', [])]" 2>/dev/null
echo ""

# Step 3: Select a different model (if available)
echo "Step 3: Attempting to switch to a different model..."
# Try to switch to qwen3:0.6b if current is smollm, or vice versa
if [[ "$CURRENT_MODEL" == *"SmolLM"* ]]; then
    SWITCH_TO="qwen3:0.6b"
    echo "Switching from SmolLM to Qwen 3 0.6B..."
else
    SWITCH_TO="smollm:135m"
    echo "Switching to SmolLM 135M..."
fi

SWITCH_RESPONSE=$(curl -s -X POST http://localhost:8081/api/models/use \
    -H "Content-Type: application/json" \
    -d "{\"model\":\"$SWITCH_TO\"}")

echo "Switch response:"
echo "$SWITCH_RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$SWITCH_RESPONSE"
echo ""

# Step 4: Check if server model changed
echo "Step 4: Checking if server model actually changed..."
NEW_MODEL=$(curl -s http://localhost:8080/v1/models | python3 -c "import sys, json; data = json.load(sys.stdin); print(data['data'][0]['id'] if data.get('data') else 'N/A')" 2>/dev/null)
echo "Server model after switch: ${GREEN}$NEW_MODEL${NC}"

if [[ "$CURRENT_MODEL" == "$NEW_MODEL" ]]; then
    echo "${YELLOW}⚠️  WARNING: Server model did NOT change!${NC}"
    echo "   This confirms that llama-server does not support dynamic model switching."
else
    echo "${GREEN}✓ Server model changed!${NC}"
fi
echo ""

# Step 5: Send a test chat request with model field
echo "Step 5: Sending test chat request with selected model in request body..."
TEST_PROMPT="What is 2+2? Answer in one word."
CHAT_RESPONSE=$(curl -s -X POST http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d "{
        \"model\": \"$SWITCH_TO\",
        \"messages\": [
            {\"role\": \"user\", \"content\": \"$TEST_PROMPT\"}
        ],
        \"max_tokens\": 10,
        \"temperature\": 0
    }")

echo "Chat request sent with model: $SWITCH_TO"
echo "Response:"
echo "$CHAT_RESPONSE" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    if 'choices' in data and len(data['choices']) > 0:
        content = data['choices'][0]['message']['content']
        model_used = data.get('model', 'N/A')
        print(f\"Model used in response: {model_used}\")
        print(f\"Response content: {content}\")
    else:
        print(json.dumps(data, indent=2))
except Exception as e:
    print(f\"Error: {e}\")
    print(sys.stdin.read())
" 2>/dev/null || echo "$CHAT_RESPONSE"
echo ""

# Step 6: Compare models
echo "Step 6: Analysis..."
echo "Selected model (via API): $SWITCH_TO"
echo "Server model (actual): $NEW_MODEL"
echo "Model in chat response: (see above)"
echo ""

if [[ "$CURRENT_MODEL" == "$NEW_MODEL" ]]; then
    echo "${RED}❌ CONCLUSION: Model switching does NOT change the actual running model.${NC}"
    echo "   The server continues to use the model loaded at startup."
    echo "   The 'model' field in requests is only metadata."
else
    echo "${GREEN}✓ CONCLUSION: Model switching DOES change the running model!${NC}"
fi

