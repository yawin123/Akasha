#pragma once

/**
 * @file type_conversion.hpp
 * @brief Shared encode/decode helpers for scalar types.
 *
 * This header provides a single source of truth for serializing and deserializing
 * scalar types (bool, int64_t, double, std::string) to/from raw bytes.
 * These helpers are used by both Store::set/get and BatchWriter::set/BatchReader::get
 * to eliminate code duplication (~200 lines) and maintain consistency.
 *
 * The encoding format:
 * - bool: 1 byte (little-endian)
 * - int64_t: 8 bytes (little-endian)
 * - double: 8 bytes (IEEE 754)
 * - std::string: [size_t (8 bytes)][string data]
 *
 * All functions are marked [[nodiscard]] to encourage proper error handling.
 */

#include "akasha/core.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace akasha::detail {

// ── Encode helpers (value → raw bytes) ───────────────────────────────────────

struct EncodedScalar {
    alignas(double) char data[sizeof(double)]{};
    std::size_t size{0};
    TypeTag tag{TypeTag::null_type};

    [[nodiscard]] const void* ptr() const noexcept { return data; }
};

[[nodiscard]] inline EncodedScalar encode_bool(bool v) noexcept {
    EncodedScalar e;
    e.size = sizeof(bool);
    e.tag  = TypeTag::bool_type;
    std::memcpy(e.data, &v, sizeof(bool));
    return e;
}

[[nodiscard]] inline EncodedScalar encode_int64(std::int64_t v) noexcept {
    EncodedScalar e;
    e.size = sizeof(std::int64_t);
    e.tag  = TypeTag::int64_type;
    std::memcpy(e.data, &v, sizeof(std::int64_t));
    return e;
}

[[nodiscard]] inline EncodedScalar encode_double(double v) noexcept {
    EncodedScalar e;
    e.size = sizeof(double);
    e.tag  = TypeTag::double_type;
    std::memcpy(e.data, &v, sizeof(double));
    return e;
}

[[nodiscard]] inline std::vector<char> encode_string(const std::string& v) {
    std::vector<char> buf(sizeof(std::size_t) + v.size());
    const std::size_t len = v.size();
    std::memcpy(buf.data(), &len, sizeof(std::size_t));
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
    if (len > 0) std::memcpy(buf.data() + sizeof(std::size_t), v.data(), len);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    return buf;
}

// ── Decode helpers (raw bytes → value) ───────────────────────────────────────

[[nodiscard]] inline std::optional<bool> decode_bool(std::string_view raw) noexcept {
    if (raw.size() != sizeof(bool)) return std::nullopt;
    return *reinterpret_cast<const bool*>(raw.data());
}

template<typename T>
    requires std::is_integral_v<T>
[[nodiscard]] inline std::optional<T> decode_integer(std::string_view raw) noexcept {
    if (raw.size() != sizeof(std::int64_t)) return std::nullopt;
    std::int64_t stored;
    std::memcpy(&stored, raw.data(), sizeof(std::int64_t));
    if constexpr (!std::is_same_v<T, std::int64_t>) {
        if constexpr (std::is_unsigned_v<T>) {
            if (stored < 0 || static_cast<std::uint64_t>(stored) > static_cast<std::uint64_t>(std::numeric_limits<T>::max()))
                return std::nullopt;
        } else {
            if (stored < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
                stored > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
                return std::nullopt;
        }
    }
    return static_cast<T>(stored);
}

template<typename T>
    requires std::is_floating_point_v<T>
[[nodiscard]] inline std::optional<T> decode_floating(std::string_view raw) noexcept {
    if (raw.size() != sizeof(double)) return std::nullopt;
    double stored;
    std::memcpy(&stored, raw.data(), sizeof(double));
    return static_cast<T>(stored);
}

[[nodiscard]] inline std::optional<std::string> decode_string(std::string_view raw) {
    if (raw.size() < sizeof(std::size_t)) return std::nullopt;
    std::size_t len;
    std::memcpy(&len, raw.data(), sizeof(std::size_t));
    if (raw.size() != sizeof(std::size_t) + len) return std::nullopt;
    if (len == 0) return std::string{};
    return std::string(raw.data() + sizeof(std::size_t), len);
}

// ── Validation helpers ───────────────────────────────────────────────────────

template<typename T>
    requires (std::is_unsigned_v<T> && sizeof(T) >= sizeof(std::int64_t))
[[nodiscard]] constexpr bool int64_overflow(T v) noexcept {
    return v > static_cast<T>(std::numeric_limits<std::int64_t>::max());
}

}  // namespace akasha::detail
