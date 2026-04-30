#pragma once

/**
 * @file batch.hpp
 * @brief Batch-mode API for transactional writes and reads.
 *
 * @note This file is included by akasha.hpp AFTER the Store class is fully defined.
 *       For users: include only "akasha.hpp"; this is an implementation detail.
 *
 * BatchWriter and BatchReader provide a scoped, transactional interface for:
 * - Sequential navigation through nested keys (push_key/pop_key)
 * - Atomic commits with automatic lock management
 * - Efficient bulk operations on tree structures
 *
 * Architecture:
 * - BatchStruct: common base with shared state (source, key_stack, file_lock)
 * - BatchWriter: exclusive lock, set<T>() with push_key context, commit() to flush
 * - BatchReader: shared lock, get<T>() for reading with navigation
 *
 * Lock semantics:
 * - Acquired in constructor (blocking if needed)
 * - Released in destructor (or unlock() for BatchWriter)
 * - Prevents concurrent Store::unload() while batch is active
 *
 * All encode/decode logic is in detail/type_conversion.hpp to avoid duplication.
 */

#include "akasha/core.hpp"
#include "akasha/detail/mutex.hpp"
#include "akasha/detail/type_conversion.hpp"

#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

namespace akasha {

// ============================================================================
// BatchStruct — common base for BatchWriter and BatchReader
// ============================================================================
class BatchStruct {
protected:
    BatchStruct(const Store& store, std::string_view key_prefix);
    ~BatchStruct() = default;

    BatchStruct(const BatchStruct&) = delete;
    BatchStruct& operator=(const BatchStruct&) = delete;
    BatchStruct(BatchStruct&&) = delete;
    BatchStruct& operator=(BatchStruct&&) = delete;

    Store& store_;
    Store::Source* source_{nullptr};
    std::string dataset_id_;
    std::unique_lock<detail::FileLockMutex> file_lock_;
    mutable std::stack<std::string> key_stack_;

    virtual void OnLockAcquired() {}
    virtual void OnLockReleased() {}

    // Pushes a key onto the stack, accumulating the full path.
    // Used internally by set<T>/get<T> for nested structures.
    void push_key(std::string_view relative_key) const;
    void pop_key() const;

public:

    void lock();
    void unlock();
    [[nodiscard]] bool is_locked() const noexcept { return static_cast<bool>(file_lock_); }

    /**
     * @brief Indicates if the relative path exists within this view.
        * @param key_path Relative path to the current view.
        */
    [[nodiscard]] bool has(std::string_view key_path) const;

    /**
     * @brief Indicates if this node has a direct value (is a leaf).
        * @return true if a value exists in this node, false otherwise.
        */
    [[nodiscard]] bool has_value() const;

    /**
     * @brief Indicates if this node has descendant keys.
        * @return true if there are keys under this node, false otherwise.
        */
    [[nodiscard]] bool has_keys() const;

    /**
     * @return Vector with immediate keys (excluding subkeys).
        */
    [[nodiscard]] std::vector<std::string> keys() const;
};

// ============================================================================
// BatchWriter
// ============================================================================
class BatchWriter : public BatchStruct {
public:
    explicit BatchWriter(const Store& store, std::string_view key_prefix);
    ~BatchWriter() noexcept;

    BatchWriter(const BatchWriter&) = delete;
    BatchWriter& operator=(const BatchWriter&) = delete;
    BatchWriter(BatchWriter&&) = delete;
    BatchWriter& operator=(BatchWriter&&) = delete;

    [[nodiscard]] Status set_null(std::string_view relative_key);
    [[nodiscard]] Status commit();
    void clear_children();
    [[nodiscard]] Status set_raw(std::string_view relative_key,
                                 const void* bytes, std::size_t size,
                                 detail::TypeTag tag);

    // ── set<T> overloads ─────────────────────────────────────────────────────

    template<typename T>
        requires std::is_same_v<T, bool>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        auto e = detail::encode_bool(v);
        return set_raw(k, e.ptr(), e.size, e.tag);
    }

