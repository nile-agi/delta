#!/usr/bin/env python3
"""
Delta CLI Python Wrapper
Provides a Python interface for scripting with Delta CLI
"""

import subprocess
import json
import os
import sys
from typing import Optional, List, Dict, Any
from dataclasses import dataclass


@dataclass
class DeltaConfig:
    """Configuration for Delta CLI"""
    model: Optional[str] = None
    temperature: float = 0.8
    max_tokens: int = 512
    ctx_size: int = 2048
    gpu_layers: int = 0
    binary_path: str = "delta"


class DeltaCLI:
    """Python wrapper for Delta CLI"""
    
    def __init__(self, config: Optional[DeltaConfig] = None):
        """
        Initialize Delta CLI wrapper
        
        Args:
            config: Optional configuration. Uses defaults if not provided.
        """
        self.config = config or DeltaConfig()
        self._check_binary()
    
    def _check_binary(self):
        """Check if Delta CLI binary is available"""
        try:
            result = subprocess.run(
                [self.config.binary_path, "--version"],
                capture_output=True,
                text=True,
                check=False
            )
            if result.returncode != 0:
                raise RuntimeError(f"Delta CLI not found at: {self.config.binary_path}")
        except FileNotFoundError:
            raise RuntimeError(f"Delta CLI not found. Please install or set binary_path.")
    
    def generate(self, prompt: str, **kwargs) -> str:
        """
        Generate text from a prompt
        
        Args:
            prompt: Input prompt
            **kwargs: Override config options (model, temperature, max_tokens, etc.)
        
        Returns:
            Generated text
        """
        cmd = [self.config.binary_path]
        
        # Add configuration options
        model = kwargs.get('model', self.config.model)
        if model:
            cmd.extend(['--model', model])
        
        temp = kwargs.get('temperature', self.config.temperature)
        cmd.extend(['--temperature', str(temp)])
        
        tokens = kwargs.get('max_tokens', self.config.max_tokens)
        cmd.extend(['--tokens', str(tokens)])
        
        ctx = kwargs.get('ctx_size', self.config.ctx_size)
        cmd.extend(['--ctx-size', str(ctx)])
        
        gpu = kwargs.get('gpu_layers', self.config.gpu_layers)
        cmd.extend(['--gpu-layers', str(gpu)])
        
        # Add prompt
        cmd.append(prompt)
        
        # Execute
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            return result.stdout.strip()
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Delta CLI error: {e.stderr}")
    
    def list_models(self) -> List[str]:
        """
        List available models
        
        Returns:
            List of model names
        """
        try:
            result = subprocess.run(
                [self.config.binary_path, "--list-models"],
                capture_output=True,
                text=True,
                check=True
            )
            # Parse output
            models = []
            for line in result.stdout.split('\n'):
                line = line.strip()
                if line and line.startswith('•'):
                    # Extract model name from "• model-name (size)"
                    model = line.split('•')[1].split('(')[0].strip()
                    models.append(model)
            return models
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Delta CLI error: {e.stderr}")
    
    def chat(self, messages: List[Dict[str, str]], **kwargs) -> str:
        """
        Chat-style interaction with conversation history
        
        Args:
            messages: List of message dicts with 'role' and 'content'
            **kwargs: Override config options
        
        Returns:
            Generated response
        
        Example:
            >>> delta = DeltaCLI()
            >>> messages = [
            ...     {"role": "user", "content": "Hello!"},
            ...     {"role": "assistant", "content": "Hi! How can I help?"},
            ...     {"role": "user", "content": "What is AI?"}
            ... ]
            >>> response = delta.chat(messages)
        """
        # Build prompt from messages
        prompt = ""
        for msg in messages:
            role = msg.get('role', 'user')
            content = msg.get('content', '')
            if role == 'user':
                prompt += f"User: {content}\n"
            elif role == 'assistant':
                prompt += f"Assistant: {content}\n"
        
        prompt += "Assistant:"
        
        return self.generate(prompt, **kwargs)


def main():
    """Command-line interface for Python wrapper"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Delta CLI Python Wrapper")
    parser.add_argument('prompt', nargs='?', help="Prompt to generate from")
    parser.add_argument('--model', help="Model to use")
    parser.add_argument('--temperature', type=float, default=0.8, help="Sampling temperature")
    parser.add_argument('--tokens', type=int, default=512, help="Max tokens to generate")
    parser.add_argument('--gpu-layers', type=int, default=0, help="Number of GPU layers")
    parser.add_argument('--list-models', action='store_true', help="List available models")
    parser.add_argument('--binary', default='delta', help="Path to delta binary")
    
    args = parser.parse_args()
    
    # Create config
    config = DeltaConfig(
        model=args.model,
        temperature=args.temperature,
        max_tokens=args.tokens,
        gpu_layers=args.gpu_layers,
        binary_path=args.binary
    )
    
    try:
        delta = DeltaCLI(config)
        
        if args.list_models:
            models = delta.list_models()
            print("Available models:")
            for model in models:
                print(f"  • {model}")
        elif args.prompt:
            response = delta.generate(args.prompt)
            print(response)
        else:
            parser.print_help()
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()

