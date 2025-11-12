#!/usr/bin/env python3
"""
Setup script for Delta CLI Python wrapper
"""

from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="delta-cli",
    version="1.0.0",
    author="Delta CLI Team",
    author_email="your.email@example.com",
    description="Python wrapper for Delta CLI - offline AI assistant",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/yourusername/delta-cli",
    packages=find_packages(),
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    python_requires=">=3.7",
    install_requires=[],
    entry_points={
        "console_scripts": [
            "delta-cli-py=delta_cli:main",
        ],
    },
)

