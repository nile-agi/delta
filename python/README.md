# Delta CLI Python Wrapper

Python interface for scripting with Delta CLI.

## Installation

```bash
# Install Delta CLI first (see main README.md)

# Add to Python path or install as package
pip install -e python/
```

## Usage

### Basic Usage

```python
from delta_cli import DeltaCLI, DeltaConfig

# Create client
delta = DeltaCLI()

# Generate text
response = delta.generate("Explain machine learning in simple terms")
print(response)
```

### With Configuration

```python
# Custom configuration
config = DeltaConfig(
    model="llama-2-7b",
    temperature=0.7,
    max_tokens=1024,
    gpu_layers=-1  # Use all GPU layers
)

delta = DeltaCLI(config)
response = delta.generate("Write a poem about AI")
print(response)
```

### Chat Interface

```python
# Chat with conversation history
messages = [
    {"role": "user", "content": "Hello!"},
    {"role": "assistant", "content": "Hi! How can I help you today?"},
    {"role": "user", "content": "What is quantum computing?"}
]

response = delta.chat(messages)
print(response)
```

### List Models

```python
models = delta.list_models()
print("Available models:")
for model in models:
    print(f"  • {model}")
```

## Command Line

The Python wrapper can also be used from command line:

```bash
# Basic usage
python -m delta_cli "Explain neural networks"

# With options
python -m delta_cli \
    --model llama-2-7b \
    --temperature 0.7 \
    --tokens 1024 \
    --gpu-layers -1 \
    "Write a story about robots"

# List models
python -m delta_cli --list-models
```

## API Reference

### DeltaConfig

Configuration dataclass for Delta CLI.

**Attributes:**
- `model` (str, optional): Model name to use
- `temperature` (float): Sampling temperature (default: 0.8)
- `max_tokens` (int): Maximum tokens to generate (default: 512)
- `ctx_size` (int): Context size (default: 2048)
- `gpu_layers` (int): Number of GPU layers (default: 0)
- `binary_path` (str): Path to delta binary (default: "delta")

### DeltaCLI

Main client class.

#### `__init__(config: Optional[DeltaConfig] = None)`

Initialize the client with optional configuration.

#### `generate(prompt: str, **kwargs) -> str`

Generate text from a prompt.

**Args:**
- `prompt`: Input prompt
- `**kwargs`: Override config options

**Returns:**
- Generated text string

#### `chat(messages: List[Dict[str, str]], **kwargs) -> str`

Chat-style interaction with conversation history.

**Args:**
- `messages`: List of message dicts with 'role' and 'content'
- `**kwargs`: Override config options

**Returns:**
- Generated response string

#### `list_models() -> List[str]`

List available models.

**Returns:**
- List of model names

## Examples

### Script Automation

```python
#!/usr/bin/env python3
from delta_cli import DeltaCLI

delta = DeltaCLI()

# Process multiple prompts
prompts = [
    "Summarize the benefits of renewable energy",
    "List 5 programming best practices",
    "Explain REST APIs"
]

for prompt in prompts:
    print(f"\nPrompt: {prompt}")
    print("=" * 60)
    response = delta.generate(prompt, max_tokens=256)
    print(response)
    print()
```

### Interactive Assistant

```python
#!/usr/bin/env python3
from delta_cli import DeltaCLI

delta = DeltaCLI()
messages = []

print("Chat with Delta CLI (type 'exit' to quit)")

while True:
    user_input = input("\nYou: ")
    if user_input.lower() in ['exit', 'quit']:
        break
    
    messages.append({"role": "user", "content": user_input})
    
    response = delta.chat(messages)
    print(f"\nAssistant: {response}")
    
    messages.append({"role": "assistant", "content": response})
```

### Batch Processing

```python
#!/usr/bin/env python3
from delta_cli import DeltaCLI
import json

delta = DeltaCLI()

# Read prompts from file
with open('prompts.json', 'r') as f:
    data = json.load(f)

results = []

for item in data['prompts']:
    response = delta.generate(
        item['prompt'],
        temperature=item.get('temperature', 0.8),
        max_tokens=item.get('max_tokens', 512)
    )
    
    results.append({
        'prompt': item['prompt'],
        'response': response
    })

# Save results
with open('results.json', 'w') as f:
    json.dump(results, f, indent=2)
```

## Requirements

- Python 3.7+
- Delta CLI installed and in PATH
- Model files in `~/.delta-cli/models/`

## License

MIT License (same as Delta CLI)

