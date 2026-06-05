#pragma once

/**
 * @file akasha/structs/vector.hpp
 * @brief akasha::vector<T> — persistent vector backed by a Store.
 *
 * Storage format (compatible with SequentialSerializable<std::vector<T>>):
 *   path/__count__  → int64_t  (number of elements)
 *   path/0          → T        (first element)
 *   path/1          → T        (second element)
 *   ...
 *
 * Interoperability: a std::vector<T> stored via store.set<std::vector<T>>(path, v)
 * can be wrapped directly with akasha::vector<T>(store, path) and vice versa.
 *
 * Usage:
 * @code
 *     akasha::vector<int64_t> vec(store, "db/numbers");
 *     vec.push_back(42);
 *     vec[0] = 100;                          // mutable proxy write
 *     int64_t v = vec[0];                    // implicit read
 *     std::cout << vec.size() << '\n';
 * @endcode
 */

#include "akasha/structs/container.hpp"
#include "akasha/structs/iterators.hpp"
#include "akasha/structs/store_ref.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace akasha {

// ── akasha::vector<T> ────────────────────────────────────────────────────────

/**
 * @brief Persistent vector backed by a Store.
 *
 * Non-const element access (operator[], at(), front(), back()) returns StoreRef<T>,
 * a mutable proxy that reads/writes directly to the Store.
 * Const element access returns T by value.
 *
 * Every element access and mutation goes directly to the Store (no local cache).
 *
 * @note Not thread-safe on its own. Thread safety is governed by the Store's
 *       AKASHA_THREAD_SAFE flag.
 * @note push_back() performs two Store writes (element + __count__). These are
 *       not atomic unless the Store itself uses a transaction.
 */
template<typename T>
class vector : public container {
public:
    // ── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief Attach to or create a vector at path in store.
     *
     * If __count__ does not exist at path, initializes it to 0 (empty vector).
     * If __count__ already exists, the vector uses the existing elements.
     */
    // ── Type aliases ─────────────────────────────────────────────────────────
    using value_type      = T;
    using reference       = StoreRef<T>;
    using const_reference = T;
    using pointer         = void;
    using const_pointer   = void;

    // ── Construction ─────────────────────────────────────────────────────────
    vector() noexcept : container() {}

    explicit vector(Store& store, std::string_view path)
        : container(store, path) {
        if (!store_->has(count_path())) {
            (void)store_->set<std::int64_t>(count_path(), 0);
        }
    }

    vector(Store& store, std::string_view path, size_type count, const T& value)
        : vector(store, path) {
        resize(count, value);
    }

    template<typename InputIt>
    vector(Store& store, std::string_view path, InputIt first, InputIt last)
        : vector(store, path) {
        for (auto it = first; it != last; ++it)
            push_back(*it);
    }

    vector(Store& store, std::string_view path, std::initializer_list<T> il)
        : vector(store, path, il.begin(), il.end()) {}

    vector(const vector&)            = delete;
    vector& operator=(const vector&) = delete;
    vector(vector&&)                 = default;
    vector& operator=(vector&&)      = default;