    template<typename T>
        requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        if constexpr (std::is_unsigned_v<T> && sizeof(T) >= sizeof(std::int64_t)) {
            if (detail::int64_overflow(v)) return Status::type_error;
        }
        auto e = detail::encode_int64(static_cast<std::int64_t>(v));
        return set_raw(k, e.ptr(), e.size, e.tag);
    }

    template<typename T>
        requires std::is_floating_point_v<T>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        auto e = detail::encode_double(static_cast<double>(v));
        return set_raw(k, e.ptr(), e.size, e.tag);
    }

    template<typename T>
        requires std::is_same_v<T, std::string>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        auto buf = detail::encode_string(v);
        return set_raw(k, buf.data(), buf.size(), detail::TypeTag::string_type);
    }

    template<typename T>
        requires IsSequentialSerializable<T>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        push_key(k);
        SequentialSerializable<T>::serialize(v, *this);
        (void)set<std::int64_t>("__count__", static_cast<std::int64_t>(SequentialSerializable<T>::size(v)));
        pop_key();
        return Status::ok;
    }

    template<typename T>
        requires IsArbitrarySerializable<T>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        push_key(k);
        ArbitrarySerializable<T>::serialize(v, *this);
        const auto children_keys = ArbitrarySerializable<T>::keys(v);
        std::string joined;
        for (const auto& ck : children_keys) { if (!joined.empty()) joined += '\n'; joined += ck; }
        (void)set<std::string>("__children__", joined);
        pop_key();
        return Status::ok;
    }

    template<typename T>
        requires IsFixedSerializable<T>
    [[nodiscard]] Status set(std::string_view k, const T& v) {
        push_key(k);
        Serializable<T>::serialize(v, *this);
        pop_key();
        return Status::ok;
    }

protected:
    void OnLockAcquired() override;
    void OnLockReleased() override;

private:
    bool committed_{false};
};

// ============================================================================
// BatchReader
// ============================================================================
class BatchReader : public BatchStruct {
public:
    explicit BatchReader(const Store& store, std::string_view key_prefix);
    ~BatchReader() = default;

    BatchReader(const BatchReader&) = delete;
    BatchReader& operator=(const BatchReader&) = delete;
    BatchReader(BatchReader&&) = delete;
    BatchReader& operator=(BatchReader&&) = delete;
    
    [[nodiscard]] std::optional<std::string_view> get_raw(std::string_view relative_key) const;

    // ── get<T> overloads ─────────────────────────────────────────────────────

    template<typename T>
        requires std::is_same_v<T, bool>
    [[nodiscard]] std::optional<bool> get(std::string_view k) const {
        auto raw = get_raw(k);
        if (!raw) return std::nullopt;
        return detail::decode_bool(*raw);
    }

    template<typename T>
        requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    [[nodiscard]] std::optional<T> get(std::string_view k) const {
        auto raw = get_raw(k);
        if (!raw) return std::nullopt;
        return detail::decode_integer<T>(*raw);
    }

    template<typename T>
        requires std::is_floating_point_v<T>
    [[nodiscard]] std::optional<T> get(std::string_view k) const {
        auto raw = get_raw(k);
        if (!raw) return std::nullopt;
        return detail::decode_floating<T>(*raw);
    }

    template<typename T>
        requires std::is_same_v<T, std::string>
    [[nodiscard]] std::optional<std::string> get(std::string_view k) const {
        auto raw = get_raw(k);
        if (!raw) return std::nullopt;
        return detail::decode_string(*raw);
    }

    template<typename T>
        requires IsSequentialSerializable<T>
    [[nodiscard]] std::optional<T> get(std::string_view k) const {
        push_key(k);
        auto result = SequentialSerializable<T>::deserialize(*this);
        pop_key();
        return result;
    }

    template<typename T>
        requires IsArbitrarySerializable<T>
    [[nodiscard]] std::optional<T> get(std::string_view k) const {
        push_key(k);
        auto result = ArbitrarySerializable<T>::deserialize(*this);
        pop_key();
        return result;
    }

    template<typename T>
        requires IsFixedSerializable<T>
    [[nodiscard]] std::optional<T> get(std::string_view k) const {
        push_key(k);
        auto result = Serializable<T>::deserialize(*this);
        pop_key();
        return result;
    }

    // ── Sequential / Arbitrary metadata helpers ──────────────────────────────

    [[nodiscard]] std::optional<std::int64_t> get_count() const {
        return get<std::int64_t>("__count__");
    }

    [[nodiscard]] std::vector<std::string> get_children() const;
};

}  // namespace akasha
