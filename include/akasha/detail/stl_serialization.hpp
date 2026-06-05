#pragma once

/**
 * @file stl_serialization.hpp
 * @brief SequentialSerializable and ArbitrarySerializable specializations for STL containers.
 *
 * Provides automatic serialization support for:
 * - std::vector<T>: homogeneous sequences
 * - std::array<T,N>: fixed-size homogeneous sequences
 * - std::list<T>: homogeneous linked lists
 * - std::set<T>: ordered homogeneous sets
 * - std::unordered_set<T>: unordered homogeneous sets
 * - std::map<K,V>: key-value pairs (both keys and values serialized)
 * - std::unordered_map<K,V>: unordered key-value pairs
 *
 * All containers are decomposed into indexed subkeys (container/0, container/1, ...)
 * with automatic __count__ metadata for reconstruction.
 *
 * This file is included by akasha.hpp AFTER BatchWriter and BatchReader are
 * fully defined, since the template implementations depend on them.
 *
 * Usage examples:
 * @code
 *     std::vector<int64_t> v = {1, 2, 3};
 *     store.set<std::vector<int64_t>>("db/numbers", v);
 *     
 *     std::array<double, 3> a = {1.0, 2.0, 3.0};
 *     store.set<std::array<double, 3>>("db/coords", a);
 *     
 *     std::list<std::string> lst = {"a", "b", "c"};
 *     store.set<std::list<std::string>>("db/items", lst);
 *     
 *     std::set<int64_t> s = {1, 2, 3};
 *     store.set<std::set<int64_t>>("db/unique", s);
 *     
 *     std::map<std::string, int64_t> m = {{"a", 1}, {"b", 2}};
 *     store.set<std::map<std::string, int64_t>>("db/config", m);
 * @endcode
 *
 * Storage format:
 * - All serializable to indexed subkeys (path/0, path/1, ...)
 * - Each element is serialized individually with its own type tag
 * - Automatic __count__ metadata for reconstruction
 * - For maps: alternating key-value pairs (path/0=key, path/1=value, path/2=key, ...)
 */

#include "akasha/core.hpp"
#include "akasha/detail/type_conversion.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <set>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <iterator>

namespace akasha {
    // Type Traits ---
    template <typename T, typename = void>
    struct is_iterable : std::false_type {};

    template <typename T>
    struct is_iterable<T, std::void_t<
        decltype(std::begin(std::declval<T&>())),
        decltype(std::end(std::declval<T&>())),
        typename T::value_type  // Nos aseguramos de que tenga un tipo interno definido
    >> : std::true_type {};

    template <typename T>
    inline constexpr bool is_iterable_v = is_iterable<T>::value;

    template <typename T, typename = void>
    struct has_mapped_type : std::false_type {};

    template <typename T>
    struct has_mapped_type<T, std::void_t<typename T::mapped_type>> : std::true_type {};

    template <typename T, typename = void>
    struct has_tuple_size : std::false_type {};

    template <typename T>
    struct has_tuple_size<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type {};

    template <typename T, typename = void>
    struct has_akasha_container_tag : std::false_type {};

    template <typename T>
    struct has_akasha_container_tag<T, std::void_t<typename T::akasha_container_tag>> : std::true_type {};

    // Especialización genérica: iterable, no mapa, no array (tamaño fijo), no string, no akasha container
    template <typename T>
    struct SequentialSerializable<T, std::enable_if_t<
        is_iterable_v<T> &&
        !has_mapped_type<T>::value &&
        !has_tuple_size<T>::value &&
        !std::is_same_v<T, std::string> &&
        !has_akasha_container_tag<T>::value
    >> {
        using ElementType = typename T::value_type;

        static void serialize(const T& v, BatchWriter& bw) {
            std::size_t i = 0;
            for (const auto& elem : v) {
                (void)bw.set(std::to_string(i++), static_cast<ElementType>(elem));
            }
        }

        static std::optional<T> deserialize(const BatchReader& br) {
            auto count_opt = br.get_count();
            if (!count_opt || *count_opt < 0) return std::nullopt;
            
            const std::size_t count = static_cast<std::size_t>(*count_opt);
            T result;

            if constexpr (requires(T& c) { c.reserve(0); }) {
                result.reserve(count);
            }

            auto it = std::inserter(result, result.end());

            for (std::size_t i = 0; i < count; ++i) {
                auto elem = br.get<ElementType>(std::to_string(i));
                if (!elem) return std::nullopt;
                
                *it = std::move(*elem);
            }
            return result;
        }

        static std::int64_t size(const T& v) {
            return static_cast<std::int64_t>(std::size(v));
        }
    };

    // ===== std::array<T, N> =====
    template<typename T, std::size_t N>
    struct SequentialSerializable<std::array<T, N>> {
        static void serialize(const std::array<T, N>& arr, BatchWriter& bw) {
            for (std::size_t i = 0; i < N; ++i) {
                (void)bw.set(std::to_string(i), arr[i]);
            }
        }

        static std::optional<std::array<T, N>> deserialize(const BatchReader& br) {
            auto count_opt = br.get_count();
            if (!count_opt || *count_opt != static_cast<std::int64_t>(N)) return std::nullopt;

            std::array<T, N> result{};
            for (std::size_t i = 0; i < N; ++i) {
                auto elem = br.get<T>(std::to_string(i));
                if (!elem) return std::nullopt;
                result[i] = std::move(*elem);
            }
            return result;
        }

        static std::int64_t size(const std::array<T, N>&) {
            return static_cast<std::int64_t>(N);
        }
    };

    // Especialización genérica para contenedores con mapped_type (map, unordered_map, ...)
    // Excluye akasha::map que tiene su propia especialización explícita.
    template <typename T>
    struct ArbitrarySerializable<T, std::enable_if_t<
        is_iterable_v<T> &&
        has_mapped_type<T>::value &&
        !has_akasha_container_tag<T>::value
    >> {
        using KeyType    = typename T::key_type;
        using MappedType = typename T::mapped_type;

        static void serialize(const T& m, BatchWriter& bw) {
            for (const auto& [k, v] : m) {
                (void)bw.set<MappedType>(detail::map_key_to_string(k), v);
            }
        }

        static std::optional<T> deserialize(const BatchReader& br) {
            auto children = br.get_children();
            T result;
            if constexpr (requires(T& c) { c.reserve(std::size_t{}); }) {
                result.reserve(children.size());
            }
            for (const auto& child_key : children) {
                auto val = br.get<MappedType>(child_key);
                if (!val) return std::nullopt;
                result[detail::string_to_map_key<KeyType>(child_key)] = std::move(*val);
            }
            return result;
        }

        static std::vector<std::string> keys(const T& m) {
            std::vector<std::string> ks;
            ks.reserve(m.size());
            for (const auto& [k, v] : m) {
                ks.push_back(detail::map_key_to_string(k));
            }
            return ks;
        }
    };

}  // namespace akasha
