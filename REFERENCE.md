# Akasha 2.0.0 — Complete API Reference

Comprehensive guide to the Akasha library: a hierarchical key-value store with persistence in memory-mapped files (mmap).

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Fundamental Concepts](#fundamental-concepts)
3. [Inclusion and Version](#inclusion-and-version)
4. [Store — Lifecycle](#store--lifecycle)
5. [Store — Data Operations](#store--data-operations)
6. [Supported Types and Conversions](#supported-types-and-conversions)
7. [DatasetView — Hierarchical Navigation](#datasetview--hierarchical-navigation)
8. [Custom Types — Serializable\<T\>](#custom-types--serializablet)
9. [Sequential Containers — SequentialSerializable\<T\>](#sequential-containers--sequentialserializablet)
10. [Arbitrary Containers — ArbitrarySerializable\<T\>](#arbitrary-containers--arbitraryserializablet)
11. [std::vector\<T\> — Built-in Support](#stdvectort--built-in-support)
12. [BatchWriter and BatchReader](#batchwriter-and-batchreader)
13. [Status — Error Codes](#status--error-codes)
14. [FileOptions — Load Options](#fileoptions--load-options)
15. [PerformanceTuning — Performance Tuning](#performancetuning--performance-tuning)
16. [Internal Binary Format](#internal-binary-format)
17. [Thread Safety and Concurrency](#thread-safety-and-concurrency)
18. [Edge Cases and Robustness](#edge-cases-and-robustness)
19. [Compilation and Integration](#compilation-and-integration)

---

## Quick Start

```cpp
#include "akasha.hpp"
#include <iostream>

int main() {
    akasha::Store store;

    // 1. Load a dataset (creates file if missing)
    auto status = store.load("config", "/tmp/config.db",
                             akasha::FileOptions::create_if_missing);
    if (status != akasha::Status::ok) return 1;

    // 2. Write typed values
    store.set<int64_t>("config/timeout", 30);
    store.set<bool>("config/debug", true);
    store.set<std::string>("config/name", "MyApp");

    // 3. Read values
    auto timeout = store.get<int64_t>("config/timeout");
    if (timeout) std::cout << "Timeout: " << *timeout << "\n";

    // 4. Lazy initialization (getorset)
    auto retries = store.getorset<int64_t>("config/max_retries", 5);

    // 5. Navigate subnodes
    auto view = store.get<akasha::Store::DatasetView>("config");
    if (view) {
        for (const auto& key : view->keys())
            std::cout << "  " << key << "\n";
    }

    // 6. Unload
    store.unload("config");
}
```

**⚠️ Thread Safety Note**: This code is single-threaded safe. If you need access the `Store` from multiple threads, compile with `-DAKASHA_THREAD_SAFE=ON`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAKASHA_THREAD_SAFE=ON
```

Without this flag, concurrent access will cause **data races**. See [Thread Safety and Concurrency](#thread-safety-and-concurrency) for details.

---

## Fundamental Concepts

| Concept | Description |
|---|---|
| **Store** | Main object. Manages multiple datasets associated with memory-mapped files. |
| **Dataset** | Independent data set identified by a `source_id`. Each dataset occupies a separate `.db` file. |
| **KeyPath** | Hierarchical path with `/` separator. The first segment is always the dataset's `source_id`. Ex: `"config/server/port"`. |
| **DatasetView** | Read-only view over an intermediate tree node. Allows relative navigation without reconstructing full paths. |
| **BatchWriter / BatchReader** | Transactional interfaces for bulk write/read. Used internally to serialize custom types. |

### Path Format (KeyPath)

Paths use `/` as separator. The first segment identifies the dataset:

```
"config/server/port"
 ^^^^^^ ^^^^^^^^^^^
 dataset    subkey
```

- **`"config"`**: root of dataset `config`.
- **`"config/server"`**: intermediate node `server` within `config`.
- **`"config/server/port"`**: leaf `port` within `server`.

---

## Inclusion and Version

```cpp
#include "akasha.hpp"  // Includes entire library
```

This umbrella header includes (in dependency order):

1. `akasha/core.hpp` — Fundamental types (`Status`, `FileOptions`, `PerformanceTuning`, traits)
2. `akasha/store.hpp` — `Store` class and `DatasetView`
3. `akasha/batch.hpp` — `BatchWriter` and `BatchReader`
4. `akasha/detail/store_serializable.hpp` — Support for custom types
5. `akasha/detail/stl_serialization.hpp` — Support for STL containers (`std::vector`, `std::list`, `std::set`, `std::unordered_set`, `std::array`, `std::map`, `std::unordered_map`)

### Version

```cpp
std::string_view v = akasha::version();  // "2.0.0"
```

Returns the semantic version defined by the `AKASHA_VERSION` macro at compilation.

### Backward Compatibility

**v2.0.0 is NOT backward compatible with v1.x files.**

The binary format changed fundamentally between versions. When you attempt to load a v1.x file with Akasha v2.0.0:

1. A backup copy is created: `filename.db.bak`
2. A new empty v2.0.0 file is initialized: `filename.db`
3. **No automatic data migration** — v1.x data is not transferred

**Reason**: v1.x data lacks type information tags. Attempting automatic conversion would result in data loss or corruption.

**How to migrate critical data:**

```cpp
// Step 1: Using Akasha v1.1.0, export data to external format
akasha::Store store_v1;
store_v1.load("db", "/path/to/old.db");
// ... export to JSON/CSV using v1.x APIs

// Step 2: Update to Akasha v2.0.0
// Step 3: Re-import with explicit types
akasha::Store store_v2;
store_v2.load("db", "/path/to/new.db", akasha::FileOptions::create_if_missing);
// ... import from JSON/CSV using v2.0.0 APIs with type annotations
```

**The backup (.bak) file preserves your original data** — you can use it for manual recovery or keep it for reference.

---

## Store — Lifecycle

### `load(source_id, file_path, options)`

Loads a dataset from a memory-mapped file.

```cpp
auto status = store.load("db", "/path/to/data.db",
                         akasha::FileOptions::create_if_missing |
                         akasha::FileOptions::migrate_if_incompatible);
```

| Parameter | Type | Description |
|---|---|---|
| `source_id` | `string_view` | Unique dataset identifier (e.g., `"config"`, `"cache"`). |
| `file_path` | `string_view` | Path to the persistence file. |
| `options` | `FileOptions` | Behavior flags (see [FileOptions](#fileoptions--load-options)). Default: `FileOptions::none`. |

**Possible errors:**

| Status | Cause |
|---|---|
| `source_already_loaded` | A dataset with that `source_id` already exists. |
| `file_not_found` | File does not exist and `create_if_missing` was not used. |
| `invalid_file_path` | The file path is not valid. |
| `file_read_error` | Error opening/reading the file. |
| `parse_error` | Internal data is corrupted. |
| `incompatible_format` | Incompatible file format and `migrate_if_incompatible` was not used. |

### `unload(source_id)`

Unloads a dataset and closes its memory-mapped file.

```cpp
auto status = store.unload("db");
```

Data persists on disk but is no longer accessible from this `Store` instance.

**Possible errors:** `dataset_not_found`.

---

## Store — Data Operations

### `set<T>(key_path, value)` — Write

Sets a typed value at a key. The write is persisted immediately to the memory-mapped file.

```cpp
store.set<int64_t>("config/retries", 3);
store.set<double>("config/threshold", 0.95);
store.set<bool>("config/verbose", true);
store.set<std::string>("config/version", "2.0.0");
```

All overloads return `[[nodiscard]] Status`. You must always check the result:

```cpp
if (store.set<int64_t>("config/timeout", 30) != akasha::Status::ok) {
    // handle error
}
```

**Possible errors:**

| Status | Cause |
|---|---|
| `invalid_key_path` | Path does not have `dataset/key` format. |
| `dataset_not_found` | The dataset is not loaded. |
| `key_conflict` | Key already exists as an intermediate node. |
| `file_write_error` | Error writing to file. |
| `file_full` | No space; automatic growth failed. |
| `type_error` | Numeric overflow (e.g., `uint64_t` > `INT64_MAX`). |

### `get<T>(key_path)` — Read

Reads a typed value. Returns `std::optional<T>`: contains the value if it exists, `std::nullopt` otherwise.

```cpp
auto timeout = store.get<int64_t>("config/timeout");
if (timeout.has_value()) {
    std::cout << "Timeout: " << *timeout << "\n";
}
```

Without template parameter or with `DatasetView`, returns a navigable view:

```cpp
auto view = store.get("config/server");
// equivalent to: store.get<akasha::Store::DatasetView>("config/server")
```

### `getorset<T>(key_path, default_value)` — Lazy Initialization

If the key exists, returns its value. If not, writes `default_value`, persists it, and returns it.

```cpp
auto timeout = store.getorset<int64_t>("config/timeout", 30);
// If "config/timeout" didn't exist, it now contains 30
```

Works with all supported types, including `DatasetView`, vectors, and custom types.

### `set_null(key_path)` — Null Value

Sets a null marker at the key. After `set_null`:

- `has(key)` returns `true`
- `get<T>(key)` returns `std::nullopt` for any `T`

```cpp
store.set_null("config/deprecated_field");
```

### `has(key_path)` — Existence

Checks if a path exists (whether it's a leaf or intermediate node).

```cpp
if (store.has("config/timeout")) {
    // key exists
}
```

### `clear(key_path)` — Deletion

Deletes persisted data.

```cpp
store.clear();                    // All in all datasets
store.clear("config");            // All of dataset "config"
store.clear("config/database");   // Subkey and entire subtree
```

### `compact(dataset_id)` — Compaction

Compacts the memory-mapped file to reclaim space freed by `clear()`.

```cpp
store.compact("config");   // Compact one dataset
store.compact();           // Compact all
```

### `last_status()` — Last Status

Returns the `Status` of the last operation. Useful when `get<T>()` returns `nullopt` and you need to know why.

```cpp
auto val = store.get<int64_t>("config/missing");
if (!val) {
    auto status = store.last_status();
    // status == Status::key_not_found
}
```

---

## Supported Types and Conversions

### Native Scalar Types

| C++ Type | Internal TypeTag | Size | Notes |
|---|---|---|---|
| `bool` | `bool_type` (0x01) | 1 byte | |
| `int64_t` | `int64_type` (0x02) | 8 bytes | Little-endian |
| `double` | `double_type` (0x03) | 8 bytes | IEEE 754 |
| `std::string` | `string_type` (0x04) | 8 + N bytes | Length prefix `size_t` + data |

### Automatic Conversions

| Original Type | Converts to | Validation |
|---|---|---|
| `int`, `int32_t`, `short`, etc. | `int64_t` | Always safe (wider range) |
| `uint64_t`, `size_t` | `int64_t` | Fails with `type_error` if > `INT64_MAX` |
| `float` | `double` | Always safe |

When reading with `get<T>()`, inverse range validation is applied:

```cpp
store.set<int64_t>("key", 300);
auto val = store.get<int8_t>("key");  // std::nullopt (300 > 127)
```

### Compound Types

| Type | Mechanism | Metadata |
|---|---|---|
| Custom struct | `Serializable<T>` (user) | — |
| Indexed container | `SequentialSerializable<T>` (built-in for STL) | `__count__` |
| Key-value container | `ArbitrarySerializable<T>` (built-in for STL) | `__children__` |
| `DatasetView` | Subtree view | — |

---

## DatasetView — Hierarchical Navigation

`DatasetView` is a window over a tree node and its descendants. Obtained with `store.get<DatasetView>(key_path)` or simply `store.get(key_path)`.

### Getting a View

```cpp
// Set up hierarchical data
store.set<int64_t>("config/server/port", 8080);
store.set<std::string>("config/server/host", "localhost");
store.set<bool>("config/server/ssl", true);

// Get view of subtree "server"
auto server = store.get<akasha::Store::DatasetView>("config/server");
```

### `keys()` — Direct Children

Returns `std::vector<std::string>` with direct child keys (non-recursive).

```cpp
if (server) {
    auto keys = server->keys();
    // keys: ["host", "port", "ssl"]
}
```

### `get<T>(relative_path)` — Relative Read

Navigates from the current position using relative paths.

```cpp
if (server) {
    auto port = server->get<int64_t>("port");           // 8080
    auto host = server->get<std::string>("host");        // "localhost"

    // Nested views
    auto nested = server->get<akasha::Store::DatasetView>("ssl_config");
}
```

### `set<T>(relative_path, value)` — Relative Write

```cpp
if (server) {
    server->set<int64_t>("port", 9000);  // Modifies config/server/port
}
```

### `has(relative_path)` — Relative Existence

```cpp
if (server && server->has("port")) {
    // "port" exists under this node
}
```

### `has_value()` — Is Leaf?

```cpp
if (server->has_value()) {
    // This node has a direct scalar value
}
```

### `has_keys()` — Has Children?

```cpp
if (server->has_keys()) {
    // This node has subkeys
}
```

### Copy Subtrees with DatasetView

`set<DatasetView>` copies an entire subtree to another location:

```cpp
auto original = store.get<akasha::Store::DatasetView>("config/server");
if (original) {
    store.set<akasha::Store::DatasetView>("config/server_backup", *original);
}
```

Also works across different datasets:

```cpp
store.load("backup", "/tmp/backup.db", akasha::FileOptions::create_if_missing);
auto src = store.get<akasha::Store::DatasetView>("config");
if (src) {
    store.set<akasha::Store::DatasetView>("backup", *src);
}
```

---

## Custom Types — Serializable\<T\>

To store structs with fixed fields, specialize `akasha::Serializable<T>`. Without this specialization, `store.set<T>()` will not compile.

### Define the Specialization

```cpp
struct Point {
    double x, y, z;
};

template<>
struct akasha::Serializable<Point> {
    static void serialize(const Point& p, akasha::BatchWriter& bw) {
        (void)bw.set<double>("x", p.x);
        (void)bw.set<double>("y", p.y);
        (void)bw.set<double>("z", p.z);
    }

    static std::optional<Point> deserialize(const akasha::BatchReader& br) {
        auto x = br.get<double>("x");
        auto y = br.get<double>("y");
        auto z = br.get<double>("z");
        if (!x || !y || !z) return std::nullopt;
        return Point{*x, *y, *z};
    }
};
```

### Use with Store

```cpp
Point origin{1.5, 2.7, 3.14};
store.set<Point>("db/origin", origin);

auto restored = store.get<Point>("db/origin");
if (restored) {
    std::cout << restored->x << ", " << restored->y << ", " << restored->z;
}

// Individual fields are still accessible as scalars
auto x = store.get<double>("db/origin/x");  // 1.5
```

### Nested Structs

For types containing other serializable types, use `bw.set<T>()` and `br.get<T>()` which handle hierarchy internally:

```cpp
struct Color {
    int64_t r, g, b;
    std::string name;
};

struct Theme {
    Point origin;
    Color accent;
    double scale;
};

template<>
struct akasha::Serializable<Color> {
    static void serialize(const Color& c, akasha::BatchWriter& bw) {
        (void)bw.set<int64_t>("r", c.r);
        (void)bw.set<int64_t>("g", c.g);
        (void)bw.set<int64_t>("b", c.b);
        (void)bw.set<std::string>("name", c.name);
    }
    static std::optional<Color> deserialize(const akasha::BatchReader& br) {
        auto r = br.get<int64_t>("r");
        auto g = br.get<int64_t>("g");
        auto b = br.get<int64_t>("b");
        auto name = br.get<std::string>("name");
        if (!r || !g || !b || !name) return std::nullopt;
        return Color{*r, *g, *b, *name};
    }
};

template<>
struct akasha::Serializable<Theme> {
    static void serialize(const Theme& t, akasha::BatchWriter& bw) {
        (void)bw.set<Point>("origin", t.origin);
        (void)bw.set<Color>("accent", t.accent);
        (void)bw.set<double>("scale", t.scale);
    }
    static std::optional<Theme> deserialize(const akasha::BatchReader& br) {
        auto origin = br.get<Point>("origin");
        auto accent = br.get<Color>("accent");
        auto scale = br.get<double>("scale");
        if (!origin || !accent || !scale) return std::nullopt;
        return Theme{*origin, *accent, *scale};
    }
};
```

---

## Sequential Containers — SequentialSerializable\<T\>

For types representing indexed collections (vectors, deques, etc.), specialize `SequentialSerializable<T>`. The infrastructure automatically writes a `__count__` metadata with the element count.

### Required Interface

```cpp
template<>
struct akasha::SequentialSerializable<MyContainer> {
    // Serializes each element using bw.set<ElementType>(index_str, element)
    static void serialize(const MyContainer& c, akasha::BatchWriter& bw);

    // Reconstructs container reading __count__ and each indexed element
    static std::optional<MyContainer> deserialize(const akasha::BatchReader& br);

    // Returns element count
    static int64_t size(const MyContainer& c);
};
```

### Example: Custom Deque

```cpp
template<typename T>
struct akasha::SequentialSerializable<std::deque<T>> {
    static void serialize(const std::deque<T>& d, akasha::BatchWriter& bw) {
        for (size_t i = 0; i < d.size(); ++i)
            (void)bw.set(std::to_string(i), d[i]);
    }
    static std::optional<std::deque<T>> deserialize(const akasha::BatchReader& br) {
        auto count = br.get_count();  // reads __count__
        if (!count || *count < 0) return std::nullopt;
        std::deque<T> result;
        for (size_t i = 0; i < static_cast<size_t>(*count); ++i) {
            auto elem = br.get<T>(std::to_string(i));
            if (!elem) return std::nullopt;
            result.push_back(std::move(*elem));
        }
        return result;
    }
    static int64_t size(const std::deque<T>& d) {
        return static_cast<int64_t>(d.size());
    }
};
```

---

## Arbitrary Containers — ArbitrarySerializable\<T\>

For types with arbitrary keys (maps, named sets, etc.), specialize `ArbitrarySerializable<T>`. The infrastructure automatically writes `__children__` with the key list separated by `\n`.

### Required Interface

```cpp
template<>
struct akasha::ArbitrarySerializable<MyMap> {
    // Serializes each entry using bw.set<ValueType>(key_str, value)
    static void serialize(const MyMap& m, akasha::BatchWriter& bw);

    // Reconstructs reading __children__ and each entry by key
    static std::optional<MyMap> deserialize(const akasha::BatchReader& br);

    // Returns direct keys
    static std::vector<std::string> keys(const MyMap& m);
};
```

Note: `BatchReader::get_children()` parses the `__children__` string and returns `std::vector<std::string>` with the keys.

---

## std::vector\<T\> — Built-in Support

The following STL containers have built-in `SequentialSerializable` or `ArbitrarySerializable` specializations that work without additional user code.

### Sequential Containers (indexed by position)

| Container | Notes |
|---|---|
| `std::vector<T>` | Dynamic array, supports `reserve` |
| `std::list<T>` | Doubly-linked list |
| `std::set<T>` | Ordered unique set |
| `std::unordered_set<T>` | Hash-based unique set |
| `std::array<T,N>` | Fixed-size; `N` must match on deserialization |

```cpp
// Sequential containers
std::vector<int64_t> v = {1, 2, 3};
store.set<std::vector<int64_t>>("db/v", v);

std::list<double> l = {1.1, 2.2, 3.3};
store.set<std::list<double>>("db/l", l);

std::set<int64_t> s = {10, 20, 30};
store.set<std::set<int64_t>>("db/s", s);

std::unordered_set<int64_t> us = {10, 20, 30};
store.set<std::unordered_set<int64_t>>("db/us", us);

std::array<double, 3> a = {1.0, 2.0, 3.0};
store.set<std::array<double, 3>>("db/a", a);
```

### Cross-Container Reads

Any sequential container can be read back as a different sequential container. The data is stored by position, so the conversion is transparent:

```cpp
std::vector<int64_t> original = {10, 20, 30};
store.set<std::vector<int64_t>>("db/seq", original);

// Read as different container types
auto as_list = store.get<std::list<int64_t>>("db/seq");           // ok
auto as_set  = store.get<std::set<int64_t>>("db/seq");            // ok
auto as_arr  = store.get<std::array<int64_t, 3>>("db/seq");       // ok (N must match count)
```

### Key-Value Containers (indexed by key)

| Container | Key types supported |
|---|---|
| `std::map<K,V>` | `std::string`, any arithmetic type |
| `std::unordered_map<K,V>` | `std::string`, any arithmetic type |

```cpp
std::map<std::string, int64_t> m = {{"timeout", 30}, {"retries", 5}};
store.set<std::map<std::string, int64_t>>("db/cfg", m);

auto restored = store.get<std::map<std::string, int64_t>>("db/cfg");
// restored->at("timeout") == 30

// Can be read back as unordered_map and vice versa
auto as_unordered = store.get<std::unordered_map<std::string, int64_t>>("db/cfg");
```

### Internal Format — Sequential

Each element is stored as an indexed subkey:

```
sensors/readings/0  → 22
sensors/readings/1  → 23
sensors/readings/2  → 21
sensors/readings/3  → 24
sensors/readings/__count__  → 4
```

This allows accessing individual elements as scalars:

```cpp
auto second = store.get<int>("sensors/readings/1");  // 23
```

---

## BatchWriter and BatchReader

`BatchWriter` and `BatchReader` provide transactional interfaces for bulk write/read operations. Used primarily internally by serialization specializations, but are part of the public API.

### BatchWriter

Constructed with a reference to `Store` and a key prefix:

```cpp
akasha::BatchWriter bw(store, "db/complex_data");
// Acquires exclusive lock automatically
```

**Main Methods:**

| Method | Description |
|---|---|
| `set<T>(key, value)` | Writes a value under current prefix |
| `set_null(key)` | Writes null marker |
| `set_raw(key, bytes, size, tag)` | Raw bytes write with TypeTag |
| `clear_children()` | Deletes all subkeys under prefix |
| `commit()` | Confirms operations and releases lock |
| `has(key)` | Checks relative existence |
| `has_value()` | Does current node have direct value? |
| `has_keys()` | Does current node have children? |
| `keys()` | Direct child keys |

Exclusive lock acquired in constructor, released with `commit()` or destructor.

### BatchReader

```cpp
const akasha::BatchReader br(store, "db/complex_data");
// Acquires shared lock automatically
```

**Main Methods:**

| Method | Description |
|---|---|
| `get<T>(key)` | Reads value under current prefix |
| `get_raw(key)` | Raw bytes read (`string_view`) |
| `get_count()` | Reads `__count__` metadata (for sequential) |
| `get_children()` | Reads and parses `__children__` (for arbitrary) |
| `has(key)` | Checks relative existence |
| `has_value()` | Does current node have direct value? |
| `has_keys()` | Does current node have children? |
| `keys()` | Direct child keys |

Shared lock acquired in constructor, released in destructor.

---

## Status — Error Codes

```cpp
enum class Status {
    ok,                     // Operation successful
    invalid_key_path,       // Path without dataset/key format
    invalid_file_path,      // Invalid file path
    key_conflict,           // Hierarchical conflict (intermediate vs leaf)
    file_read_error,        // Error reading memory-mapped file
    file_write_error,       // Error writing memory-mapped file
    file_not_found,         // File doesn't exist and create_if_missing not used
    file_full,              // No space after growth attempts
    parse_error,            // Internal data corrupted
    dataset_not_found,      // Dataset (first segment) not loaded
    key_not_found,          // Key doesn't exist
    source_already_loaded,  // Dataset with that source_id already exists
    incompatible_format,    // Incompatible format without migrate_if_incompatible
    type_error,             // Numeric overflow or type mismatch
};
```

### Diagnostic Helper Function

```cpp
void print_status(akasha::Status s) {
    switch (s) {
        case akasha::Status::ok:                    std::cout << "OK\n"; break;
        case akasha::Status::invalid_key_path:      std::cout << "Invalid path\n"; break;
        case akasha::Status::invalid_file_path:     std::cout << "Invalid file\n"; break;
        case akasha::Status::key_conflict:          std::cout << "Key conflict\n"; break;
        case akasha::Status::file_read_error:       std::cout << "Read error\n"; break;
        case akasha::Status::file_write_error:      std::cout << "Write error\n"; break;
        case akasha::Status::file_not_found:        std::cout << "File not found\n"; break;
        case akasha::Status::file_full:             std::cout << "File full\n"; break;
        case akasha::Status::parse_error:           std::cout << "Parse error\n"; break;
        case akasha::Status::dataset_not_found:     std::cout << "Dataset not found\n"; break;
        case akasha::Status::key_not_found:         std::cout << "Key not found\n"; break;
        case akasha::Status::source_already_loaded: std::cout << "Already loaded\n"; break;
        case akasha::Status::incompatible_format:   std::cout << "Incompatible format\n"; break;
        case akasha::Status::type_error:            std::cout << "Type error\n"; break;
    }
}
```

---

## FileOptions — Load Options

`FileOptions` is a bitmask controlling `load()` behavior.

```cpp
enum class FileOptions {
    none                     = 0,  // No special options
    create_if_missing        = 1,  // Create empty file if doesn't exist
    migrate_if_incompatible  = 2,  // Migrate format if incompatible
};
```

Combined with `|` operator:

```cpp
auto status = store.load("db", "/tmp/data.db",
    akasha::FileOptions::create_if_missing | akasha::FileOptions::migrate_if_incompatible);
```

| Flag | Effect |
|---|---|
| `none` | File must exist and be compatible. |
| `create_if_missing` | Creates empty file if doesn't exist. |
| `migrate_if_incompatible` | Attempts migration if format is incompatible. |

---

## PerformanceTuning — Performance Tuning

Controls automatic growth of memory-mapped files.

```cpp
struct PerformanceTuning {
    size_t initial_mapped_file_size = 64 * 1024;   // 64 KB initial
    size_t initial_grow_step        = 32 * 1024;    // 32 KB per growth
    int    max_grow_retries         = 8;            // Max attempts
};
```

### Configure

```cpp
akasha::PerformanceTuning tuning;
tuning.initial_mapped_file_size = 1024 * 1024;  // 1 MB initial
tuning.initial_grow_step = 512 * 1024;           // 512 KB per growth
tuning.max_grow_retries = 16;

store.set_performance_tuning(tuning);
```

### Query

```cpp
auto tuning = store.performance_tuning();
std::cout << "Initial size: " << tuning.initial_mapped_file_size << " bytes\n";
```

> **Note:** Parameters apply to new file creation and future growth. They don't change already-mapped files.

---

## Internal Binary Format

Each stored value has a type byte (`TypeTag`) followed by payload:

| TypeTag | Value | Payload |
|---|---|---|
| `null_type` | `0x00` | No payload |
| `bool_type` | `0x01` | 1 byte |
| `int64_type` | `0x02` | 8 bytes (little-endian) |
| `double_type` | `0x03` | 8 bytes (IEEE 754) |
| `string_type` | `0x04` | `size_t` (8 bytes) + N bytes of data |

Compound types have no TypeTag: they decompose into scalar subkeys.

---

## Thread Safety and Concurrency

**Default behavior**: Single-threaded, no locks (zero overhead via `NoOpSharedMutex`).

To use from multiple threads, compile with `-DAKASHA_THREAD_SAFE=ON` to enable `std::shared_mutex`.

### Lock Semantics

When `AKASHA_THREAD_SAFE=ON`:
- Each memory-mapped file has its own `std::shared_mutex`
- **Reads** (`get<T>`, `has`, `keys`): acquire shared lock → multiple simultaneous readers
- **Writes** (`set<T>`, `clear`, `compact`): acquire exclusive lock → serialized, blocks readers
- `BatchWriter` holds exclusive lock from construction until `commit()` or destruction
- `BatchReader` holds shared lock throughout its lifetime
- Safe to operate on different datasets from multiple threads (independent per-file locks)

### Performance Impact

| Scenario | Default | THREAD_SAFE=ON | Overhead |
|----------|---------|----------------|----------|
| Write scalar (single) | ~1.27 µs | ~1.47 µs | +15.8% |
| Read scalar (single) | ~524 ns | ~548 ns | +4.6% |
| Write vector (100 elem) | ~73.5 µs | ~75.3 µs | +2.4% |
| Read vector (100 elem) | ~20.5 µs | ~39.8 µs | +94.1% ⚠️ |

**Note**: Large vector reads are expensive with per-element locking. Use `BatchReader` for bulk operations on multi-threaded workloads.

### Best Practices

- **Single-threaded**: Leave `AKASHA_THREAD_SAFE=OFF` (default) for maximum performance
- **Thread-per-Store**: Each thread creates its own `Store` and works independently → `AKASHA_THREAD_SAFE=OFF` safe
- **Shared Store**: Multiple threads access same `Store` → **must use** `AKASHA_THREAD_SAFE=ON`
- **Bulk reads, multi-threaded**: Use `BatchReader` to acquire single shared lock instead of per-element locks

### Data Safety Without THREAD_SAFE

Without `AKASHA_THREAD_SAFE=ON`, concurrent access is **undefined behavior**:

```cpp
// UNSAFE - data races
akasha::Store store;
store.load("data", "file.db");

// Thread 1        | Thread 2
store.set("a", 1); | store.set("b", 2);  // RACE CONDITION
```

Solution: Either use separate `Store` per thread, or compile with `-DAKASHA_THREAD_SAFE=ON`.

---

## Edge Cases and Robustness

### String Reallocation

Overwriting strings of different sizes works correctly:

```cpp
store.set<std::string>("data/text", "small");
store.set<std::string>("data/text", "much much larger string with more content");
store.set<std::string>("data/text", "x");

auto text = store.get<std::string>("data/text");
// text.value() == "x" — no residual bytes
```

### Numeric Extremes

```cpp
store.set<int64_t>("limits/min", INT64_MIN);
store.set<int64_t>("limits/max", INT64_MAX);
store.set<double>("limits/near_zero", 1e-308);
store.set<double>("limits/large", 1e308);

// All retrieve exactly
```

### Integer Overflow

```cpp
// uint64_t exceeding INT64_MAX → type_error
uint64_t big = static_cast<uint64_t>(INT64_MAX) + 1;
auto status = store.set<uint64_t>("data/big", big);
// status == Status::type_error

// Read with insufficient range → nullopt
store.set<int64_t>("data/val", 300);
auto narrow = store.get<int8_t>("data/val");
// narrow == std::nullopt (300 > 127)
```

### Unicode and Special Characters

Full UTF-8 support with byte-exact preservation:

```cpp
store.set<std::string>("lang/rtl", "العربية");
store.set<std::string>("lang/cjk", "中文 日本語 한국語");
store.set<std::string>("lang/emoji", "🎉🚀🌟");

// All retrieve exactly as stored
```

### Large Values

```cpp
// 1 MB string
std::string large(1024 * 1024, 'x');
store.set<std::string>("data/large_string", large);

// 100K element vector
std::vector<int64_t> big_array(100000);
std::iota(big_array.begin(), big_array.end(), 0);
store.set<std::vector<int64_t>>("data/big_array", big_array);
```

### Persistence Across Sessions

Data persists immediately to the memory-mapped file. `unload()` + `load()` recovers everything:

```cpp
store.set<int64_t>("db/counter", 42);
store.unload("db");

// Later...
store.load("db", "/tmp/data.db");
auto val = store.get<int64_t>("db/counter");  // 42
```

---

## Compilation and Integration

### Requirements

- **C++23** (GCC 13+ or Clang 16+)
- **Boost.Interprocess** (managed by Conan)
- **CMake 3.16+**

### With Conan (Recommended)

```bash
# Install dependencies and configure
conan install . --output-folder=build --build=missing -s build_type=Release
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build -j
```

### CMake Subdirectory Integration

```cmake
add_subdirectory(vendor/akasha)
target_link_libraries(myapp akasha::akasha)
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `BUILD_EXAMPLE` | `OFF` | Compiles examples in `examples/` |
| `BUILD_TESTS` | `OFF` | Compiles test suite |
| `BUILD_BENCHMARKS` | `OFF` | Compiles benchmarks |
| `AKASHA_THREAD_SAFE` | `OFF` | Enable std::shared_mutex for multi-threaded access (5-15% overhead, required for concurrent use) |
| `AKASHA_BUILD_SINGLE_ARCHIVE` | `OFF` | Build bundled static archive with all static dependencies |

### Build and Run Examples

```bash
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_EXAMPLE=ON
cmake --build build -j

./build/akasha_quickstart
./build/akasha_serializable
./build/akasha_vectors
./build/akasha_datasetview
./build/akasha_nested_data
```

### Build and Run Tests

```bash
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON
cmake --build build -j
./build/akasha_tests
```
