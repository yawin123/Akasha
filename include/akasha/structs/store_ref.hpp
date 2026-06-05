#pragma once

/**
 * @file akasha/structs/store_ref.hpp
 * @brief StoreRef<T> — mutable proxy for a typed value in a Store.
 *
 * StoreRef is a generic, reusable proxy that binds a type T to a fixed key path
 * in a Store. It is designed to be returned by non-const element access in
 * akasha containers (e.g., akasha::vector<T>).
 *
 * Supports:
 *   - Implicit conversion to T: reads the value from the Store.
 *   - operator=(const T&) / operator=(T&&): writes a new value to the Store.
 *   - operator=(const StoreRef<T>&): reads from the source proxy, writes to this path.
 *
 * Usage:
 * @code
 *     akasha::StoreRef<int64_t> ref = vec[0];
 *     ref = 42;                         // persists to the Store
 *     int64_t v = ref;                  // reads from the Store
 *     vec[0] = vec[1];                  // proxy-to-proxy value transfer
 * @endcode
 */

#include "akasha/store.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace akasha {

/**
 * @brief Mutable proxy for a typed value at a fixed path in a Store.
 *
 * @note Copying a proxy creates a new proxy bound to the same path.
 *       Assignment between proxies transfers the value, not the binding.
 */
template<typename T>
class StoreRef {
public:
    StoreRef(Store& store, std::string path)
        : store_(store), path_(std::move(path)) {}

    StoreRef(const StoreRef&) = default;
    StoreRef(StoreRef&&)      = default;

    // ── Field-level mutation via operator-> ──────────────────────────────────
    //
    // Reads T from the store, exposes T* for field access, and writes T back
    // to the store when the proxy is destroyed (end of the enclosing expression).
    //
    //   ref->x = 3;   // reads T, sets .x = 3, persists on ~WriteProxy

    struct WriteProxy {
        T         value;
        StoreRef* self;

        explicit WriteProxy(T v, StoreRef* s) : value(std::move(v)), self(s) {}
        WriteProxy(const WriteProxy&)            = delete;
        WriteProxy& operator=(const WriteProxy&) = delete;
        WriteProxy(WriteProxy&& o) noexcept : value(std::move(o.value)), self(o.self) { o.self = nullptr; }

        T*       operator->()       noexcept { return &value; }
        const T* operator->() const noexcept { return &value; }

        ~WriteProxy() { if (self) *self = std::move(value); }
    };

    [[nodiscard]] WriteProxy operator->() {
        return WriteProxy{static_cast<T>(*this), this};
    }

    // Read-only member access for const StoreRef (e.g. const auto ref = m[key]).
    // Returns a temporary; do NOT store the pointer beyond the expression.
    struct ReadProxy {
        T value;
        const T* operator->() const noexcept { return &value; }
    };

    [[nodiscard]] ReadProxy operator->() const {
        return ReadProxy{static_cast<T>(*this)};
    }

    // ── Write ────────────────────────────────────────────────────────────────

    StoreRef& operator=(const T& value) {
        (void)store_.set<T>(path_, value);
        return *this;
    }

    StoreRef& operator=(T&& value) {
        (void)store_.set<T>(path_, std::move(value));
        return *this;
    }

    // Proxy-to-proxy assignment: reads value from other, writes it to this path.
    StoreRef& operator=(const StoreRef& other) {
        return (*this) = static_cast<T>(other);
    }

    // ── Read ─────────────────────────────────────────────────────────────────

    [[nodiscard]] T operator*() const { return static_cast<T>(*this); }

    [[nodiscard]] operator T() const {
        auto v = store_.get<T>(path_);
        if (!v) throw std::out_of_range("StoreRef: key not found: " + path_);
        return *v;
    }

    // ── Metadata ─────────────────────────────────────────────────────────────

    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    [[nodiscard]] Store& store() noexcept { return store_; }

private:
    Store& store_;
    std::string path_;
};

}  // namespace akasha
