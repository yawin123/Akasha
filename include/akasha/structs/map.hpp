#pragma once

/**
 * @file akasha/structs/map.hpp
 * @brief akasha::map<K, V> — persistent map backed by a Store.
 *
 * Storage format (compatible with ArbitrarySerializable<std::map<K, V>>):
 *   path/<map_key_to_string(k)>   → V    (value for each key)
 *   path/__children__             → string  (newline-separated list of key strings)
 *
 * Interoperability: a std::map<K, V> (or std::unordered_map<K, V>) stored via
 * store.set<std::map<K,V>>(path, m) can be wrapped directly with
 * akasha::map<K,V>(store, path) and vice versa.
 *
 * Usage:
 * @code
 *     akasha::map<std::string, int64_t> m(store, "db/counters");
 *     m.insert("hits", 0);
 *
 *     m["hits"] = m["hits"] + 1;          // mutable proxy write + read
 *     std::cout << m.at("hits") << '\n';  // const read, throws if not found
 *     std::cout << m.size() << '\n';
 *
 *     for (auto [k, v] : m) { ... }       // forward iteration
 * @endcode
 */

#include "akasha/structs/container.hpp"
#include "akasha/structs/store_ref.hpp"
#include "akasha/detail/type_conversion.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace akasha {

// ── akasha::map<K, V> ────────────────────────────────────────────────────────

/**
 * @brief Persistent map backed by a Store.
 *
 * Non-const element access (operator[]) returns StoreRef<V>, a mutable proxy
 * that reads/writes directly to the Store. Const access (at(), operator[] const)
 * returns V by value.
 *
 * The key catalogue is maintained in a `__children__` subkey (newline-separated
 * list of string-encoded keys), mirroring the ArbitrarySerializable format used
 * for std::map<K,V>. This guarantees round-trip compatibility.
 *
 * Every access and mutation goes directly to the Store (no local cache).
 *
 * @note Not thread-safe on its own. Thread safety is governed by the Store's
 *       AKASHA_THREAD_SAFE flag.
 * @note insert() performs two Store writes (value + __children__ update). These
 *       are not atomic unless the Store itself uses a transaction.
 */
template<typename K, typename V>
class map : public container {
public:
    // ── Type aliases ──────────────────────────────────────────────────────────
    using key_type        = K;
    using mapped_type     = V;
    using value_type      = std::pair<const K, V>;
    using reference       = std::pair<K, StoreRef<V>>;
    using const_reference = std::pair<K, V>;
    using pointer         = void;
    using const_pointer   = void;

    // ── Construction ──────────────────────────────────────────────────────────

    /**
     * @brief Attach to or create a map at path in store.
     *
     * If __children__ does not exist at path, initialises it to "" (empty map).
     * If __children__ already exists, the map uses the existing entries.
     */
    map() noexcept : container() {}

    explicit map(Store& store, std::string_view path)
        : container(store, path) {
        if (!store_->has(children_path())) {
            (void)store_->set<std::string>(children_path(), "");
        }
    }

    map(const map&)            = delete;
    map& operator=(const map&) = delete;
    map(map&&)                 = default;
    map& operator=(map&&)      = default;

    // ── Iterators ────────────────────────────────────────────────────────────
    // Defined early so all methods below can use iterator/const_iterator types.
    //
    // iterator:       mutable — operator* returns {first: K, second: StoreRef<V>}
    //                           it->second = x writes through to the Store.
    // const_iterator: readonly — operator* returns std::pair<K,V> by value.
    // Both are bidirectional. Neither caches; each dereference hits the Store.

    // Forward-declared so const_iterator can declare it as a friend.
    class iterator;

    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = std::pair<K, V>;
        using reference         = std::pair<K, V>;
        using pointer           = void;
        using difference_type   = std::ptrdiff_t;

        struct arrow_proxy {
            std::pair<K, V> val;
            const std::pair<K, V>* operator->() const noexcept { return &val; }
        };

        const_iterator() = default;

        [[nodiscard]] reference operator*() const {
            const auto& ks = keys_[index_];
            K k  = detail::string_to_map_key<K>(ks);
            auto v = map_->store().template get<V>(std::string(map_->path()) + '/' + ks);
            if (!v) throw std::out_of_range(
                "akasha::map iterator: value not found for key: " + ks);
            return {std::move(k), std::move(*v)};
        }
        [[nodiscard]] arrow_proxy operator->() const { return {**this}; }

