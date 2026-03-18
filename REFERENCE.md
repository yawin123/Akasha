# Akasha API Reference

Complete guide to the Akasha 1.0.0 API.

## Basic Concepts

**Store**: Main object that manages multiple datasets mapped to persistent memory-mapped files.

**Dataset**: Independent set of data identified by `source_id`. Each dataset occupies a separate memory-mapped file.

**KeyPath**: Hierarchical path in dot notation. Minimum 1 segment. 
> Examples: `"counter"`, `"config.timeout"`, `"settings.subsection.property"`

**Status**: Enum with 10 error codes indicating the result of any operation.

## Typical Lifecycle

```cpp
akasha::Store store;

// 1. Load a dataset
auto status = store.load("config", "/path/to/config.dat", true);
if (status != akasha::Status::ok) {
    std::cerr << "Error loading: " << static_cast<int>(status) << std::endl;
}

// 2. Set values
store.set<int64_t>("config.timeout", 30);
store.set<std::string>("config.name", "MyApp");

// 3. Read values
auto timeout = store.get<int64_t>("config.timeout");
if (timeout.has_value()) {
    std::cout << "Timeout: " << *timeout << std::endl;
}

// 4. Clear dataset
store.clear("config");

// 5. Unload
store.unload("config");
```

## Store Methods

### `load(source_id, file_path, create_if_missing = false)`

Load a dataset from a memory-mapped file.

```cpp
Status status = store.load("db", "/path/to/data.db", false);
```

- `source_id`: Unique dataset identifier (e.g., "config", "user", "cache").
- `file_path`: Absolute or relative path to the file.
- `create_if_missing`: If true, creates an empty file if it doesn't exist.

**Possible errors:**
- `source_already_loaded`: Dataset is already loaded.
- `file_not_found`: File doesn't exist and `create_if_missing=false`.
- `file_read_error`: Error reading the file.
- `parse_error`: Corrupted internal data.

### `unload(source_id)`

Unload a dataset and close its memory-mapped file.

```cpp
Status status = store.unload("db");
```

Data persists on disk but is no longer accessible from this Store instance.

### `set<T>(key_path, value)`

Set a typed value at a key.

```cpp
store.set<int64_t>("config.retries", 3);
store.set<double>("config.threshold", 0.95);
store.set<std::string>("config.version", "1.0");
```

The value is persisted immediately to the memory-mapped file.

**Supported types:**
- Scalars: bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double
- Strings: std::string (serialized as [length][data])
- Custom trivially_copyable structures

**Possible errors:**
- `invalid_key_path`: Path is not in `dataset.key` format.
- `dataset_not_found`: Dataset is not loaded.
- `key_conflict`: Key exists but is an intermediate node, not a leaf.
- `file_write_error`: Error writing to file.
- `file_full`: Insufficient space (growth attempt failed).

### `get<T>(key_path)`

Get a typed value from a key.

```cpp
auto retries = store.get<int64_t>("config.retries");
if (retries.has_value()) {
    std::cout << "Retries: " << *retries << std::endl;
}
```

Reads from memory-mapped file and returns a copy of the stored value.

**Behavior:**
- If the key does not exist or has no value, returns `std::nullopt`.

### `get<DatasetView>(key_path)`

Get a navigable view over an node.

```cpp
auto cfg = store.get<akasha::Store::DatasetView>("config.database");
if (cfg.has_value()) {
    auto host = cfg->get<std::string>("host");
    auto port = cfg->get<int64_t>("port");
}
```

Allows navigating subkeys relative to the current position.

### `getorset<T>(key_path, default_value)`

Get a value or set it with a default if it doesn't exist.

```cpp
auto timeout = store.getorset<int64_t>("config.timeout", 30);
// If "config.timeout" didn't exist, it now contains 30
```

Useful for lazy initialization of configuration values.

### `has(key_path)`

Check if a key exists.

```cpp
if (store.has("config.timeout")) {
    auto val = store.get<int64_t>("config.timeout");
}
```

Does not distinguish between leaf and intermediate nodes.