    // ── Size ─────────────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t size() const {
        return get_count();
    }

    [[nodiscard]] bool empty() const {
        return size() == 0;
    }

    // ── Element Access ───────────────────────────────────────────────────────

    /**
     * @brief Access element at index (non-const).
     * @return StoreRef<T> proxy. Writing to it persists the value.
     * @throw std::out_of_range if index >= size().
     */
    [[nodiscard]] StoreRef<T> operator[](std::size_t index) {
        bounds_check(index);
        return StoreRef<T>(*store_, element_path(index));
    }

    /**
     * @brief Access element at index (const).
     * @return T value copy read from the Store.
     * @throw std::out_of_range if index >= size() or element not found.
     */
    [[nodiscard]] T operator[](std::size_t index) const {
        bounds_check(index);
        return read_element(index);
    }

    /**
     * @brief Access element with bounds check (non-const).
     * @throw std::out_of_range if index >= size().
     */
    [[nodiscard]] StoreRef<T> at(std::size_t index) {
        bounds_check(index);
        return StoreRef<T>(*store_, element_path(index));
    }

    /**
     * @brief Access element with bounds check (const).
     * @throw std::out_of_range if index >= size() or element not found.
     */
    [[nodiscard]] T at(std::size_t index) const {
        bounds_check(index);
        return read_element(index);
    }

    [[nodiscard]] StoreRef<T> front() {
        if (empty()) throw std::out_of_range("akasha::vector::front: vector is empty");
        return StoreRef<T>(*store_, element_path(0));
    }

    [[nodiscard]] T front() const {
        if (empty()) throw std::out_of_range("akasha::vector::front: vector is empty");
        return read_element(0);
    }

    [[nodiscard]] StoreRef<T> back() {
        if (empty()) throw std::out_of_range("akasha::vector::back: vector is empty");
        return StoreRef<T>(*store_, element_path(size() - 1));
    }

    [[nodiscard]] T back() const {
        if (empty()) throw std::out_of_range("akasha::vector::back: vector is empty");
        return read_element(size() - 1);
    }

    // ── Modifiers ────────────────────────────────────────────────────────────

    template<typename U>
    void push_back(U&& value) {
        const auto idx = get_count();
        (void)store_->set<T>(element_path(idx), std::forward<U>(value));
        set_count(idx + 1);
    }

    /**
     * @brief Insert element at the beginning. O(n): shifts all existing elements right.
     */
    template<typename U>
    void push_front(U&& value) {
        const auto n = get_count();
        for (std::size_t i = n; i > 0; --i) {
            auto elem = store_->get<T>(element_path(i - 1));
            if (elem) (void)store_->set<T>(element_path(i), std::move(*elem));
        }
        (void)store_->set<T>(element_path(0), std::forward<U>(value));
        set_count(n + 1);
    }

    /**
     * @brief Remove the last element. O(1).
     * @throw std::out_of_range if empty.
     */
    void pop_back() {
        const auto n = get_count();
        if (n == 0) throw std::out_of_range("akasha::vector::pop_back: vector is empty");
        (void)store_->clear(element_path(n - 1));
        set_count(n - 1);
    }

    /**
     * @brief Remove the first element. O(n): shifts all remaining elements left.
     * @throw std::out_of_range if empty.
     */
    void pop_front() {
        const auto n = get_count();
        if (n == 0) throw std::out_of_range("akasha::vector::pop_front: vector is empty");
        for (std::size_t i = 0; i < n - 1; ++i) {
            auto elem = store_->get<T>(element_path(i + 1));
            if (elem) (void)store_->set<T>(element_path(i), std::move(*elem));
        }
        (void)store_->clear(element_path(n - 1));
        set_count(n - 1);
    }

    /**
     * @brief Remove all elements. Sets __count__ to 0 and clears each element key.
     */
    void clear() {
        const auto n = get_count();
        for (std::size_t i = 0; i < n; ++i) {
            (void)store_->clear(element_path(i));
        }
        set_count(0);
    }

    void resize(std::size_t new_size) {
        resize(new_size, T{});
    }

    void resize(std::size_t new_size, const T& value) {
        const auto current = get_count();
        if (new_size > current) {
            for (std::size_t i = current; i < new_size; ++i) {
                (void)store_->set<T>(element_path(i), value);
            }
        } else if (new_size < current) {
            for (std::size_t i = new_size; i < current; ++i) {
                (void)store_->clear(element_path(i));
            }
        }
        set_count(new_size);
    }

    // ── assign ────────────────────────────────────────────────────────────────

    void assign(size_type count, const T& value) { clear(); resize(count, value); }

    template<typename InputIt>
    void assign(InputIt first, InputIt last) {
        clear();
        for (auto it = first; it != last; ++it) push_back(*it);
    }

    void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

    // ── Iterators ─────────────────────────────────────────────────────────────
    //
    // iterator:       mutable  — operator* returns StoreRef<T> (writes persist to Store)
    // const_iterator: readonly — operator* returns T by value (proxy, like std::vector<bool>)
    //
    // Both are instantiations of detail::IndexIterator<Container, Reference>.
    // A converting constructor from iterator → const_iterator is provided.

    using iterator               = detail::IndexIterator<vector<T>,       StoreRef<T>, T>;
    using const_iterator         = detail::IndexIterator<const vector<T>, T,           T>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    [[nodiscard]] iterator       begin()        { return {this, 0}; }
    [[nodiscard]] iterator       end()          { return {this, static_cast<std::ptrdiff_t>(size())}; }
    [[nodiscard]] const_iterator begin()  const { return {this, 0}; }
    [[nodiscard]] const_iterator end()    const { return {this, static_cast<std::ptrdiff_t>(size())}; }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend()   const { return end(); }

    [[nodiscard]] reverse_iterator       rbegin()        { return reverse_iterator(end()); }
    [[nodiscard]] reverse_iterator       rend()          { return reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator rbegin()  const { return const_reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator rend()    const { return const_reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator crbegin() const { return rbegin(); }
    [[nodiscard]] const_reverse_iterator crend()   const { return rend(); }

    // ── insert ────────────────────────────────────────────────────────────────

    iterator insert(const_iterator pos, const T& value) {
        return insert_impl(pos.index(), value);
    }
    iterator insert(const_iterator pos, T&& value) {
        return insert_impl(pos.index(), std::move(value));
    }
    iterator insert(const_iterator pos, size_type count, const T& value) {
        const auto idx = pos.index();
        shift_right(idx, count);
        for (std::size_t i = 0; i < count; ++i)
            (void)store_->set<T>(element_path(idx + i), value);
        set_count(get_count() + count);
        return {this, static_cast<std::ptrdiff_t>(idx)};
    }
    template<typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        const auto idx = pos.index();
        std::vector<T> vals(first, last);
        const auto count = vals.size();
        shift_right(idx, count);
        for (std::size_t i = 0; i < count; ++i)
            (void)store_->set<T>(element_path(idx + i), vals[i]);
        set_count(get_count() + count);
        return {this, static_cast<std::ptrdiff_t>(idx)};
    }
    iterator insert(const_iterator pos, std::initializer_list<T> il) {
        return insert(pos, il.begin(), il.end());
    }

    // ── emplace_back / emplace ────────────────────────────────────────────────

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        push_back(T(std::forward<Args>(args)...));
        return back();
    }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        return insert(pos, T(std::forward<Args>(args)...));
    }

    // ── erase ─────────────────────────────────────────────────────────────────

    iterator erase(const_iterator pos) {
        const auto idx = pos.index();
        bounds_check(idx);
        const auto n = get_count();
        shift_left(idx + 1, 1);
        (void)store_->clear(element_path(n - 1));
        set_count(n - 1);
        return {this, static_cast<std::ptrdiff_t>(idx)};
    }
    iterator erase(const_iterator first, const_iterator last) {
        const auto from  = first.index();
        const auto to    = last.index();
        const auto count = to - from;
        const auto n     = get_count();
        shift_left(to, count);
        for (std::size_t i = n - count; i < n; ++i)
            (void)store_->clear(element_path(i));
        set_count(n - count);
        return {this, static_cast<std::ptrdiff_t>(from)};
    }

    // ── swap / comparison ─────────────────────────────────────────────────────

    void swap(vector& other) noexcept { container::swap(other); }

    [[nodiscard]] bool operator==(const vector& other) const {
        if (size() != other.size()) return false;
        for (std::size_t i = 0; i < size(); ++i)
            if (read_element(i) != other.read_element(i)) return false;
        return true;
    }
    [[nodiscard]] bool operator!=(const vector& other) const { return !(*this == other); }

