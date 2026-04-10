#pragma once

/**
 * @file core.hpp
 * @brief Core types and concepts for the Akasha library.
 *
 * This header defines fundamental types and concepts required by all other
 * components. It has NO dependencies on Store, BatchWriter, or BatchReader,
 * making it safe to include from anywhere without circular dependency issues.
 *
 * Provides:
 * - Status enum: result codes for all Store operations
 * - FileOptions: flags for load() behavior (create_if_missing, migrate_if_incompatible)
 * - PerformanceTuning: configurable growth parameters
 * - TypeTag enum: binary markers for serialized data (bool_type, int64_type, etc.)
 * - Serializable<T>, SequentialSerializable<T>, ArbitrarySerializable<T>: user-defined type traits
 * - IsFixedSerializable, IsSequentialSerializable, IsArbitrarySerializable: detection concepts
 * - Forward declarations: BatchWriter, BatchReader (to avoid circular includes)
 */

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <limits>

namespace akasha {

/**
 * @brief Semantic version of the library.
 */
[[nodiscard]] inline std::string_view version() noexcept {
#ifdef AKASHA_VERSION
	return AKASHA_VERSION;
#else
	return "0.0.0-dev";
#endif
}

using KeyPath = std::string;

namespace detail {
    enum class TypeTag : std::uint8_t {
        null_type   = 0x00,
        bool_type   = 0x01,
        int64_type  = 0x02,
        double_type = 0x03,
        string_type = 0x04,
        legacy_type = 0xFF,
    };
}

enum class Status {
    ok,
    invalid_key_path,
    invalid_file_path,
    key_conflict,
    file_read_error,
    file_write_error,
    file_not_found,
    file_full,
    parse_error,
    dataset_not_found,
    key_not_found,
    source_already_loaded,
    incompatible_format,
    type_error,
};

enum class FileOptions {
    none = 0,
    create_if_missing = 1,
    migrate_if_incompatible = 2,
};

inline FileOptions operator|(FileOptions lhs, FileOptions rhs) {
    return static_cast<FileOptions>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
inline FileOptions operator&(FileOptions lhs, FileOptions rhs) {
    return static_cast<FileOptions>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
inline FileOptions operator~(FileOptions x) {
    return static_cast<FileOptions>(~static_cast<int>(x));
}
inline FileOptions& operator|=(FileOptions& lhs, FileOptions rhs) { lhs = lhs | rhs; return lhs; }
inline FileOptions& operator&=(FileOptions& lhs, FileOptions rhs) { lhs = lhs & rhs; return lhs; }

struct PerformanceTuning {
    std::size_t initial_mapped_file_size{64 * 1024};
    std::size_t initial_grow_step{(64 * 1024) / 2};
    int max_grow_retries{8};
};

// ── Serializable<T> trait ────────────────────────────────────────────────────
// Users specialize this for custom types.
// After the refactoring, serialize/deserialize receive BatchWriter/BatchReader.
// (BatchWriter and BatchReader are defined in akasha/batch.hpp and are forward-
// declared here so the IsSerializable concept compiles.)

class BatchWriter;  // forward declaration
class BatchReader;  // forward declaration

// ── Serializable<T> — fixed-field structs ────────────────────────────────────
template<typename T>
struct Serializable;  // primary template, intentionally undefined

// ── SequentialSerializable<T> — indexed containers (vector, deque, …) ────────
// Infrastructure writes __count__ automatically; user implements serialize,
// deserialize, and size(v) → int64_t (element count).
template<typename T, typename = void>
struct SequentialSerializable;  // primary template, intentionally undefined

// ── ArbitrarySerializable<T> — key-based containers (map, set, …) ────────────
// Infrastructure writes __children__ automatically; user implements serialize,
// deserialize, and keys(v) → vector<string> (direct child key names).
template<typename T, typename = void>
struct ArbitrarySerializable;  // primary template, intentionally undefined

// ── Detection concepts (lightweight, work with forward-declared Batch types) ──
template<typename T>
concept IsFixedSerializable = requires {
    typename std::void_t<decltype(&Serializable<T>::serialize)>;
    typename std::void_t<decltype(&Serializable<T>::deserialize)>;
};

template<typename T>
concept IsSequentialSerializable = requires {
    typename std::void_t<decltype(&SequentialSerializable<T>::serialize)>;
    typename std::void_t<decltype(&SequentialSerializable<T>::deserialize)>;
    typename std::void_t<decltype(&SequentialSerializable<T>::size)>;
} && !std::is_same_v<T, std::string>;

template<typename T>
concept IsArbitrarySerializable = requires {
    typename std::void_t<decltype(&ArbitrarySerializable<T>::serialize)>;
    typename std::void_t<decltype(&ArbitrarySerializable<T>::deserialize)>;
    typename std::void_t<decltype(&ArbitrarySerializable<T>::keys)>;
};

// ── IsSerializable — any of the three categories ─────────────────────────────
template<typename T>
concept IsSerializable = IsFixedSerializable<T> || IsSequentialSerializable<T> || IsArbitrarySerializable<T>;

}  // namespace akasha