### `clear(key_path = "")`

Delete persisted data.

```cpp
// Delete everything
store.clear();

// Delete entire dataset
store.clear("config");

// Delete a subtree
store.clear("config.database");
```

- If `key_path` is empty, deletes all data from all datasets.
- If it includes only a dataset (e.g., "user"), deletes all data in that dataset.
- If it includes a subkey, deletes that key and its entire subtree.

### `compact(dataset_id = "")`

Compact a memory-mapped file to reclaim space.

```cpp
// Compact a specific dataset
store.compact("config");

// Compact all datasets
store.compact();
```

Useful after `clear()` operations to free disk space.

### `last_status()`

Get the status of the last operation.

```cpp
auto result = store.get<int64_t>("config.missing");
if (result == std::nullopt) {
    auto status = store.last_status();
    std::cerr << "Status: " << static_cast<int>(status) << std::endl;
}
```

Returns `Status::ok` if the last operation succeeded, otherwise returns the error code.

## DatasetView Methods

`DatasetView` is a navigable read-only window into a node and its descendants. Obtained via `store.get<DatasetView>(key_path)`.

### `has_value()`

Check if the node has a direct atomic value.

```cpp
auto view = store.get<akasha::Store::DatasetView>("config.server");
if (view && view->has_value()) {
    // This node has a direct value (not just descendants)
}
```

**Returns:**
- `true` if a value exists at this exact node
- `false` if node is a container only (intermediate node with children)

### `has_keys()`

Check if the node has descendant keys.

```cpp
auto view = store.get<akasha::Store::DatasetView>("config.server");
if (view && view->has_keys()) {
    // This node has child keys
}
```

**Returns:**
- `true` if there are any keys under this node
- `false` if node is a leaf (direct value only, no descendants)

### `keys()`

Get immediate child keys (non-recursive).

```cpp
auto view = store.get<akasha::Store::DatasetView>("config.server");
if (view) {
    auto child_keys = view->keys();
    for (const auto& key : child_keys) {
        std::cout << key << "\n";  // Only immediate children
    }
}
```

**Returns:** `std::vector<std::string>` with immediate children only (not descendants).

### `get<T>(relative_path)`

Navigate relatively from this view.

```cpp
auto view = store.get<akasha::Store::DatasetView>("config.server");
if (view) {
    // Navigate relative paths
    auto port = view->template get<int32_t>("port");
    auto host = view->template get<std::string>("host");
    
    // Get nested views
    auto ssl_view = view->template get<akasha::Store::DatasetView>("ssl");
}
```

**Parameters:**
- `relative_path`: Path relative to this view (can be empty for this view itself)

**Returns:** `std::optional<T>` with the value/view if it exists.

### `has(relative_path)`

Check if a relative path exists from this view.

```cpp
auto view = store.get<akasha::Store::DatasetView>("config.server");
if (view && view->has("port")) {
    // This view has a "port" child
}
```

**Returns:** `true` if the relative path exists, `false` otherwise.

## Status Enum

```cpp
enum class Status {
    ok = 0,
    invalid_key_path,       // Key doesn't have at least 2 dot-separated segments
    key_conflict,           // Conflict on write (e.g., overwriting intermediate with leaf)
    file_read_error,        // Could not read from memory-mapped file
    file_write_error,       // Could not write to memory-mapped file
    file_not_found,         // File doesn't exist and create_if_missing=false
    file_full,              // File full after growth attempts
    parse_error,            // Error parsing internal data
    dataset_not_found,      // Dataset (first key segment) not loaded
    source_already_loaded   // Dataset already exists (duplicate source_id in load)
};
```

## Performance Tuning

Configure file growth parameters:

```cpp
akasha::PerformanceTuning tuning;
tuning.initial_mapped_file_size = 1024 * 1024;  // 1 MB initial
tuning.initial_grow_step = 512 * 1024;          // Grow 512 KB each time
tuning.max_grow_retries = 16;                   // Attempt up to 16 times

store.set_performance_tuning(tuning);
```

Get current configuration:

```cpp
auto tuning = store.performance_tuning();
std::cout << "Initial size: " << tuning.initial_mapped_file_size << " bytes" << std::endl;
```

## Usage Examples

### Atomic Dataset Values

Store a single value at dataset root:

```cpp
akasha::Store store;
store.load("counter", "./counter.dat", true);

// Set an atomic int64 at dataset root
store.set<int64_t>("counter", 42);

// Read it back
auto val = store.get<int64_t>("counter");
if (val.has_value()) {
    std::cout << "Counter: " << *val << std::endl;
}

// Check existence
if (store.has("counter")) {
    std::cout << "Counter exists" << std::endl;
}

// Clear the dataset
store.clear("counter");
```

### Application Configuration

```cpp
akasha::Store config;
config.load("app", "./config.dat", true);

// Initialize with defaults
config.getorset<std::string>("app.name", "MyApp");
config.getorset<int64_t>("app.port", 8080);
config.getorset<std::string>("app.version", "1.0.0");

// Read later
auto port = config.get<int64_t>("app.port");
```

### Multiple Datasets

```cpp
akasha::Store store;
store.load("config", "./config.dat", true);
store.load("cache", "./cache.dat", true);
store.load("state", "./state.dat", true);

// Each dataset is independent
store.set<int64_t>("config.timeout", 30);
store.set<int64_t>("cache.entries", 1000);
store.set<std::string>("state.status", "running");
```

### Custom Structures

```cpp
struct Point {
    int64_t x, y;
};
static_assert(std::is_trivially_copyable_v<Point>);

store.set<Point>("map.origin", {0, 0});
auto p = store.get<Point>("map.origin");
if (p.has_value()) {
    std::cout << "(" << p->x << ", " << p->y << ")" << std::endl;
}
```

### Hierarchical Navigation

```cpp
auto cfg = store.get<akasha::Store::DatasetView>("app.database");
if (cfg.has_value()) {
    auto keys = cfg->keys();
    for (const auto& key : keys) {
        std::cout << key << std::endl;
    }
    
    auto host = cfg->get<std::string>("host");
    auto port = cfg->get<int64_t>("port");
}
```

## Security and Considerations

- **Thread-safety**: Operations on the same Store from multiple threads are safe. Each memory-mapped file has its own `std::shared_mutex`.

- **Type responsibility**: You are 100% responsible for ensuring that type `T` used in `set<T>` and `get<T>` is consistent. There is no runtime type validation.

- **Persistence**: All writes with `set<T>()` are immediate and persistent. There is no internal buffer.

- **File growth**: If a `set<T>()` requires more space, the file grows automatically according to `PerformanceTuning`. If growth fails, `Status::file_full` is returned.

- **Trivially copyable**: Only trivially copyable types are supported. Structs with pointers, internal strings, or custom logic will not work correctly.

## Compilation Cycle

Include in your CMake project:

```cmake
add_subdirectory(vendor/akasha)
target_link_libraries(myapp akasha::akasha)
```

Compile with C++23:

```cmake
set(CMAKE_CXX_STANDARD 23)
```

## Building Examples

Examples are in `examples/` but do not compile by default.

To build examples:

```bash
cmake . -B build -DBUILD_EXAMPLE=ON
cmake --build build -j
```

Run:

```bash
./build/akasha_quickstart
./build/akasha_benchmarks
```

CMake auto-resets `BUILD_EXAMPLE` to `OFF` after compilation, so subsequent builds without the flag only rebuild the library.

## Diagnostics

To verify what loaded correctly:

```cpp
akasha::Store store;
auto status = store.load("test", "/path/to/test.dat", true);

if (status != akasha::Status::ok) {
    std::cerr << "Error: " << static_cast<int>(status) << std::endl;
}

// Verify datasets with has()
if (store.has("test.key")) {
    std::cout << "Dataset 'test' loaded successfully" << std::endl;
}
```

## Next Steps

- Read the examples in `examples/` for common usage patterns.
- Consult `include/akasha.hpp` for inline method documentation.
- Report issues on the repository if you encounter unexpected behavior.