private:
    [[nodiscard]] std::string count_path() const {
        return path_ + "/__count__";
    }

    [[nodiscard]] std::string element_path(std::size_t index) const {
        return path_ + "/" + std::to_string(index);
    }

    [[nodiscard]] std::size_t get_count() const {
        auto v = store_->get<std::int64_t>(count_path());
        if (!v || *v < 0) return 0;
        return static_cast<std::size_t>(*v);
    }

    void set_count(std::size_t count) {
        (void)store_->set<std::int64_t>(count_path(), static_cast<std::int64_t>(count));
    }

    void bounds_check(std::size_t index) const {
        const auto n = get_count();
        if (index >= n) {
            throw std::out_of_range(
                "akasha::vector: index " + std::to_string(index) +
                " out of range (size = " + std::to_string(n) + ")"
            );
        }
    }

    [[nodiscard]] T read_element(std::size_t index) const {
        auto v = store_->get<T>(element_path(index));
        if (!v) throw std::out_of_range(
            "akasha::vector: element not found at index " + std::to_string(index));
        return *v;
    }

    // Shift elements [from, n) right by `count` positions to make room for insertions.
    void shift_right(std::size_t from, std::size_t count) {
        const auto n = get_count();
        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(n) - 1;
             i >= static_cast<std::ptrdiff_t>(from); --i) {
            auto elem = store_->get<T>(element_path(static_cast<std::size_t>(i)));
            if (elem) (void)store_->set<T>(
                element_path(static_cast<std::size_t>(i) + count), std::move(*elem));
        }
    }

    // Shift elements [from, n) left by `count` positions (overwriting erased slots).
    void shift_left(std::size_t from, std::size_t count) {
        const auto n = get_count();
        for (std::size_t i = from; i < n; ++i) {
            auto elem = store_->get<T>(element_path(i));
            if (elem) (void)store_->set<T>(element_path(i - count), std::move(*elem));
        }
    }

    template<typename U>
    iterator insert_impl(std::size_t idx, U&& value) {
        shift_right(idx, 1);
        (void)store_->set<T>(element_path(idx), std::forward<U>(value));
        set_count(get_count() + 1);
        return {this, static_cast<std::ptrdiff_t>(idx)};
    }
};

