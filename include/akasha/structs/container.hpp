#pragma once

/**
 * @file akasha/structs/container.hpp
 * @brief akasha::container — non-virtual base class for all persistent containers.
 *
 * Provides the common state (Store reference + key path) and accessors shared
 * by all akasha persistent containers (akasha::vector<T>, akasha::map<K,V>, …).
 *
 * No virtual functions. Polymorphism is handled at compile time via the
 * IsAkashaContainer concept and std::derived_from.
 *
 * All derived containers must call the base constructor:
 * @code
 *     class vector : public akasha::container {
 *     public:
 *         explicit vector(Store& store, std::string_view path)
 *             : akasha::container(store, path) { ... }
 *     };
 * @endcode
 */

#include "akasha/store.hpp"

#include <concepts>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

namespace akasha {

/**
 * @brief Non-virtual base class for all persistent akasha containers.
 *
 * Holds a reference to the Store and the key path where the container is rooted.
 * Derived classes are not copyable (Store& cannot be rebound), but are movable.
 */
class container {
public:
    // ── Common type aliases ───────────────────────────────────────────────────
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    // ── Construction ──────────────────────────────────────────────────────────
    container() noexcept : store_(nullptr), path_() {}

    explicit container(Store& store, std::string_view path)
        : store_(&store), path_(path) {}

    container(const container&)            = delete;
    container& operator=(const container&) = delete;
    container(container&&)                 = default;
    container& operator=(container&&)      = default;

    // ── Observers ─────────────────────────────────────────────────────────────
    // Tag para distinguir contenedores akasha de contenedores STL en las
    // especializaciones genéricas de SequentialSerializable/ArbitrarySerializable.
    using akasha_container_tag = void;

    [[nodiscard]] std::string_view path() const noexcept { return path_; }
    [[nodiscard]] Store& store() noexcept { return *store_; }
    [[nodiscard]] const Store& store() const noexcept { return *store_; }

    // ── Capacity ──────────────────────────────────────────────────────────────
    [[nodiscard]] static constexpr size_type max_size() noexcept {
        return std::numeric_limits<size_type>::max();
    }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void swap(container& other) noexcept {
        std::swap(store_, other.store_);
        std::swap(path_,  other.path_);
    }

protected:
    Store* store_;
    std::string path_;
};

// ── IsAkashaContainer concept ─────────────────────────────────────────────────

/**
 * @brief Satisfied by any type that derives from akasha::container.
 *
 * Used to provide Store::set/get overloads that operate on akasha containers
 * without conflicting with the existing Serializable machinery.
 */
template<typename T>
concept IsAkashaContainer = std::derived_from<T, container>;

}  // namespace akasha
