#pragma once

/**
 * @file akasha/structs/iterators.hpp
 * @brief Generic iterator templates for akasha persistent containers.
 *
 * Provides:
 * - detail::IndexIterator<Container, Reference>
 *     Random-access iterator for index-based containers (akasha::vector, etc.).
 *     Container must expose: operator[](size_t) → Reference, size() → size_t.
 *     Parametrized by Reference so that the same template covers both:
 *       - mutable:  IndexIterator<vector<T>,       StoreRef<T>>  (*it writes to Store)
 *       - const:    IndexIterator<const vector<T>, T>            (*it reads by value)
 *     A converting constructor from mutable to const is provided (mirrors std::vector).
 */

#include <compare>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace akasha::detail {

// ── IndexIterator ─────────────────────────────────────────────────────────────

/**
 * @brief Generic random-access iterator for index-based akasha containers.
 *
 * @tparam Container  The container type (may be const-qualified for read-only iteration).
 * @tparam Reference  The type returned by operator*. Use StoreRef<T> for mutable
 *                    iterators and T for const iterators (proxy-by-value).
 *
 * @note operator* calls container.operator[](index) on every dereference —
 *       there is no local cache. Each read/write goes directly to the Store.
 */
template<typename Container, typename Reference, typename ValueType>
class IndexIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = ValueType;
    using difference_type   = std::ptrdiff_t;
    using reference         = Reference;
    using pointer           = void;

    IndexIterator() = default;
    IndexIterator(Container* container, std::ptrdiff_t index)
        : container_(container), index_(index) {}

    // Converting constructor: mutable → const (mirrors std::vector iterator conversion).
    // Enabled only when Container is const-qualified and OtherContainer is not.
    template<typename OtherContainer, typename OtherRef, typename OtherVal>
        requires (std::is_const_v<Container> && !std::is_const_v<OtherContainer> &&
                  std::is_same_v<std::remove_const_t<Container>, OtherContainer>)
    IndexIterator(const IndexIterator<OtherContainer, OtherRef, OtherVal>& other) noexcept
        : container_(other.container_), index_(other.index_) {}

    // ── Dereference ───────────────────────────────────────────────────────────

    [[nodiscard]] reference operator*() const {
        return (*container_)[static_cast<std::size_t>(index_)];
    }

    // Arrow proxy: holds the Reference type.
    // - Mutable iterator (Reference = StoreRef<T>): operator->() returns StoreRef<T>*,
    //   so it->operator=(x) and *it-> ... persist to the Store.
    // - Const iterator (Reference = T): operator->() returns const T*, read-only.
    struct arrow_proxy {
        Reference ref;
        Reference*       operator->()       noexcept { return &ref; }
        const Reference* operator->() const noexcept { return &ref; }
    };
    [[nodiscard]] arrow_proxy operator->() const {
        return {(*container_)[static_cast<std::size_t>(index_)]};
    }

    [[nodiscard]] reference operator[](difference_type n) const {
        return (*container_)[static_cast<std::size_t>(index_ + n)];
    }

    // ── Increment / Decrement ─────────────────────────────────────────────────

    IndexIterator& operator++()    { ++index_; return *this; }
    IndexIterator  operator++(int) { auto t = *this; ++index_; return t; }
    IndexIterator& operator--()    { --index_; return *this; }
    IndexIterator  operator--(int) { auto t = *this; --index_; return t; }

    // ── Arithmetic ────────────────────────────────────────────────────────────

    IndexIterator& operator+=(difference_type n) { index_ += n; return *this; }
    IndexIterator& operator-=(difference_type n) { index_ -= n; return *this; }

    friend IndexIterator operator+(IndexIterator it, difference_type n) { it += n; return it; }
    friend IndexIterator operator+(difference_type n, IndexIterator it) { it += n; return it; }
    friend IndexIterator operator-(IndexIterator it, difference_type n) { it -= n; return it; }

    friend difference_type operator-(const IndexIterator& a, const IndexIterator& b) {
        return a.index_ - b.index_;
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    bool operator==(const IndexIterator& o) const { return index_ == o.index_; }
    auto operator<=>(const IndexIterator& o) const { return index_ <=> o.index_; }

    [[nodiscard]] std::size_t index() const noexcept {
        return static_cast<std::size_t>(index_);
    }

private:
    template<typename, typename, typename> friend class IndexIterator;

    Container*     container_ = nullptr;
    std::ptrdiff_t index_     = 0;
};

}  // namespace akasha::detail
