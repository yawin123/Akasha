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

**Version 2.0.0** introduces a new binary format (incompatible with v1.x files).

When you open a v1.x file with Akasha v2.0.0:
- A backup copy is created with `.bak` extension (e.g., `config.db.bak`) to preserve the original data
- A new empty v2.0.0 file is initialized, replacing the original
- Any existing v1.x data in the file **is not recovered** — you start with a fresh dataset

**Why?** The wire format changed fundamentally. Old data (stored without type tags) cannot be reliably interpreted in the new schema without information loss. Migration with backup ensures you never lose the original file — you can externally migrate any critical data before upgrading.

**Recommendation:** Before updating to v2.0.0, export any critical data from v1.x files using v1.x code, then re-import it using v2.0.0 APIs.

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

Results from 10,000 iterations per benchmark on Intel i7-13700K, Ubuntu 22.04 LTS:

| Operation | Average (ops/s) | Best (ops/s) | Worst (ops/s) | Notes |
|-----------|-----------------|--------------|---------------|-------|
| Load empty dataset | 7.2M | 14.1M | 42K | File creation overhead |
| Write scalar keys (1K) | 2.3M | 3.2M | 359K | 1000 × int64 writes |
| Read scalar keys (1K) | 4.9M | 6.5M | 570K | 1000 × int64 reads (2× faster) |
| Write string keys (1K) | 1.7M | 2.5M | 138K | 1000 × string writes (with padding) |
| Read string keys (1K) | 4.0M | 5.8M | 262K | 1000 × string reads (2× faster) |
| Write vector (1K) | 1.1M | 1.5M | 41K | 1000 × vector\<double\> writes |
| Read vector (1K) | 3.9M | 5.7M | 408K | 1000 × vector\<double\> reads |
| Write serializable (Point) | 528K | 748K | 16K | 1000 × Point (hierarchical, 3 doubles) |
| Read serializable (Point) | 855K | 1.2M | 189K | 1000 × Point deserialization |
| Write complex serializable (Scene) | 111K | 163K | 1.3K | 1000 × Scene (deeply nested) |
| Read complex serializable (Scene) | 166K | 247K | 54K | 1000 × Scene deserialization |

**Run the benchmarks:**

```bash
cmake --build build --target akasha_benchmarks && ./build/akasha_benchmarks
```

Results are exported to `benchmark_results.csv` for further analysis.

### Key Insights

1. **Read vs. Write**: Reads are consistently **2-7× faster** than writes
   - Writes require disk synchronization; reads hit mmap page cache
   - Scalar reads are fastest (4.9M ops/s), while complex deserializations are slower due to object reconstruction

2. **Type Complexity Impact**: Performance degrades gracefully with complexity
   - Scalars: ~2.3M writes, ~4.9M reads
   - Vectors: ~1.1M writes, ~3.9M reads
   - Simple serializables (Point): ~528K writes, ~855K reads
   - Complex serializables (Scene): ~111K writes, ~166K reads

3. **Serializable<T> Overhead**: The hierarchical storage model adds overhead
   - Point (3 doubles serialized as 3 hierarchical keys) is ~4× slower than scalars
   - Scene (complex nesting + vectors) is ~20× slower than scalars
   - This is intentional: it enables flexible, queryable storage at the cost of serialization time

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