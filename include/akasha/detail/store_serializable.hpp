#pragma once

/**
 * @file store_serializable.hpp
 * @brief Store::set/get template specializations for custom types.
 *
 * This header defines Store::set/get overloads for three trait categories:
 * - Serializable<T>: fixed-field structs (user provides serialize/deserialize)
 * - SequentialSerializable<T>: indexed containers (vector, deque, etc.)
 * - ArbitrarySerializable<T>: key-based containers (map, set, etc.)
 *
 * @note This file is included by akasha.hpp AFTER akasha/batch.hpp is complete,
 *       because these templates instantiate BatchWriter/BatchReader.
 *
 * Design:
 * - All three variants wrap the target in a temporary BatchWriter/BatchReader
 * - Metadata is automatically managed (__count__ for Sequential, __children__ for Arbitrary)
 * - User specializations inherit from one of these three traits and implement
 *   serialize(value, bw_or_br) and deserialize(bw_or_br) methods
 *
 * Example usage:
 * @code
 *     struct Point { double x, y; };
 *     template<> struct akasha::Serializable<Point> {
 *         static void serialize(const Point& p, akasha::BatchWriter& bw) {
 *             bw.set<double>("x", p.x);
 *             bw.set<double>("y", p.y);
 *         }
 *         static std::optional<Point> deserialize(const akasha::BatchReader& br) {
 *             auto x = br.get<double>("x");
 *             auto y = br.get<double>("y");
 *             return x && y ? Point{*x, *y} : std::optional<Point>();
 *         }
 *     };
 * @endcode
 */

#include "akasha/core.hpp"

namespace akasha {

// ── Store::set ───────────────────────────────────────────────────────────────

template<typename T>
    requires IsSequentialSerializable<T>
inline Status Store::set(std::string_view key_path, const T& value) {
    BatchWriter bw(*this, key_path);
    if (!bw.is_locked()) return Status::file_write_error;
    SequentialSerializable<T>::serialize(value, bw);
    (void)bw.set<std::int64_t>("__count__", static_cast<std::int64_t>(SequentialSerializable<T>::size(value)));
    return bw.commit();
}

template<typename T>
    requires IsArbitrarySerializable<T>
inline Status Store::set(std::string_view key_path, const T& value) {
    BatchWriter bw(*this, key_path);
    if (!bw.is_locked()) return Status::file_write_error;
    ArbitrarySerializable<T>::serialize(value, bw);
    const auto children_keys = ArbitrarySerializable<T>::keys(value);
    std::string joined;
    for (const auto& ck : children_keys) { if (!joined.empty()) joined += '\n'; joined += ck; }
    (void)bw.set<std::string>("__children__", joined);
    return bw.commit();
}

template<typename T>
    requires IsFixedSerializable<T>
inline Status Store::set(std::string_view key_path, const T& value) {
    BatchWriter bw(*this, key_path);
    if (!bw.is_locked()) return Status::file_write_error;
    bw.clear_children();
    Serializable<T>::serialize(value, bw);
    return bw.commit();
}

// ── Store::get ───────────────────────────────────────────────────────────────

template<typename T>
    requires IsSequentialSerializable<T>
inline std::optional<T> Store::get(std::string_view key_path) const {
    BatchReader br(*this, key_path);
    if (!br.is_locked()) return std::nullopt;
    return SequentialSerializable<T>::deserialize(br);
}

template<typename T>
    requires IsArbitrarySerializable<T>
inline std::optional<T> Store::get(std::string_view key_path) const {
    BatchReader br(*this, key_path);
    if (!br.is_locked()) return std::nullopt;
    return ArbitrarySerializable<T>::deserialize(br);
}

template<typename T>
    requires IsFixedSerializable<T>
inline std::optional<T> Store::get(std::string_view key_path) const {
    BatchReader br(*this, key_path);
    if (!br.is_locked()) return std::nullopt;
    return Serializable<T>::deserialize(br);
}

}  // namespace akasha
