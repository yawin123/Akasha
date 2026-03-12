# Contributing to Akasha

Thank you for your interest in contributing to Akasha! This document provides guidelines to make the process smooth and consistent for everyone.

---

## Reporting Bugs

If you find a bug, please [open an issue](https://github.com/yawin123/Akasha/issues/new) with the title prefixed by `[Bug]` (e.g. `[Bug] Crash when loading empty file`) and include the following information:

- **Description**: A clear and concise description of the bug.
- **Reproduction steps**: Minimal code or steps to reproduce the behavior.
- **Expected behavior**: What you expected to happen.
- **Actual behavior**: What actually happened.
- **Environment**: OS, compiler version (GCC/Clang/MSVC), and C++ standard in use.

Before opening a new issue, please search existing issues to avoid duplicates.

---

## Proposing Features

Have an idea for Akasha? Open an issue with the title prefixed by `[Feature]` (e.g. `[Feature] Add key expiration support`) and include:

- **Motivation**: What problem does this feature solve? Why is it relevant to Akasha's goals?
- **Proposed API or behavior**: A sketch of how the feature would look or work.
- **Alternatives considered**: Any other approaches you thought about.

Keep in mind that Akasha is intentionally minimalist. Proposals that add complexity without a clear performance or usability benefit are unlikely to be accepted.

---

## Code Style

Akasha follows a consistent style throughout the codebase. Please adhere to the following when writing code:

- **Standard**: C++23. Use modern features where they improve clarity or performance.
- **Naming**: `snake_case` for variables, functions, and files. `PascalCase` for types and classes.
- **Formatting**: 4-space indentation, no tabs. Keep lines under 100 characters where possible.
- **Headers**: Use `#pragma once`. Group includes in this order: standard library, third-party, project headers — each group separated by a blank line.
- **Error handling**: Use the `Status` enum. No exceptions. Return errors explicitly.
- **Comments**: Write comments to explain *why*, not *what*. The code should be readable enough to explain what it does.

When in doubt, follow the style of the surrounding code.

---

## Pull Request Process

1. **Fork** the repository and create your branch from `dev`.
2. **Name your branch** descriptively: `bugfix/key-not-found-crash`, `feature/list-keys`, etc.
3. **Write or update tests** if your change affects behavior.
4. **Update documentation** (README, REFERENCE.md, or inline comments) if your change affects the public API.
5. **Ensure the project builds cleanly**:
   ```bash
   conan install . --output-folder=build --build=missing
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
   cmake --build .
   ```
6. **Open a Pull Request** against `dev` with a clear description of what the change does and why.

PRs that introduce breaking API changes must include a clear justification and, if accepted, will be noted in the changelog.

---

## Questions

For general questions about usage, please open a [discussion](https://github.com/yawin123/Akasha/discussions) rather than an issue.
