#pragma once

/**
 * @file store_internal.hpp
 * @brief Private header for implementation files (NOT public API).
 *
 * This header is included by all Store implementation .cpp files and contains:
 * - Type aliases for Boost.Interprocess components
 * - MappedFileStorage struct definition
 * - Helper functions for serialization/deserialization
 * - Unified key-path parser shared between Store and Batch code
 *
 * Type system:
 * - SegmentManager: memory allocator for managed_mapped_file
 * - InterprocessString: key type (with heterogeneous lookup)
 * - InterprocessValue: value type (serialized data with type marker)
 * - InterprocessDatasetMap: the data structure (bip::map)
 *
 * Constants:
 * - kDatasetMapName: well-known name for root map in memory-mapped file
 * - kFormatVersionKeyName: version marker key
 * - kFormatVersion: current file format version (v2)
 * - kDefaultInitialMappedFileSize, kDefaultInitialGrowStep, kDefaultMaxGrowRetries: tuning
 *
 * Key functions:
 * - parse_key_path(): validates and splits "dataset/subkey/..." paths
 * - parse_batch_prefix(): lightweight variant for Batch mode
 * - serialize_tagged/deserialize_tagged: adds type marker to payloads
 *
 * Usage:
 * - Included by store_core.cpp, store_data.cpp, store_memory.cpp, store_views.cpp
 * - Never included by user code (private implementation detail)
 */

#include "akasha.hpp"

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/throw_exception.hpp>

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// ── Boost.Interprocess type aliases ──────────────────────────────────────────

namespace bip = boost::interprocess;

using SegmentManager = bip::managed_mapped_file::segment_manager;
using CharAllocator = bip::allocator<char, SegmentManager>;
using InterprocessString = bip::basic_string<char, std::char_traits<char>, CharAllocator>;
using InterprocessValue = InterprocessString;

struct InterprocessStringLess {
    bool operator()(const InterprocessString& a, const InterprocessString& b) const {
        return std::string_view(a.c_str(), a.size()) < std::string_view(b.c_str(), b.size());
    }
    bool operator()(const InterprocessString& a, std::string_view b) const {
        return std::string_view(a.c_str(), a.size()) < b;
    }
    bool operator()(std::string_view a, const InterprocessString& b) const {
        return a < std::string_view(b.c_str(), b.size());
    }
    using is_transparent = void;
};

using MapAllocator = bip::allocator<std::pair<const InterprocessString, InterprocessValue>, SegmentManager>;
using InterprocessDatasetMap = bip::map<InterprocessString, InterprocessValue, InterprocessStringLess, MapAllocator>;

// ── Well-known names and constants ───────────────────────────────────────────

constexpr const char* kDatasetMapName = "akasha_root";
constexpr const char* kFormatVersionKeyName = "akasha_version";
constexpr uint32_t kFormatVersion = 2u;
constexpr std::size_t kDefaultInitialMappedFileSize = 64 * 1024;
constexpr std::size_t kDefaultInitialGrowStep = kDefaultInitialMappedFileSize / 2;
constexpr int kDefaultMaxGrowRetries = 8;

// ── MappedFileStorage (definition visible to all Store .cpp files) ──────────

struct akasha::Store::MappedFileStorage {
    explicit MappedFileStorage(const std::string& path, std::size_t initial_size)
        : file(bip::open_or_create, path.c_str(), initial_size), file_path(path) {
    }

    bip::managed_mapped_file file;
    std::string file_path;  // Path to the memory-mapped file for backup/recovery operations
};

// ── Convenience casts ────────────────────────────────────────────────────────

inline InterprocessDatasetMap* as_dataset_map(void* ptr) {
    return static_cast<InterprocessDatasetMap*>(ptr);
}
inline const InterprocessDatasetMap* as_dataset_map(const void* ptr) {
    return static_cast<const InterprocessDatasetMap*>(ptr);
}

// ── Tagged serialization (prepend type tag byte to payload) ──────────────────

inline std::string serialize_tagged(akasha::detail::TypeTag tag, std::string_view payload) {
    std::string result;
    result.reserve(payload.size() + 1);
    result.push_back(static_cast<uint8_t>(tag));
    result.append(payload.data(), payload.size());
    return result;
}

inline std::pair<akasha::detail::TypeTag, std::string_view> deserialize_tagged(std::string_view tagged_data) {
    if (tagged_data.empty()) {
        return {akasha::detail::TypeTag::legacy_type, {}};
    }
    akasha::detail::TypeTag tag = static_cast<akasha::detail::TypeTag>(static_cast<uint8_t>(tagged_data[0]));
    std::string_view payload = tagged_data.substr(1);
    return {tag, payload};
}

// ── Key-path parsing ─────────────────────────────────────────────────────────
// Splits "dataset/subkey/..." into (dataset_id, subkey_rest).
// __root__ is always the implicit first segment after the dataset id:
//   "dataset"            → ("dataset",  "__root__")
//   "dataset/foo"        → ("dataset",  "__root__/foo")
//   "dataset/__root__"   → ("dataset",  "__root__")       (already explicit)
//   "dataset/__root__/x" → ("dataset",  "__root__/x")     (already explicit)
// Returns nullopt on invalid paths (empty, leading/trailing/double slashes).

inline std::optional<std::pair<std::string_view, std::string_view>>
parse_key_path(std::string_view key_path) {
    if (key_path.empty()) return std::nullopt;
    if (key_path.front() == '/') return std::nullopt;
    if (key_path.back() == '/') return std::nullopt;
    if (key_path.find("//") != std::string_view::npos) return std::nullopt;

    const std::size_t slash_pos = key_path.find('/');
    if (slash_pos == std::string_view::npos) {
        return std::pair{key_path, std::string_view("__root__")};
    }

    const auto dataset_id = key_path.substr(0, slash_pos);
    const auto rest       = key_path.substr(slash_pos + 1);

    // If the subkey already starts with __root__, return as-is.
    if (rest == std::string_view("__root__") || rest.starts_with("__root__/")) {
        return std::pair{dataset_id, rest};
    }

    // Otherwise, inject __root__/ before the subkey.
    // We return into a thread_local buffer so the string_view stays valid
    // for the caller's lifetime within the same call chain.
    thread_local std::string buf;
    buf.clear();
    buf += "__root__/";
    buf += rest;
    return std::pair{dataset_id, std::string_view(buf)};
}

// Lightweight variant for Batch: returns (dataset_id, subkey_prefix).
// Always injects __root__ as the first subkey segment:
//   "dataset"            → ("dataset", "__root__")
//   "dataset/foo"        → ("dataset", "__root__/foo")
//   "dataset/__root__"   → ("dataset", "__root__")
//   "dataset/__root__/x" → ("dataset", "__root__/x")
inline std::pair<std::string_view, std::string_view>
parse_batch_prefix(std::string_view key_prefix) {
    if (key_prefix.empty()) return {};
    const std::size_t slash = key_prefix.find('/');
    if (slash == std::string_view::npos) {
        return {key_prefix, std::string_view("__root__")};
    }

    const auto dataset_id = key_prefix.substr(0, slash);
    const auto rest       = key_prefix.substr(slash + 1);

    if (rest == std::string_view("__root__") || rest.starts_with("__root__/")) {
        return {dataset_id, rest};
    }

    thread_local std::string buf;
    buf.clear();
    buf += "__root__/";
    buf += rest;
    return {dataset_id, std::string_view(buf)};
}
