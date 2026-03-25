# Akasha — C++23 mmap Storage

Akasha is a minimalist C++23 library for storing and retrieving data in memory-mapped files (mmap), prioritizing **low latency**, **direct persistence**, and **type safety**.

Designed to be embedded in projects as a Git submodule — no complex build dependencies, no global installation, no binary artifacts.

## Why Akasha?

- **mmap-backed persistence**: data lives in memory-mapped files; the OS syncs pages to disk implicitly.
- **Direct writes**: values persisted directly to the mapped file in a single atomic operation.
- **Simple API**: load datasets, type-safe get/set, hierarchical navigation with dot notation.
- **Low overhead**: no statistics, no trackers, no background workers.
- **Type-safe templates**: `get<T>()`, `set<T>()` with compile-time validation of copyable types.
- **Unified error handling**: `Status` enum for diagnostics, no exceptions.

## Name Origin

**Akasha** comes from Sanskrit **ākāśa** (आकाश), meaning "ether," "space," or "sky." In Indian philosophy, *ākāśa* represents the subtle, all-pervading medium that enables the existence and transmission of all manifestations — a universal space containing all things.

The metaphor fits: Akasha is a unified space where data from multiple sources aggregates, organizes, and resolves through a single interface. Just as *ākāśa* is the fundamental medium in philosophy, Akasha is the fundamental storage layer for your application's data.

## Requirements

- **Compiler**: C++23 (GCC 13+, Clang 16+, MSVC 194+)
- **Dependencies**: Boost.Interprocess (managed automatically by Conan)

## Installation as Submodule

```bash
git submodule add https://git.yawin.es/personal/akasha.git vendor/akasha
```
Or if you prefer, from GitHub

```bash
git submodule add https://github.com/yawin123/Akasha.git vendor/akasha
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
#include <akasha.hpp>

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

// Unload dataset
if (store.unload("config") != akasha::Status::ok) {
    std::cerr << "Error: " << static_cast<int>(store.last_status());
}
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

### Documentation Files

- **[REFERENCE.md](REFERENCE.md)** — Comprehensive API guide with examples and use cases

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
- Collections: `std::vector<T>` for any copyable or memcpy-serializable type, including vectors of strings
- Structs: Any `trivially_copyable` type

### Status Enum

Error codes: `ok`, `invalid_key_path`, `key_conflict`, `file_read_error`, `file_write_error`, `file_not_found`, `file_full`, `parse_error`, `dataset_not_found`, `source_already_loaded`.

## Example Programs (Learning the API)

All examples compile with `cmake --build build` and run from `./build/akasha_*`.

### 1. Quickstart (`quickstart.cpp`)
**What it teaches:** Basic API usage (load, set, get, getorset, unload)

**Questions answered:**
- How do I load a dataset?
- How do I write and read values?
- How do I set a default if a key doesn't exist?
- How do I unload a dataset?

### 2. Error Handling (`error_handling.cpp`)
**What it teaches:** Status enum validation and error handling patterns

**Questions answered:**
- How do I detect errors?
- What error codes exist?
- How do I validate operations?

### 3. Multiple Datasets (`multiple_datasets.cpp`)
**What it teaches:** Loading multiple independent data sources in one Store

**Questions answered:**
- Can I load multiple datasets?
- Are they isolated from each other?
- How do I prevent accidental reloading?

### 4. Navigation & Introspection (`navigation.cpp`)
**What it teaches:** Checking existence, listing contents, hierarchical queries

**Questions answered:**
- Can I check if a key exists?
- Can I list what's in a branch?
- How do I navigate hierarchical data?

### 5. Cleanup & Lifecycle (`cleanup.cpp`)
**What it teaches:** Deletion and compaction operations

**Questions answered:**
- How do I delete data?
- Can I delete just a branch?
- How do I reclaim storage space?
- What happens after many deletions?

### 6. Nested Structures (`nested_data.cpp`)
**What it teaches:** Storing and retrieving complex trivially-copyable structs

**Questions answered:**
- Can I store nested structures?
- Are structs preserved exactly as stored?
- Can I store smaller structs from components?

### 7. Performance Benchmarks (`benchmarks.cpp`)
**What it teaches:** Measuring throughput of load, read, and write operations across scalars, strings, and structs.

**Questions answered:**
- How fast are reads and writes?
- How does performance scale?

### 8. DatasetView Navigation (`datasetview.cpp`)
**What it teaches:** Advanced hierarchical navigation, introspection, and subtree copying

**Questions answered:**
- How do I navigate subtrees?
- Can I check if a node has children or a direct value?
- How do I copy entire subtree structures?
- Can I use get-or-set with complex subtrees?

---

## Performance Benchmarks

Results from 60 benchmark runs on Intel i7-13700K, Ubuntu 22.04 LTS. Run with `./build/akasha_benchmarks`.

| Operation | Average | Best | Worst | Notes |
|-----------|---------|------|-------|-------|
| Load empty dataset | 14,161 ops/sec | 18,148 | 9,206 | File creation overhead |
| Write scalar (1K keys) | 932K ops/sec | 965K | 786K | 1000 int64 writes |
| Write scalar (10K keys) | 883K ops/sec | 907K | 845K | 10000 int64 writes |
| Read scalar (1K keys) | 5.3M ops/sec | 5.7M | 4.1M | mmap page-cache hit |
| Read scalar (10K keys) | 4.5M ops/sec | 4.7M | 4.0M | mmap page-cache hit |
| Write string (1K keys) | 574K ops/sec | 610K | 518K | Serialized as [length][chars] |
| Write string (10K keys) | 580K ops/sec | 599K | 456K | Serialized as [length][chars] |
| Read string (1K keys) | 4.9M ops/sec | 5.3M | 3.4M | Deserialized from mmap |
| Read string (10K keys) | 3.9M ops/sec | 4.2M | 3.0M | Deserialized from mmap |
| Write struct (1K keys) | 630K ops/sec | 666K | 463K | trivially_copyable types |
| Write struct (10K keys) | 628K ops/sec | 665K | 412K | trivially_copyable types |
| Read struct (1K keys) | 5.2M ops/sec | 5.6M | 3.8M | Consistent with scalar |
| Read struct (10K keys) | 4.2M ops/sec | 4.4M | 2.8M | Consistent with scalar |
| Compact (5K keys, 50% deleted) | 1,026 ops/sec | 1,102 | 671 | extract→delete→recreate |
| Compact (10K keys, 50% deleted) | 568 ops/sec | 609 | 479 | extract→delete→recreate |

**Key insights:**
- Reads are **5-7x faster** than writes — mmap page-cache hits avoid system call overhead
- Write performance is consistent (σ < 10%) across runs
- Read performance shows larger variance (σ ≈ 15-20%) due to system scheduling and cache state
- The `compact()` operation uses extract→delete→recreate to truly eliminate Boost.Interprocess internal fragmentation, ensuring optimal data locality for subsequent operations