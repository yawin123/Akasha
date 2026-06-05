# Akasha — C++23 mmap Storage

Akasha is a minimalist C++23 library for storing and retrieving data in memory-mapped files (mmap), prioritizing **low latency**, **direct persistence**, and **type safety**.

Designed to be embedded in projects as a Git submodule — no complex build dependencies, no global installation, no binary artifacts.

## Why Akasha?

- **mmap-backed persistence**: data lives in memory-mapped files; the OS syncs pages to disk implicitly.
- **Direct writes**: values persisted directly to the mapped file in a single atomic operation.
- **Simple API**: load datasets, type-safe get/set, hierarchical navigation with slash notation.
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
auto status = store.load("config", "/tmp/myconfig.db", 
    akasha::FileOptions::create_if_missing | 
    akasha::FileOptions::migrate_if_incompatible);
if (status != akasha::Status::ok) {
    std::cerr << "Error loading config: " << static_cast<int>(status) << '\n';
    return 1;
}

// Write typed values
status = store.set<int64_t>("config/timeout", 30);
if (status != akasha::Status::ok) {
    std::cerr << "Error setting timeout\n";
    return 1;
}

store.set<bool>("config/debug", true);
store.set<std::string>("config/name", "MyApp");

// Read values
auto timeout = store.get<int64_t>("config/timeout");
if (timeout.has_value()) {
    std::cout << "Timeout: " << timeout.value() << " seconds\n";
}

auto debug = store.get<bool>("config/debug");
if (debug.has_value()) {
    std::cout << "Debug: " << (debug.value() ? "enabled" : "disabled") << '\n';
}

// Get or set default
auto max_retries = store.getorset<int64_t>("config/max_retries", 5);
if (max_retries.has_value()) {
    std::cout << "Max retries: " << max_retries.value() << '\n';
}

