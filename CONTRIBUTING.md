# Contributing to Delta CLI

Thank you for your interest in contributing to Delta CLI! This document provides guidelines and instructions for contributing.

## 🤝 How to Contribute

### Reporting Bugs

If you find a bug, please create an issue with:
- Clear description of the problem
- Steps to reproduce
- Expected vs actual behavior
- System information (OS, architecture, GPU)
- Model being used (if applicable)

### Suggesting Features

Feature requests are welcome! Please include:
- Clear description of the feature
- Use case and motivation
- Possible implementation approach
- Any relevant examples or mockups

### Submitting Code

1. **Fork the repository**
   ```bash
   git clone https://github.com/yourusername/delta-cli.git
   cd delta-cli
   ```

2. **Create a branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Make your changes**
   - Follow the code style guidelines
   - Add tests for new features
   - Update documentation as needed

4. **Test your changes**
   ```bash
   ./installers/build_linux.sh  # or appropriate platform script
   cd build_*
   ctest --output-on-failure
   ```

5. **Commit your changes**
   ```bash
   git add .
   git commit -m "Add: brief description of changes"
   ```

6. **Push to your fork**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **Create a Pull Request**
   - Provide a clear description
   - Reference any related issues
   - Include screenshots if applicable

## 📝 Code Style

### C++ Guidelines

- **Standard**: C++17
- **Formatting**: Follow existing code style
- **Naming Conventions**:
  - Classes: `PascalCase`
  - Functions: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_CASE`
  - Private members: trailing underscore `member_`

Example:
```cpp
class ModelManager {
public:
    bool load_model(const std::string& path);
    
private:
    std::string models_dir_;
    static constexpr int MAX_MODELS = 10;
};
```

### Comments

- Use `//` for single-line comments
- Use `/** */` for function documentation
- Explain *why*, not *what* (code should be self-documenting)

```cpp
/**
 * Load a model from the specified path
 * @param path Path to the .gguf model file
 * @return true if model loaded successfully, false otherwise
 */
bool load_model(const std::string& path);
```

### Error Handling

- Use exceptions for exceptional cases
- Return `bool` or error codes for expected failures
- Provide meaningful error messages

```cpp
// Good
if (!model_) {
    throw std::runtime_error("Model not loaded");
}

// Also good
bool load_model(const std::string& path) {
    if (!file_exists(path)) {
        return false;
    }
    // ... load model
    return true;
}
```

## 🧪 Testing

### Writing Tests

All new features should include tests using Catch2:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"

TEST_CASE("Feature name", "[category]") {
    SECTION("Specific behavior") {
        // Arrange
        ModelManager mgr;
        
        // Act
        bool result = mgr.load_model("test.gguf");
        
        // Assert
        REQUIRE(result == true);
    }
}
```

### Test Categories

Use these tags:
- `[ui]` - UI module tests
- `[auth]` - Authentication tests
- `[models]` - Model management tests
- `[inference]` - Inference engine tests
- `[tools]` - Tool integration tests
- `[integration]` - Integration tests

### Running Tests

```bash
cd build_*
ctest --output-on-failure

# Run specific test
ctest -R test_ui

# Run with verbose output
ctest -V
```

## 📚 Documentation

### Code Documentation

- Document all public APIs
- Include usage examples for complex features
- Update README.md for user-facing changes

### Commit Messages

Follow conventional commit format:

```
<type>: <description>

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

Examples:
```
feat: add multimodal support for image inputs

Implements image+text input for vision models like LLaVA.
Adds new API: generate_multimodal()

Closes #123
```

```
fix: resolve memory leak in inference engine

The sampling context was not being properly freed on model unload.

Fixes #456
```

## 🏗️ Project Structure

Understanding the codebase:

```
delta-cli/
├── src/                    # C++ source code
│   ├── main.cpp           # CLI entry point
│   ├── ui.cpp             # Terminal UI
│   ├── auth.cpp           # Authentication/telemetry
│   ├── models.cpp         # Model management
│   ├── inference.cpp      # Inference engine
│   ├── tools/             # Extended tools
│   └── web/               # Web dashboard
├── tests/                 # Test suite
├── dashboard/             # Node.js dashboard
├── installers/            # Build scripts
├── vendor/                # Third-party dependencies
│   └── llama.cpp/         # llama.cpp submodule
├── CMakeLists.txt         # Main CMake config
└── README.md              # User documentation
```

## 🔧 Development Setup

### Prerequisites

See [BUILD.md](BUILD.md) for platform-specific requirements.

### IDE Setup

**Visual Studio Code:**
1. Install C/C++ extension
2. Install CMake Tools extension
3. Open workspace: `code delta-cli.code-workspace`

**CLion:**
1. Open CMakeLists.txt as project
2. Configure CMake settings
3. Enable Catch2 integration

**Cursor:**
1. Excellent AI-powered development support
2. Use for code generation and suggestions
3. Great for understanding complex codebases

### Building for Development

```bash
# Debug build with all checks
mkdir build_debug && cd build_debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build .

# Run with debugging
gdb ./delta
# or
lldb ./delta
```

## 🌍 Platform-Specific Contributions

### Adding Platform Support

To add support for a new platform:

1. Create build script in `installers/build_<platform>.sh`
2. Update `CMakeLists.txt` with platform detection
3. Add platform-specific code with `#ifdef` guards
4. Update documentation in `BUILD.md`
5. Add platform tests

### Cross-Platform Considerations

- Use platform-agnostic APIs when possible
- Test on multiple platforms
- Avoid hardcoding paths (use `FileOps::join_path()`)
- Handle line endings (`\n` vs `\r\n`)
- Be mindful of character encodings

## 🚀 Release Process

(For maintainers)

1. Update version in `CMakeLists.txt`
2. Update CHANGELOG.md
3. Run full test suite on all platforms
4. Create git tag: `git tag -a v1.0.0 -m "Release v1.0.0"`
5. Build release binaries for all platforms
6. Create GitHub release with binaries
7. Update documentation

## 💡 Areas for Contribution

Looking for where to contribute? Here are some areas:

### High Priority
- [ ] Python wrapper for CLI scripting
- [ ] More comprehensive test coverage
- [ ] Performance benchmarks
- [ ] Model quantization utilities
- [ ] Configuration file support (YAML/JSON)

### Medium Priority
- [ ] Shell completion scripts (bash, zsh, fish)
- [ ] Docker container support
- [ ] Snap/Flatpak packages
- [ ] Homebrew formula improvements
- [ ] Windows Store package

### Documentation
- [ ] Video tutorials
- [ ] Example use cases
- [ ] API documentation (Doxygen)
- [ ] Internationalization (i18n)

### Platform Support
- [ ] FreeBSD support
- [ ] WebAssembly build
- [ ] Chrome OS support
- [ ] More ARM board support

## 📞 Getting Help

- **Questions**: Use [GitHub Discussions](https://github.com/yourusername/delta-cli/discussions)
- **Chat**: Join our Discord server (link in README)
- **Email**: your.email@example.com

## 📄 License

By contributing, you agree that your contributions will be licensed under the MIT License.

## 🙏 Recognition

Contributors will be:
- Listed in CONTRIBUTORS.md
- Mentioned in release notes
- Given credit in documentation

Thank you for contributing to Delta CLI! 🎉

