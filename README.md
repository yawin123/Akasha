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
- `unload(source_id)` → `Status`
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

## Example Programs (Learning the API)

All examples compile with `cmake --build build` and run from `./build/akasha_*`.

### 1. Quickstart (`quickstart.cpp`)
**What it teaches:** Basic API usage (load, set, get, getorset, unload)

```cpp
akasha::Store store;

// Load a dataset
store.load("config", "/tmp/app.db", true);

// Write typed values
store.set<int64_t>("config.timeout", 30);
store.set<std::string>("config.name", "MyApp");

// Read values
auto timeout = store.get<int64_t>("config.timeout");
if (timeout.has_value()) {
    std::cout << "Timeout: " << timeout.value() << " seconds\n";
}

// Get or set default
auto max_retries = store.getorset<int64_t>("config.max_retries", 5);

// Unload when done
store.unload("config");
```

**Questions answered:**
- How do I load a dataset?
- How do I write and read values?
- How do I set a default if a key doesn't exist?
- How do I unload a dataset?

### 2. Error Handling (`error_handling.cpp`)
**What it teaches:** Status enum validation and error handling patterns

```cpp
auto status = store.load("data", "/tmp/data.db", false);
if (status != akasha::Status::ok) {
    std::cerr << "Error: " << (int)status << '\n';
}

// Check last error
auto last = store.last_status();
switch(last) {
    case akasha::Status::ok: break;
    case akasha::Status::file_not_found: /* ... */
    case akasha::Status::source_already_loaded: /* ... */
    // ... all 10 Status codes
}
```

**Questions answered:**
- How do I detect errors?
- What error codes exist?
- How do I validate operations?

### 3. Multiple Datasets (`multiple_datasets.cpp`)
**What it teaches:** Loading multiple independent data sources in one Store

```cpp
store.load("app", "/tmp/app.db", true);
store.load("user", "/tmp/user.db", true);
// Attempt to reload "app" (fails with source_already_loaded)

store.set<int64_t>("app.server.port", 8080);
store.set<int64_t>("user.profile.id", 42);

// Each dataset is independent
auto app_port = store.get<int64_t>("app.server.port");
auto user_id = store.get<int64_t>("user.profile.id");
```

**Questions answered:**
- Can I load multiple datasets?
- Are they isolated from each other?
- How do I prevent accidental reloading?

### 4. Navigation & Introspection (`navigation.cpp`)
**What it teaches:** Checking existence, listing contents, hierarchical queries

```cpp
store.set<std::string>("settings.server.host", "localhost");
store.set<int64_t>("settings.server.port", 8080);

// Check if a key exists
if (store.has("settings.server.host")) {
    auto host = store.get<std::string>("settings.server.host");
}

// Get a hierarchical view and list its contents
auto server_view = store.get<akasha::Store::DatasetView>("settings.server");
if (server_view) {
    for (const auto& key : server_view->keys()) {
        std::cout << "  - " << key << "\n";  // Outputs: host, port
    }
}
```

**Questions answered:**
- Can I check if a key exists?
- Can I list what's in a branch?
- How do I navigate hierarchical data?

### 5. Cleanup & Lifecycle (`cleanup.cpp`)
**What it teaches:** Deletion and compaction operations

```cpp
store.set<int64_t>("data.counter.hits", 100);
store.set<std::string>("data.cache.key1", "value1");

// Delete a specific branch
store.clear("data.cache");  // Removes all data.cache.*

// Delete entire dataset
store.clear("data");        // Removes all data.*

// Delete everything
store.clear();              // Removes all data from all datasets

// Reclaim space after deletions
store.compact("data");      // Compacts single dataset
store.compact();            // Compacts all datasets
```

**Questions answered:**
- How do I delete data?
- Can I delete just a branch?
- How do I reclaim storage space?
- What happens after many deletions?

### 6. Nested Structures (`nested_data.cpp`)
**What it teaches:** Storing and retrieving complex trivially-copyable structs

```cpp
struct Location {
    double latitude;
    double longitude;
};

struct User {
    int64_t id;
    int32_t age;
    char name[64];
    Location home;
};

struct UserProfile {
    User user;
    int64_t signup_timestamp;
    int32_t login_count;
};

UserProfile profile{{1001, 30, "Alice", {41.3851, 2.1734}}, 1577836800, 42};
store.set<UserProfile>("profiles.alice.data", profile);

auto retrieved = store.get<UserProfile>("profiles.alice.data");
```

**Questions answered:**
- Can I store nested structures?
- Are structs preserved exactly as stored?
- Can I store smaller structs from components?

### 7. Performance Benchmarks (`benchmarks.cpp`)
**What it teaches:** Measuring throughput and validating zero-copy architecture

Runs comprehensive tests for load, read, write operations across scalars, strings, and structs.

**Questions answered:**
- How fast are reads and writes?
- Is zero-copy really faster?
- How does performance scale?

---

## Performance Benchmarks

Results measured on Intel i7-13700K, Ubuntu 22.04 LTS. Run with `./build/akasha_benchmarks`.

| Operation | Throughput | Notes |
|-----------|-----------|-------|
| Load empty dataset | 13,717 ops/sec | File creation overhead |
| Write scalar (int64) | 848K / 803K ops/sec | 1K and 10K keys |
| **Read scalar (int64)** | **3.8M / 3.1M ops/sec** | Zero-copy performance |
| Write string | 539K / 546K ops/sec | Serialized as [length][chars] |
| **Read string** | **3.6M / 2.9M ops/sec** | mmap zero-copy reads |
| Write struct | 601K / 607K ops/sec | trivially_copyable types |
| **Read struct** | **3.8M / 3.0M ops/sec** | Consistent with scalar |
| Compact (50% deleted) | 7.3K / 4.3K ops/sec | 5K and 10K keys |

**Key insight:** Reads are **5-7x faster** than writes — validates zero-copy mmap architecture.