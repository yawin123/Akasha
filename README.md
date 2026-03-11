# Akasha — Zero-Copy C++23 Configuration Storage

Akasha is a minimalist C++23 library for storing and retrieving configuration in memory-mapped files (mmap), prioritizing **low latency**, **direct persistence**, and **type safety**.

Designed to be embedded in projects as a Git submodule — no complex build dependencies, no global installation, no binary artifacts.

## Why Akasha?

- **Zero-copy reads**: data read directly from mmap, no intermediate buffering.
- **Direct writes**: values persisted directly to the mapped file in a single atomic operation.
- **Simple API**: load datasets, type-safe get/set, hierarchical navigation with dot notation.
- **Low overhead**: no statistics, no trackers, no background workers.
- **Type-safe templates**: `get<T>()`, `set<T>()` with compile-time validation of copyable types.
- **Unified error handling**: `Status` enum for diagnostics, no exceptions.

## Name Origin

**Akasha** comes from Sanskrit **ākāśa** (आकाश), meaning "ether," "space," or "sky." In Indian philosophy, *ākāśa* represents the subtle, all-pervading medium that enables the existence and transmission of all manifestations — a universal space containing all things.

The metaphor fits: Akasha is a unified space where configuration data from multiple sources aggregates, organizes, and resolves through a single interface. Just as *ākāśa* is the fundamental medium in philosophy, Akasha is the fundamental storage layer for your application's configuration.

## Requirements

- **Compiler**: C++23 (GCC 13+, Clang 16+, MSVC 194+)
- **Dependencies**: Boost.Interprocess (managed automatically by Conan)
- **OS**: Linux, macOS, Windows

## Installation as Submodule

```bash
git submodule add https://git.yawin.es/personal/akasha.git vendor/akasha
```

## Building

### Build Akasha Locally

After cloning (or adding as submodule), install dependencies and build:

```bash
cd vendor/akasha  # or your Akasha checkout
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

### Integrate into Your Project

In your `CMakeLists.txt`:

```cmake
add_subdirectory(vendor/akasha)
target_link_libraries(myapp akasha::akasha)
```

## Quick Start

```cpp
#include "akasha.hpp"

akasha::Store store;

// Load dataset (persistent file)
auto status = store.load("app_config", "/path/to/config.db", true);
if (status != akasha::Status::ok) {
    std::cerr << "Error: " << static_cast<int>(status) << '\n';
}

// Write typed values
store.set<int64_t>("app_config.timeout", 30);
store.set<bool>("app_config.debug", true);
store.set<std::string>("app_config.name", "MyApp");

// Read values
auto timeout = store.get<int64_t>("app_config.timeout");
if (timeout.has_value()) {
    std::cout << "Timeout: " << timeout.value() << " seconds\n";
}

// Get or set default
auto max_retries = store.getorset<int64_t>("app_config.max_retries", 5);
```

See more examples in [examples/](examples/).

## Development (Local Build)

```bash
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
./akasha_example
```

## API Reference

Full documentation in [include/akasha.hpp](include/akasha.hpp).

### Store Methods

- `load(source_id, file_path, create_if_missing)` → `Status`
- `set<T>(key_path, value)` → `Status`
- `get<T>(key_path)` → `std::optional<T>`
- `getorset<T>(key_path, default)` → `std::optional<T>`
- `has(key_path)` → `bool`
- `clear(key_path = {})` → `Status`
- `compact(dataset_id = {})` → `Status`
- `last_status()` → `Status`

### Supported Types

- Scalars: `bool`, `char`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `double`, `float`
- Strings: `std::string`
- Structs: Any `trivially_copyable` type

### Status Enum

Error codes: `ok`, `invalid_key_path`, `key_conflict`, `file_read_error`, `file_write_error`, `file_not_found`, `file_full`, `parse_error`, `dataset_not_found`, `source_already_loaded`.

## Examples

### Quickstart

```cpp
akasha::Store store;
store.load("config", "/tmp/app.db", true);
store.set("config.timeout", 30);
auto val = store.get<int64_t>("config.timeout");
```

### Hierarchical Navigation

```cpp
store.set<std::string>("db.postgres.host", "localhost");
store.set<int64_t>("db.postgres.port", 5432);

auto db = store.get<akasha::Store::DatasetView>("db");
if (db) {
    auto pg = db->get<akasha::Store::DatasetView>("postgres");
    if (pg) {
        auto host = pg->get<std::string>("host");
    }
}
```

### Error Handling

See [examples/error_handling.cpp](examples/error_handling.cpp) for complete examples of Status validation.