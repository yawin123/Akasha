#pragma once

/**
 * @file akasha.hpp
 * @brief Main umbrella header for the Akasha hierarchical configuration store.
 *
 * Include this single file to use the complete Akasha library. All necessary
 * types, classes, and functions are brought into namespace akasha::.
 *
 * Quick start:
 * @code
 *     akasha::Store store;
 *     store.load("db.akasha", "mydb");
 *     store.set<int64_t>("mydb/counter", 42);
 *     auto value = store.get<int64_t>("mydb/counter");
 * @endcode
 *
 * Main components:
 * - Store: hierarchical key-value store with memory-mapped backend
 * - DatasetView: read-only navigation through tree nodes
 * - BatchWriter/BatchReader: transactional bulk operations
 * - Serializable<T>: user-defined type support
 *
 * Included headers (in dependency order):
 * - core.hpp: fundamental types
 * - store.hpp: Store class and DatasetView
 * - batch.hpp: BatchWriter and BatchReader
 * - store_serializable.hpp: custom type support
 * - stl_serialization.hpp: std::vector<T> support
 */

#include "akasha/core.hpp"
#include "akasha/store.hpp"
#include "akasha/batch.hpp"
#include "akasha/detail/store_serializable.hpp"
#include "akasha/detail/stl_serialization.hpp"