        const_iterator& operator++()    { ++index_; return *this; }
        const_iterator  operator++(int) { auto t = *this; ++index_; return t; }
        const_iterator& operator--()    { --index_; return *this; }
        const_iterator  operator--(int) { auto t = *this; --index_; return t; }

        [[nodiscard]] bool operator==(const const_iterator& o) const {
            if (map_ != o.map_) {
                const bool this_end  = (map_  == nullptr || index_  >= keys_.size());
                const bool other_end = (o.map_ == nullptr || o.index_ >= o.keys_.size());
                return this_end && other_end;
            }
            const bool this_end  = index_  >= keys_.size();
            const bool other_end = o.index_ >= o.keys_.size();
            if (this_end && other_end) return true;
            if (this_end != other_end) return false;
            return index_ == o.index_;
        }
        [[nodiscard]] bool operator!=(const const_iterator& o) const { return !(*this == o); }

    private:
        friend class map<K, V>;
        friend class iterator;

        const_iterator(const map<K,V>* m, std::vector<std::string> keys, std::size_t idx)
            : map_(m), keys_(std::move(keys)), index_(idx) {}

        const map<K,V>*          map_  = nullptr;
        std::vector<std::string> keys_;
        std::size_t              index_ = 0;
    };

    class iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<const K, V>;
        using pointer           = void;

        // Mutable reference: first is the key, second is a StoreRef<V> proxy.
        // Write-back via: it->second = newval   (scalar)
        //                 it->second->field = x  (struct field, uses WriteProxy)
        struct reference {
            K           first;
            StoreRef<V> second;
        };
        struct arrow_proxy {
            reference ref;
            reference*       operator->()       noexcept { return &ref; }
            const reference* operator->() const noexcept { return &ref; }
        };

        iterator() = default;

        [[nodiscard]] reference& operator*() const {
            const auto& ks = keys_[index_];
            cached_ = reference{detail::string_to_map_key<K>(ks),
                                 StoreRef<V>(map_->store(), std::string(map_->path()) + '/' + ks)};
            return *cached_;
        }
        [[nodiscard]] arrow_proxy operator->() const { return {**this}; }

        iterator& operator++()    { ++index_; return *this; }
        iterator  operator++(int) { auto t = *this; ++index_; return t; }
        iterator& operator--()    { --index_; return *this; }
        iterator  operator--(int) { auto t = *this; --index_; return t; }

        [[nodiscard]] bool operator==(const iterator& o) const {
            if (map_ != o.map_) {
                const bool this_end  = (map_  == nullptr || index_  >= keys_.size());
                const bool other_end = (o.map_ == nullptr || o.index_ >= o.keys_.size());
                return this_end && other_end;
            }
            const bool this_end  = index_  >= keys_.size();
            const bool other_end = o.index_ >= o.keys_.size();
            if (this_end && other_end) return true;
            if (this_end != other_end) return false;
            return index_ == o.index_;
        }
        [[nodiscard]] bool operator!=(const iterator& o) const { return !(*this == o); }

        // Implicit conversion to const_iterator (mirrors std::map).
        operator const_iterator() const {
            return const_iterator(const_cast<const map<K,V>*>(map_), keys_, index_);
        }

    private:
        friend class map<K, V>;
        friend class const_iterator;

        iterator(map<K,V>* m, std::vector<std::string> keys, std::size_t idx)
            : map_(m), keys_(std::move(keys)), index_(idx) {}

        map<K,V>*                        map_  = nullptr;
        std::vector<std::string>         keys_;
        std::size_t                      index_ = 0;
        mutable std::optional<reference> cached_;
    };

    // Custom reverse_iterator: std::reverse_iterator doesn't work here because
    // its operator*() creates a temporary copy of the base iterator, decrements
    // it, and returns *tmp — a reference into tmp.cached_ that is immediately
    // dangling. This class owns its own cache so the reference is stable.
    class reverse_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = typename iterator::value_type;
        using reference         = typename iterator::reference;
        using pointer           = void;

        reverse_iterator() = default;
        explicit reverse_iterator(iterator it) : current_(std::move(it)) {}

        [[nodiscard]] reference& operator*() const {
            auto tmp = current_;
            --tmp;
            cached_ = *tmp;  // copia el optional<reference> del iterador temporal
            return *cached_;
        }

        reverse_iterator& operator++()    { --current_; return *this; }
        reverse_iterator  operator++(int) { auto t = *this; --current_; return t; }
        reverse_iterator& operator--()    { ++current_; return *this; }
        reverse_iterator  operator--(int) { auto t = *this; ++current_; return t; }

        [[nodiscard]] bool operator==(const reverse_iterator& o) const { return current_ == o.current_; }
        [[nodiscard]] bool operator!=(const reverse_iterator& o) const { return current_ != o.current_; }

        iterator base() const { return current_; }

    private:
        iterator                         current_;
        mutable std::optional<reference> cached_;
    };

    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    [[nodiscard]] iterator       begin()        { auto k = load_key_strings(); return iterator(this, k, 0); }
    [[nodiscard]] iterator       end()          { auto k = load_key_strings(); return iterator(this, k, k.size()); }
    [[nodiscard]] const_iterator begin()  const { auto k = load_key_strings(); return const_iterator(this, k, 0); }
    [[nodiscard]] const_iterator end()    const { auto k = load_key_strings(); return const_iterator(this, k, k.size()); }
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    [[nodiscard]] const_iterator cend()   const { return end(); }

    [[nodiscard]] reverse_iterator       rbegin()        { return reverse_iterator(end()); }
    [[nodiscard]] reverse_iterator       rend()          { return reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator rbegin()  const { return const_reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator rend()    const { return const_reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator crbegin() const { return rbegin(); }
    [[nodiscard]] const_reverse_iterator crend()   const { return rend(); }

    // ── Size ─────────────────────────────────────────────────────────────────

    [[nodiscard]] std::size_t size() const {
        auto v = store_->get<std::string>(children_path());
        if (!v || v->empty()) return 0;
        return static_cast<std::size_t>(std::count(v->begin(), v->end(), '\n') + 1);
    }

    [[nodiscard]] bool empty() const { return size() == 0; }

    // ── Lookup ────────────────────────────────────────────────────────────────

    [[nodiscard]] bool contains(const K& key) const {
        return store_->has(value_path(key));
    }

    [[nodiscard]] size_type count(const K& key) const {
        return contains(key) ? 1 : 0;
    }

    [[nodiscard]] const_iterator find(const K& key) const {
        auto keys = load_key_strings();
        const auto ks = detail::map_key_to_string(key);
        for (std::size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == ks) return const_iterator(this, std::move(keys), i);
        // end iterator
        auto k2 = load_key_strings();
        return const_iterator(this, std::move(k2), k2.size());
    }

    [[nodiscard]] iterator find(const K& key) {
        auto keys = load_key_strings();
        const auto ks = detail::map_key_to_string(key);
        for (std::size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == ks) return iterator(this, std::move(keys), i);
        auto k2 = load_key_strings();
        return iterator(this, std::move(k2), k2.size());
    }

    /**
     * @brief Read element by key.
     * @return V by value.
     * @throw std::out_of_range if key not found.
     */
    [[nodiscard]] V at(const K& key) const {
        auto v = store_->get<V>(value_path(key));
        if (!v) throw std::out_of_range(
            "akasha::map::at: key not found: " + detail::map_key_to_string(key));
        return *v;
    }

    /**
     * @brief Const element access. Equivalent to at().
     * @throw std::out_of_range if key not found.
     */
    [[nodiscard]] V operator[](const K& key) const {
        return at(key);
    }

    /**
     * @brief Mutable element access. Inserts a default-constructed V if the
     *        key is not present (same semantics as std::map::operator[]).
     * @return StoreRef<V> proxy. Writing to it persists the value.
     * @note Requires V to be default-constructible when used with a new key.
     */
    [[nodiscard]] StoreRef<V> operator[](const K& key) {
        if (!contains(key)) {
            insert(key, V{});
        }
        return StoreRef<V>(*store_, value_path(key));
    }

    // ── Modifiers ────────────────────────────────────────────────────────────

    // insert(key, value) — akasha-native signature (kept for backwards compat)
    template<typename W>
    void insert(const K& key, W&& value) {
        const auto ks     = detail::map_key_to_string(key);
        const bool is_new = !store_->has(value_path(key));
        const auto status = store_->set<V>(value_path(key), std::forward<W>(value));
        if (is_new && status == Status::ok) add_child_key(ks);
    }

    // insert(pair) — std::map-compatible signature, returns pair<iterator,bool>
    std::pair<iterator, bool> insert(const value_type& kv) {
        return insert_kv(kv.first, kv.second);
    }
    std::pair<iterator, bool> insert(value_type&& kv) {
        return insert_kv(kv.first, std::move(kv.second));
    }
    // Brace-init: insert({"key", val})
    std::pair<iterator, bool> insert(std::pair<K, V> kv) {
        return insert_kv(kv.first, std::move(kv.second));
    }

    void insert(std::initializer_list<value_type> il) {
        for (const auto& kv : il) insert(kv.first, kv.second);
    }

    // insert range
    template<typename InputIt>
    void insert(InputIt first, InputIt last) {
        auto current_opt = store_->get<std::string>(children_path());
        std::string children = (current_opt && !current_opt->empty()) ? *current_opt : "";

        for (auto it = first; it != last; ++it) {
            const auto ks     = detail::map_key_to_string(it->first);
            const bool is_new = !store_->has(value_path(it->first));
            (void)store_->set<V>(value_path(it->first), it->second);
            if (is_new) {
                if (!children.empty()) children += '\n';
                children += ks;
            }
        }
        (void)store_->set<std::string>(children_path(), children);
    }

    // emplace — constructs V in-place
    template<typename... Args>
    std::pair<iterator, bool> emplace(const K& key, Args&&... args) {
        return insert_kv(key, V(std::forward<Args>(args)...));
    }

    // try_emplace — only inserts if key absent
    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const K& key, Args&&... args) {
        if (contains(key)) {
            auto it = find(key);
            return {it, false};
        }
        return insert_kv(key, V(std::forward<Args>(args)...));
    }

    // insert_or_assign — always writes, returns bool=true if inserted
    template<typename W>
    std::pair<iterator, bool> insert_or_assign(const K& key, W&& value) {
        const bool is_new = !contains(key);
        insert(key, std::forward<W>(value));
        return {find(key), is_new};
    }

    // erase(key) — returns number of erased elements (0 or 1)
    size_type erase(const K& key) {
        if (!contains(key)) return 0;
        (void)store_->clear(value_path(key));
        remove_child_key(detail::map_key_to_string(key));
        return 1;
    }

    // erase(iterator) — std::map-compatible
    iterator erase(const_iterator pos) {
        auto keys = load_key_strings();
        if (pos.index_ >= keys.size()) return end();
        const auto ks = keys[pos.index_];
        (void)store_->clear(path_ + '/' + ks);
        remove_child_key(ks);
        auto new_keys = load_key_strings();
        const auto new_idx = std::min(pos.index_, new_keys.size());
        return iterator(this, std::move(new_keys), new_idx);
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto keys = load_key_strings();
        for (std::size_t i = first.index_; i < last.index_ && i < keys.size(); ++i) {
            (void)store_->clear(path_ + '/' + keys[i]);
        }
        // rebuild children from scratch
        auto all = load_key_strings();
        std::string joined;
        for (std::size_t i = 0; i < all.size(); ++i) {
            bool removed = false;
            for (std::size_t j = first.index_; j < last.index_ && j < keys.size(); ++j)
                if (all[i] == keys[j]) { removed = true; break; }
            if (!removed) {
                if (!joined.empty()) joined += '\n';
                joined += all[i];
            }
        }
        (void)store_->set<std::string>(children_path(), joined);
        auto new_keys = load_key_strings();
        const auto new_idx = std::min(first.index_, new_keys.size());
        return iterator(this, std::move(new_keys), new_idx);
    }

    void clear() {
        for (const auto& ks : load_key_strings()) {
            (void)store_->clear(path_ + '/' + ks);
        }
        (void)store_->set<std::string>(children_path(), "");
    }

    // ── swap / comparison ─────────────────────────────────────────────────────

    void swap(map& other) noexcept { container::swap(other); }

    [[nodiscard]] bool operator==(const map& other) const {
        if (size() != other.size()) return false;
        for (const auto& [k, v] : *this) {
            if (!other.contains(k)) return false;
            if (other.at(k) != v)   return false;
        }
        return true;
    }
    [[nodiscard]] bool operator!=(const map& other) const { return !(*this == other); }

    // ── Children catalogue (used by ArbitrarySerializable) ───────────────────

    [[nodiscard]] std::vector<std::string> load_key_strings() const {
        auto v = store_->get<std::string>(children_path());
        if (!v || v->empty()) return {};
        std::vector<std::string> result;
        bool dirty = false;
        std::string_view sv = *v;
        while (!sv.empty()) {
            const auto nl = sv.find('\n');
            std::string_view entry = (nl == std::string_view::npos) ? sv : sv.substr(0, nl);
            if (!entry.empty()) {
                const std::string full_path = path_ + '/' + std::string(entry);
                if (store_->has(full_path)) {
                    result.emplace_back(entry);
                } else {
                    dirty = true;  // phantom key detected
                }
            }
            if (nl == std::string_view::npos) break;
            sv = sv.substr(nl + 1);
        }
        // Auto-repair: rewrite __children__ without phantom keys
        if (dirty) {
            std::string repaired;
            for (const auto& k : result) {
                if (!repaired.empty()) repaired += '\n';
                repaired += k;
            }
            (void)store_->set<std::string>(children_path(), repaired);
        }
        return result;
    }

private:
    [[nodiscard]] std::string children_path() const {
        return path_ + "/__children__";
    }

    [[nodiscard]] std::string value_path(const K& key) const {
        return path_ + '/' + detail::map_key_to_string(key);
    }

    void add_child_key(const std::string& ks) {
        auto current_opt = store_->get<std::string>(children_path());
        std::string current = (current_opt && !current_opt->empty()) ? *current_opt : "";
        if (!current.empty()) current += '\n';
        current += ks;
        (void)store_->set<std::string>(children_path(), current);
    }

    void remove_child_key(const std::string& ks) {
        auto keys = load_key_strings();
        keys.erase(std::remove(keys.begin(), keys.end(), ks), keys.end());
        std::string joined;
        for (const auto& k : keys) {
            if (!joined.empty()) joined += '\n';
            joined += k;
        }
        (void)store_->set<std::string>(children_path(), joined);
    }

    template<typename W>
    std::pair<iterator, bool> insert_kv(const K& key, W&& value) {
        const bool is_new = !contains(key);
        if (is_new)
            insert(key, std::forward<W>(value));
        return {find(key), is_new};
    }
};

// ── ArbitrarySerializable<map<K,V>> ──────────────────────────────────────────

template<typename K, typename V>
struct ArbitrarySerializable<map<K,V>> {
    static void serialize(const map<K,V>& m, BatchWriter& bw) {
        const auto src_dataset  = std::string(m.path()).substr(0, std::string(m.path()).find('/'));
        const auto dest_dataset = std::string(bw.root_key()).substr(0, std::string(bw.root_key()).find('/'));

        if (src_dataset == dest_dataset) {
            // Same dataset: BatchWriter holds the exclusive file lock.
            // Any Store::get on the same dataset would deadlock (non-recursive mutex).
            // Solution: unlock → snapshot all pairs → relock → write.
            bw.unlock();
            const auto keys = m.load_key_strings();
            std::vector<std::pair<std::string, V>> tmp;
            tmp.reserve(keys.size());
            for (const auto& ks : keys) {
                auto v = m.store().template get<V>(std::string(m.path()) + '/' + ks);
                if (v) tmp.emplace_back(ks, std::move(*v));
            }
            bw.lock();
            for (auto& [ks, v] : tmp) {
                (void)bw.set<V>(ks, v);
            }
        } else {
            const auto keys = m.load_key_strings();
            for (const auto& ks : keys) {
                auto v = m.store().template get<V>(std::string(m.path()) + '/' + ks);
                if (v) (void)bw.set<V>(ks, std::move(*v));
            }
        }
    }

    static std::optional<map<K,V>> deserialize(const BatchReader& br) {
        // __children__ absent → path was never written as an ArbitrarySerializable map.
        if (!br.has("__children__")) return std::nullopt;
        // Release the lock before constructing: the map constructor calls
        // store_.has() which would deadlock trying to re-acquire the same
        // non-recursive lock (AKASHA_THREAD_SAFE mode).
        auto& br_mut = const_cast<BatchReader&>(br);
        const std::string key(br.root_key());
        br_mut.unlock();
        return map<K,V>(br_mut.store(), key);
    }

    static std::vector<std::string> keys(const map<K,V>& m) {
        return m.load_key_strings();
    }
};

} // namespace akasha
