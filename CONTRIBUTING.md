# Contributing to TX Terminal

Thank you for your interest in contributing to TX Terminal! This document provides guidelines and instructions for contributing.

## Code of Conduct

Be respectful and constructive in all interactions.

## How to Contribute

### Reporting Bugs

1. Check if the bug has already been reported in [Issues](https://github.com/yourusername/tx/issues)
2. If not, create a new issue with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - System information (OS, architecture, version)
   - Screenshots if applicable

### Suggesting Features

1. Open an issue with the "feature request" label
2. Describe the feature and its use case
3. Discuss implementation approach if you have ideas

### Pull Requests

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Update documentation if needed
7. Submit a pull request

## Development Setup

### Prerequisites

- CMake 3.16+
- C++20 compiler (GCC 10+ or Clang 12+)
- For Linux: GLFW3, FreeType, HarfBuzz
- For Android: Android Studio or SDK + NDK

### Building

```bash
# Clone your fork
git clone https://github.com/yourusername/tx.git
cd tx

# Linux build
./build.sh

# Android build
cd android && ./gradlew assembleDebug
```

### Running Tests

```bash
cd build
make tx_tests
./tests/tx_tests
```

## Code Style

### C++

- Use C++20 features where appropriate
- Follow the existing code style
- Use meaningful variable names
- Add comments for complex logic
- Keep functions focused and small

### Formatting

We use `clang-format` for C++ code. Run before committing:

```bash
find src include -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

### Naming Conventions

- Classes: `PascalCase` (e.g., `Terminal`, `ScreenBuffer`)
- Functions: `camelCase` (e.g., `processInput`, `renderFrame`)
- Variables: `snake_case` (e.g., `cursor_x`, `cell_buffer`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_COLS`, `DEFAULT_FG`)
- Member variables: trailing underscore (e.g., `cursor_col_`)

## Testing

- Write unit tests for new features
- Ensure all tests pass before submitting PR
- Aim for good code coverage

## Documentation

- Update README.md if adding user-facing features
- Add comments to complex code
- Update architecture docs if changing design

## Commit Messages

Use clear, descriptive commit messages:

```
feat: Add support for Sixel graphics

- Implement Sixel parser in parser.cpp
- Add image buffer to screen
- Update renderer for bitmap display
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Build process or auxiliary tool changes

## Review Process

1. Maintainers will review your PR
2. Address any feedback
3. Once approved, a maintainer will merge

## Questions?

Feel free to open an issue for questions or join discussions.

Thank you for contributing!