template<typename T>
struct SequentialSerializable<vector<T>> {
    static void serialize(const vector<T>& v, BatchWriter& bw) {
        const auto src_dataset  = v.path().substr(0, v.path().find('/'));
        const auto dest_dataset = bw.root_key().substr(0, bw.root_key().find('/'));
        if (src_dataset == dest_dataset) {
            // Same dataset: BatchWriter holds the lock. Snapshot first.
            bw.unlock();
            std::vector<T> tmp(v.cbegin(), v.cend());
            bw.lock();
            std::size_t i = 0;
            for (const T& val : tmp) (void)bw.set<T>(std::to_string(i++), val);
        } else {
            std::size_t i = 0;
            for (auto it = v.cbegin(); it != v.cend(); ++it)
                (void)bw.set<T>(std::to_string(i++), *it);
        }
    }

    static std::optional<vector<T>> deserialize(const BatchReader& br) {
        // __count__ not present means this path was never written as a vector.
        auto count = br.get_count();
        if (!count) return std::nullopt;
        // Release the lock before constructing: the normal vector constructor calls
        // store_.has() which would deadlock trying to re-acquire the same
        // non-recursive lock (AKASHA_THREAD_SAFE mode).
        auto& br_mut = const_cast<BatchReader&>(br);
        const std::string key(br.root_key());
        br_mut.unlock();
        return vector<T>(br_mut.store(), key);
    }

    static std::int64_t size(const vector<T>& v) {
        return static_cast<std::int64_t>(v.size());
    }
};

} // namespace akasha