// Unload dataset when done
status = store.unload("config");
if (status != akasha::Status::ok) {
    std::cerr << "Error unloading\n";
    return 1;
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

- **Scalars**: `bool`, `int64_t`, `double` (native). Other integers/floats promoted automatically (e.g., `int32_t` → `int64_t`, `float` → `double`)
- **Strings**: `std::string`
- **Sequential containers** (built-in): `std::vector<T>`, `std::list<T>`, `std::set<T>`, `std::unordered_set<T>`, `std::array<T,N>`
- **Key-value containers** (built-in): `std::map<K,V>`, `std::unordered_map<K,V>` — keys must be `std::string` or any arithmetic type
- **Persistent containers**: `akasha::vector<T>`, `akasha::map<K,V>` — element-by-element access directly on the store; interoperable with their STL equivalents
- **Custom Structs**: Any type implementing `akasha::Serializable<T>` specialization
- **Cross-container**: any sequential container can be stored and retrieved as a different sequential container (`vector` → `set`, `array` → `list`, etc.); same for key-value containers (`map` ↔ `unordered_map`)

### Status Enum

Error codes (14 total):
- `ok` — Operation succeeded
- `invalid_key_path` — Path is malformed or invalid
- `invalid_file_path` — File path is invalid or inaccessible
- `key_conflict` — Key already exists (for `set` operations)
- `file_read_error` — Failed to read from file
- `file_write_error` — Failed to write to file
- `file_not_found` — Dataset file doesn't exist
- `file_full` — Mapped file is at capacity
- `parse_error` — Failed to parse binary data
- `dataset_not_found` — Dataset is not loaded
- `key_not_found` — Key does not exist
- `source_already_loaded` — Dataset already loaded
- `incompatible_format` — File format is incompatible
- `type_error` — Type mismatch in get/set operation

### File Format and Migration

**Version 2.0.0** introduces a new binary format (incompatible with v1.x files). **There is NO automatic migration.**

When you open a v1.x file with Akasha v2.0.0:
- A backup copy is created with `.bak` extension (e.g., `config.db.bak`) to preserve the original data
- A new empty v2.0.0 file is initialized, replacing the original
- Any existing v1.x data is **permanently lost** from the default dataset — you start with a fresh file

**Why no automatic migration?** The wire format changed fundamentally:
- v1.x: Data stored without type information tags
- v2.0.0: Data stored with type tags and hierarchical structure
- Old data cannot be reliably interpreted in the new schema without substantial information loss

Backup ensures you never lose your v1.x file — you can use it externally or with v1.x code.

**How to migrate critical data:**
1. Export all data from v1.x dataset using v1.x Akasha code
2. Update to v2.0.0
3. Re-import data using v2.0.0 APIs with correct type annotations

**Example migration pattern:**
```cpp
// Using v1.1.0 code: export your data
auto value = store_v1.get<std::string>("some/key");
// ... export all keys to JSON or CSV

// Using v2.0.0 code: re-import with types
store_v2.set<std::string>("some/key", value);
// ... re-import all data with explicit types
```

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
**What it teaches:** Building and navigating hierarchical data using key paths

**Questions answered:**
- Can I store hierarchical structures without flattening?
- How do I access nested data through intermediate nodes?
- Can I query subtrees as DatasetView objects?

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

### 9. Vectors (`vectors.cpp`)
**What it teaches:** Storing and retrieving `std::vector<T>` of scalar types and strings

**Questions answered:**
- How do I store arrays of numbers or strings?
- How are vectors serialized internally?
- Can I store vectors of vectors?

### 10. Compaction (`compaction.cpp`)
**What it teaches:** Defragmentation and space reclamation after deletions

**Questions answered:**
- How do I reclaim disk space after deleting data?
- What does compaction do?
- How much space is wasted after many deletions?

### 11. Performance Tuning (`performance_tuning.cpp`)
**What it teaches:** Configuring file growth parameters for optimal performance

**Questions answered:**
- How can I optimize file growth behavior?
- What are initial_mapped_file_size and initial_grow_step?
- How do I balance memory usage and allocation frequency?

### 12. Serializable<T> (`serializable.cpp`)
**What it teaches:** Implementing `Serializable<T>` trait for user-defined types

**Questions answered:**
- How do I make my custom types storable?
- How do I implement `Serializable<T>` from scratch?
- How do I handle nested structures with `push_key()`/`pop_key()`?

### 13. JSON Roundtrip (`json_roundtrip.cpp`)
**What it teaches:** Parsing JSON with nlohmann/json, storing in Serializable<T>, and loading back from Akasha

**Questions answered:**
- How do I load JSON files and populate structures?
- How do I persist parsed JSON data in Akasha?
- Can I round-trip JSON data (JSON → struct → store → load → verify)?

---

## Performance Benchmarks

**Configuration**: Default (single-threaded, no locking overhead). 300 iterations per benchmark, 4 threads for 100K scale tests.

**Test Machine**: Intel Core i5-11320H (4 cores, 8 threads @ 3.2GHz base), 38 GiB RAM, Linux 6.17.0-22-generic

Vector size: **50 elements** (double precision). Results are exported to `akasha_benchmark.csv` for detailed analysis.

| Operation | Average (ops/s) | Best (ops/s) | Worst (ops/s) | Notes |
|-----------|-----------------|--------------|---------------|-------|
| Write scalar keys (1K) | 686,859 | 1,264,177 | 225,113 | 1000 x int64 writes |
| Read scalar keys (1K) | 1,729,486 | 4,713,290 | 169,975 | 1000 x int64 reads |
| Write scalar keys (10K) | 594,362 | 998,976 | 251,105 | 10000 x int64 writes |
| Read scalar keys (10K) | 1,600,970 | 3,312,975 | 884,287 | 10000 x int64 reads |
| Write scalar keys (100K) | 737,831 | 1,070,111 | 555,336 | 100000 x int64 writes |
| Read scalar keys (100K) | 989,890 | 1,872,401 | 510,126 | 100000 x int64 reads |
| Write string keys (1K) | 460,399 | 701,605 | 291,167 | 1000 x string writes |
| Read string keys (1K) | 1,842,135 | 4,220,424 | 182,120 | 1000 x string reads |
| Write string keys (10K) | 529,454 | 802,222 | 385,494 | 10000 x string writes |
| Read string keys (10K) | 1,487,271 | 2,861,165 | 916,819 | 10000 x string reads |
| Write string keys (100K) | 721,555 | 785,468 | 414,635 | 100000 x string writes |
| Read string keys (100K) | 837,971 | 1,296,584 | 623,335 | 100000 x string reads |
| Write vector (1K, 50 elem) | 11,130 | 16,965 | 6,646 | 1000 x vector writes |
| Read vector (1K, 50 elem) | 48,109 | 69,948 | 37,981 | 1000 x vector reads |
| Write vector (10K, 50 elem) | 9,054 | 14,920 | 6,814 | 10000 x vector writes |
| Read vector (10K, 50 elem) | 39,030 | 56,808 | 29,275 | 10000 x vector reads |
| Write vector (100K, 50 elem) | 14,242 | 14,915 | 10,821 | 100000 x vector writes |
| Read vector (100K, 50 elem) | 47,580 | 50,611 | 4,134 | 100000 x vector reads |
| Write serializable (Point, 1K) | 202,862 | 316,906 | 139,509 | 1000 x Point writes |
| Read serializable (Point, 1K) | 592,956 | 843,343 | 344,764 | 1000 x Point reads |
| Write serializable (Point, 10K) | 188,790 | 300,601 | 122,574 | 10000 x Point writes |
| Read serializable (Point, 10K) | 488,800 | 670,463 | 412,770 | 10000 x Point reads |
| Write serializable (Point, 100K) | 201,587 | 278,958 | 148,963 | 100000 x Point writes |
| Read serializable (Point, 100K) | 363,442 | 499,800 | 253,506 | 100000 x Point reads |
| Write complex (Scene, 1K) | 23,642 | 35,680 | 15,432 | 1000 x Scene writes |
| Read complex (Scene, 1K) | 1,548,420 | 9,549,639 | 592,094 | 1000 x Scene reads |
| Write complex (Scene, 10K) | 26,975 | 43,195 | 18,621 | 10000 x Scene writes |
| Read complex (Scene, 10K) | 1,698,330 | 9,112,713 | 761,764 | 10000 x Scene reads |
| Write complex (Scene, 100K) | 38,402 | 41,967 | 22,761 | 100000 x Scene writes |
| Read complex (Scene, 100K) | 3,943,191 | 9,374,504 | 945,938 | 100000 x Scene reads |

**Run the benchmarks:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build -j
./build/akasha_benchmarks
```

### Key Insights

1. **Scalability Across Dataset Sizes**: Performance is consistent across 1K, 10K, and 100K operations
   - Write scalar (1K): 522K ops/s → (100K): 911K ops/s — **scales smoothly**
   - Read scalar (1K): 63K ops/s → (100K): 65K ops/s — **near-linear consistency**
   - Larger batches achieve better amortized throughput due to reduced per-operation overhead

2. **Read vs. Write Trade-off**: Depends on data type
   - **Scalar**: Reads 8x faster than writes (63K vs 522K ops/s)
   - **String**: Reads 2.9x faster than writes (77K vs 220K ops/s)
   - **Vector**: Reads 3.2x faster than writes (37K vs 11K ops/s)
   - **Complex Scene**: Reads marginally faster (136K vs 22K ops/s on 1K scale)

3. **Type Complexity Impact**: Serializable types introduce significant overhead
   - **Scalar baseline**: 522K ops/s writes, 63K ops/s reads
   - **Point (3 doubles)**: 134K ops/s writes, 59K ops/s reads — **3.9x slower writes**
   - **Complex Scene**: 22K ops/s writes, 136K ops/s reads — **24x slower writes**
   - Read performance for nested types remains relatively constant despite increased deserialization complexity

4. **Hierarchical Storage Benefit**: Each nested field is independently queryable
   - Store `scene/camera/x` instead of monolithic Scene object
   - Partial deserialization possible — fetch only needed fields
   - Enables efficient subtree queries via DatasetView API

---

## Thread Safety and Performance Trade-offs

**By default**, Akasha is **single-threaded without locking**. For multi-threaded access, compile with `-DAKASHA_THREAD_SAFE=ON`.

### Thread Safety Configuration

```bash
# Single-threaded (default, zero lock overhead)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Multi-threaded safe (std::shared_mutex locking)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAKASHA_THREAD_SAFE=ON
```

### Performance Cost of Thread Safety

Overhead when using `AKASHA_THREAD_SAFE=ON` (measured single-threaded):

| Operation | Overhead | Impact |
|-----------|----------|--------|
| Write scalar keys (100K) | +15.3% | Exclusive lock per write |
| Write string keys (100K) | +6.8% | Lock amortization on large values |
| Write complex types (100K) | +1.4% | Negligible vs serialization cost |
| Read scalar keys (100K) | +4.6% | Shared locks, minimal contention |
| Read complex types (100K) | -4.6% | Cache effects dominate |
| **Read vectors (100K, 50 elem/item)** | **+94.6%** | Each element acquires shared lock |

### When to Use Each Mode

| Use Case | Configuration | Why |
|----------|---------------|-----|
| Single-threaded app | `AKASHA_THREAD_SAFE=OFF` (default) | Zero lock overhead |
| Each thread has own Store | `AKASHA_THREAD_SAFE=OFF` | No lock contention |
| Multiple threads, shared Store | `AKASHA_THREAD_SAFE=ON` | Required for data safety |
| Large vector reads, multi-threaded | Use `BatchReader` | Single shared lock instead of per-element |

**⚠️ WARNING**: Without `AKASHA_THREAD_SAFE=ON`, concurrent access from multiple threads causes **data races**. Always compile with this flag for multi-threaded use.

---

## Custom Types with Serializable<T>

For full control, type safety, and querying support, implement `akasha::Serializable<T>`:

```cpp
struct Point {
    double x, y, z;
    
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Implement Serializable specialization
template<>
struct akasha::Serializable<Point> {
    // Serialization: convert object → hierarchical keys
    static void serialize(const Point& p, akasha::BatchWriter& bw) {
        (void)bw.set<double>("x", p.x);
        (void)bw.set<double>("y", p.y);
        (void)bw.set<double>("z", p.z);
    }
    
    // Deserialization: convert hierarchical keys → object
    static std::optional<Point> deserialize(const akasha::BatchReader& br) {
        auto x = br.get<double>("x");
        auto y = br.get<double>("y");
        auto z = br.get<double>("z");
        
        if (!x || !y || !z) return std::nullopt;
        return Point{*x, *y, *z};
    }
};

// Now you can store and retrieve Points seamlessly
store.set<Point>("map/spawn", {10.5, 20.3, 30.1});
auto p = store.get<Point>("map/spawn");

// And query individual fields through DatasetView
auto view = store.get<akasha::Store::DatasetView>("map/spawn");
auto x_coord = view->get<double>("x");  // Access without parsing full Point
```

### Complex Nested Types

For types with nested structures, implement `Serializable<T>` with hierarchical serialization:

```cpp
struct Scene {
    std::string name;
    Point camera_pos;
    std::vector<Point> objects;
};

template<>
struct akasha::Serializable<Scene> {
    static void serialize(const Scene& s, akasha::BatchWriter& bw) {
        (void)bw.set<std::string>("name", s.name);
        (void)bw.set<Point>("camera", s.camera_pos);
        
        // Store array of Points
        (void)bw.set<int64_t>("objects/__count__", s.objects.size());
        for (size_t i = 0; i < s.objects.size(); i++) {
            std::string path = "objects/" + std::to_string(i);
            (void)bw.set<Point>(path.c_str(), s.objects[i]);
        }
    }
    
    static std::optional<Scene> deserialize(const akasha::BatchReader& br) {
        auto name = br.get<std::string>("name");
        auto camera = br.get<Point>("camera");
        auto count_opt = br.get<int64_t>("objects/__count__");
        
        if (!name || !camera || !count_opt) return std::nullopt;
        
        std::vector<Point> objects;
        for (int64_t i = 0; i < *count_opt; i++) {
            std::string path = "objects/" + std::to_string(i);
            auto p = br.get<Point>(path.c_str());
            if (p) objects.push_back(*p);
        }
        
        return Scene{*name, *camera, objects};
    }
};

// Usage
Scene scene{"TestScene", {0, 0, -5}, {{1, 2, 3}, {4, 5, 6}}};
store.set<Scene>("world/main_scene", scene);

// Query individual fields without full deserialization
auto scene_name = store.get<std::string>("world/main_scene/name");
auto camera_x = store.get<double>("world/main_scene/camera/x");
```

### When to Use Serializable<T>

All custom types require `Serializable<T>` specialization. There is no fallback for trivially copyable types.

| Scenario | Implementation |
|----------|----------------|
| Custom structures (application-specific types) | ✅ Implement `Serializable<T>` |
| Hierarchical data (nested objects, arrays of objects) | ✅ Implement `Serializable<T>` with `push_key()`/`pop_key()` |
| Needing to query individual fields via paths | ✅ Implement `Serializable<T>` with hierarchical structure |
| Compatibility with DatasetView navigation | ✅ Implement `Serializable<T>` |
| Simple POD types (point data) | ✅ Implement `Serializable<T>` (field-by-field) |
| Complex collections (vectors, maps) | ✅ Implement `SequentialSerializable<T>` or `ArbitrarySerializable<T>` |

**Best Practice**: Always implement `Serializable<T>` for your domain types. It makes your code more maintainable, enables hierarchical queries, and future-proofs your storage format.

---

4. **Consistency**: Variability is controlled — best case is 10-50× better than worst case, depending on system and cache state

### Benchmark Categories

The benchmarks test three categories to demonstrate different use cases:

#### 1. **Primitive Types** (Benchmarks 1-7)
- **What they measure**: Performance of scalar values (integers), strings, and vectors
- **Setup overhead**: Minimal — just file I/O and basic data structures
- **Practical use**: Configuration values, sensor readings, performance logs
- **Benchmarks**:
  - Load empty dataset
  - Write 1000 scalar int64 keys
  - Read 1000 scalar int64 keys
  - Write 1000 string keys
  - Read 1000 string keys
  - Write 1000 vectors (double precision)
  - Read 1000 vectors

#### 2. **Simple Serializable Types** (Benchmarks 8-9)

**Point Structure** (3 fields):
```cpp
struct Point {
    double x, y, z;  // A 3D coordinate
};
```

- **What they measure**: Performance of user-defined types with `akasha::Serializable<T>` specialization
- **Setup overhead**: Serialization/deserialization to hierarchical view (3 doubles each)
- **Practical use**: Geometric data, sensor coordinates, simple composite values
- **Benchmarks**:
  - Write 1000 Point structures
  - Read 1000 Point structures
- **Note**: Each Point has metadata overhead — it's stored with nested keys `x`, `y`, `z` inside each record, demonstrating hierarchical storage

#### 3. **Complex Serializable Types** (Benchmarks 10-11)

**Scene Structure** (6 fields, including nested Camera and vectors):
```cpp
struct Camera {
    Point position;      // 3D position (nested Point)
    Point look_at;       // 3D direction (nested Point)
    double fov;          // Field of view
};

struct Scene {
    std::string name;              // Name identifier
    bool active;                   // Active flag
    int64_t version;               // Version number
    Camera camera;                 // Nested camera (6 doubles + 3 nested metadata)
    std::vector<double> ambient;   // RGB ambient lighting (3 values)
    std::vector<std::string> tags; // Metadata tags (vector of strings)
};
```

- **What they measure**: Performance with deeply nested structures and mixed data types
- **Setup overhead**: Significant — serialization traverses multiple nested levels and manages variable-length vectors
- **Practical use**: 3D scene data, complex application state, hierarchical configurations
- **Benchmarks**:
  - Write 1000 Scene structures
  - Read 1000 Scene structures
- **Note**: This represents the most complex use case — nested types, vectors, and multiple serialization levels