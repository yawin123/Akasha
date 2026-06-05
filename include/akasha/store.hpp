#pragma once

/**
 * @file store.hpp
 * @brief Declares the Store class and DatasetView for hierarchical data storage.
 *
 * The Store class is the main entry point for the Akasha library. It provides:
 * - load/unload: manage memory-mapped files for datasets
 * - set/get<T>: store and retrieve typed values (primitives + custom types)
 * - DatasetView: read-only navigation interface for exploring tree nodes
 * - Memory management: grow, shrink, compact, snapshot operations
 *
 * Template organization:
 * - Primitive types (bool, int64_t, double, std::string): inline definitions here
 * - Custom types (Serializable, Sequential, Arbitrary): in store_serializable.hpp
 *
 * Design principles:
 * - Type-safe: compile-time type checking with concepts
 * - Thread-safe: shared_mutex for read/write coordination
 * - Zero-copy for reads: returns string_view when possible
 * - Single source of truth for encoding/decoding (detail/type_conversion.hpp)
 */

#include "akasha/core.hpp"
#include "akasha/detail/type_conversion.hpp"

#include "akasha/detail/mutex.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace akasha {

/**
 * @brief Hierarchical configuration store with persistent memory-mapped backend.
 *
 * The Store class is the main user-facing entry point for Akasha. It manages:
 *
 * **Dataset lifecycle:** load/unload memory-mapped files, migrate formats
 *
 * **Data operations:** set/get typed values, navigate with DatasetView, has/clear
 *
 * **Memory management:** grow file as needed, shrink/compact to reclaim space,
 * snapshot/rebuild for recovery
 *
 * **Type safety:** primitives (bool, int64_t, double, string) built-in;
 * custom types via Serializable<T>; std::vector<T> automatic
 *
 * **Concurrency:** shared_mutex for readers, exclusive for writers, atomicity
 * per-key or transactional with BatchWriter/BatchReader
 */
class Store {
private:
	struct Source;

public:
	// Forward declaration - inherent implementation detail
	struct MappedFileStorage;

	// ── DatasetView ──────────────────────────────────────────────────────────

	/**
	 * @brief Read-only view of an intermediate node in the tree.
	 *
	 * Allows continuing navigation relatively through get/has.
	 * Example: if get("core.settings") returns DatasetView, then
	 * you can query get("enabled") on that view.
	 */
	class DatasetView {
	public:
		/**
		 * @brief Indicates if the relative path exists within this view.
		 * @param key_path Relative path to the current view.
		 */
		[[nodiscard]] bool has(std::string_view key_path) const;

		/**
		 * @brief Gets a typed value relative to this view.
		 * @param key_path Relative path to the current view.
		 * @return std::optional<T> with the value if it exists, std::nullopt otherwise.
		 * @note The user is responsible for type T. If it does not match the data, behavior is undefined.
		 */
		template<typename T = DatasetView>
		[[nodiscard]] std::optional<T> get(std::string_view key_path = "") const {
			if (source_ == nullptr) {
				return std::nullopt;
			}

			// Build complete key, including dataset as first segment
			std::string full_key = source_->id;
			if (!prefix_.empty()) {
				full_key += '/';
				full_key += prefix_;
			}
			if (!key_path.empty()) {
				full_key += '/';
				full_key += key_path;
			}

			// If T is DatasetView, validate and return a view via Store::get_dataset_view()
			if constexpr (std::is_same_v<T, DatasetView>) {
				if (source_->store == nullptr) {
					return std::nullopt;
				}
				// Delegate to Store::get_dataset_view() which validates the path exists
				return source_->store->get_dataset_view(full_key);
			} else {
			// For other types T, access the Store backend
				if (source_->store == nullptr) {
					return std::nullopt;
				}
				return source_->store->get<T>(full_key);
			}
		}

		/**
		 * @brief Sets a typed value relative to this view.
		 * @param key_path Relative path to the current view.
		 * @param value Value to set.
		 * @return Status::ok on success, or error status.
		 */
		template<typename T>
		[[nodiscard]] Status set(std::string_view key_path, const T& value) {
			if (source_ == nullptr || source_->store == nullptr) {
				return Status::invalid_key_path;
			}

			// Build complete key, including dataset as first segment
			std::string full_key = source_->id;
			if (!prefix_.empty()) {
				full_key += '/';
				full_key += prefix_;
			}
			if (!key_path.empty()) {
				full_key += '/';
				full_key += key_path;
			}

			// Delegate to Store::set<T>()
			return source_->store->set<T>(full_key, value);
		}

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

	private:
		friend class Store;
	
		DatasetView(const Source* source, std::string prefix = {}) noexcept 
			: source_{source}, prefix_{std::move(prefix)} {}
	
		const Source* source_{nullptr};
		std::string prefix_;
	};

	// ── load / unload ────────────────────────────────────────────────────────

	/**
	 * @brief Load configuration from a memory-mapped file.
	 *
	 * Uses Boost.Interprocess managed_mapped_file to store directly
	 * a map<string, value> in the file. Reads return copies of the stored
	 * bytes; the OS handles implicit persistence of modified pages.
	 *
	 * - If the source already exists (same source_id), returns `source_already_loaded` error.
	 *
	 * @param source_id Unique dataset identifier (dataset name).
	 * @param file_path Path to the memory-mapped file.
	 * @param create_if_missing If true and file does not exist, creates empty file.
	 * @return Status of the load operation.
	 */
	[[nodiscard]] Status load(
		std::string_view source_id,
		std::string_view file_path,
		FileOptions options = FileOptions::none
	);

	/**
	 * @brief Unloads a dataset from the Store.
	 *
	 * Closes the memory-mapped file and removes the dataset from the Store.
	 * The data persists on disk, but is no longer accessible through this Store instance.
	 * 
	 * @param source_id Dataset identifier (matches the one used in load()).
	 * @return Status of the unload operation.
	 */
	[[nodiscard]] Status unload(std::string_view source_id);

	// ── set ──────────────────────────────────────────────────────────────────

	/**
	 * @brief Sets or replaces a value at a dataset-qualified key.
	 *
	 * Built-in scalar types (bool, int64_t, double, std::string) have automatic
	 * serialization via encode/decode helpers (memcpy + TypeTag).
	 * Example: set<int64_t>("user.core.timeout", 90)
	 * 
	 * std::vector<T> for scalar T (int64_t, double, bool, string) automatically
	 * serializes as SequentialSerializable with __count__ metadata.
	 * Example: set<std::vector<int>>("sensors.readings", {1, 2, 3})
	 * 
	 * Custom types require Serializable<T> / SequentialSerializable<T> / 
	 * ArbitrarySerializable<T> specialization (user-provided serialize/deserialize).
	 * Example: set<Point>("map/origin", {1.0, 2.0, 3.0}) [with Serializable<Point>]
	 * 
	 * DatasetView specialization copies entire subtree to new location.
	 * Example: set<DatasetView>("backup.servers", view_of_servers)
	 */
	// ── set: DatasetView ──────────────────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, DatasetView>
	[[nodiscard]] Status set(std::string_view key_path, const T& value) {
		return set_datasetview_impl(key_path, value);
	}

	// ── set: bool ────────────────────────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, bool>
	[[nodiscard]] Status set(std::string_view key_path, T value) {
		auto e = detail::encode_bool(value);
		return set_bytes_impl(key_path, e.ptr(), e.size, e.tag);
	}

	// ── set: int64_t (native integer) ────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, std::int64_t>
	[[nodiscard]] Status set(std::string_view key_path, T value) {
		auto e = detail::encode_int64(value);
		return set_bytes_impl(key_path, e.ptr(), e.size, e.tag);
	}

	// ── set: any other integer → convert to int64_t ─────────────────
	template<typename T>
		requires (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, std::int64_t>)
	[[nodiscard]] Status set(std::string_view key_path, T value) {
		return set<std::int64_t>(key_path, static_cast<std::int64_t>(value));
	}

	// ── set: double (native float) ──────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, double>
	[[nodiscard]] Status set(std::string_view key_path, T value) {
		auto e = detail::encode_double(value);
		return set_bytes_impl(key_path, e.ptr(), e.size, e.tag);
	}

	// ── set: any other float → convert to double ────────────────────
	template<typename T>
		requires (std::is_floating_point_v<T> && !std::is_same_v<T, double>)
	[[nodiscard]] Status set(std::string_view key_path, T value) {
		return set<double>(key_path, static_cast<double>(value));
	}

	// ── set: std::string ────────────────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, std::string>
	[[nodiscard]] Status set(std::string_view key_path, const T& value) {
		auto buf = detail::encode_string(value);
		return set_bytes_impl(key_path, buf.data(), buf.size(), detail::TypeTag::string_type);
	}

	// ── set: SequentialSerializable<T> / ArbitrarySerializable<T> / Serializable<T> ──
	// Bodies defined in akasha/detail/store_serializable.hpp (needs BatchWriter).
	template<typename T>
		requires IsSequentialSerializable<T>
	[[nodiscard]] Status set(std::string_view key_path, const T& value);

	template<typename T>
		requires IsArbitrarySerializable<T>
	[[nodiscard]] Status set(std::string_view key_path, const T& value);

	template<typename T>
		requires IsFixedSerializable<T>
	[[nodiscard]] Status set(std::string_view key_path, const T& value);

	/**
	 * @brief Stores a null value at the given key path.
	 * @param key_path Complete path (includes dataset).
	 * @return Status::ok on success, or error status.
	 * 
	 * After set_null(key):
	 * - has(key) returns true
	 * - get<T>(key) returns std::nullopt (for any T)
	 */
	[[nodiscard]] Status set_null(std::string_view key_path);

	/**
	 * @brief Adjust local performance parameters for new grow/creations.
	 */
	void set_performance_tuning(const PerformanceTuning& tuning) noexcept;

	/**
	 * @brief Gets the current performance configuration.
	 */
	[[nodiscard]] PerformanceTuning performance_tuning() const noexcept;

	// ── clear / compact ──────────────────────────────────────────────────────

	/**
	 * @brief Deletes persisted data.
	 *
	 * - If key_path is empty, deletes all data from all loaded datasets.
	 * - If key_path includes only dataset (e.g. "user"), deletes all data in that dataset.
	 * - If key_path includes subkey (e.g. "user.core"), deletes that key and its entire subtree.
	 */
	[[nodiscard]] Status clear(std::string_view key_path = {});

	/**
	 * @brief Compacts the mapped file for a dataset or all of them.
	 *
	 * - If dataset_id is empty, compacts all files in loaded datasets.
	 * - If dataset_id exists, compacts only its associated file.
	 */
	[[nodiscard]] Status compact(std::string_view dataset_id = {});

	// ── has ──────────────────────────────────────────────────────────────────

	/**
	 * @brief Indicates if a complete path exists.
	 * @param key_path Complete path (includes dataset).
	 */
	[[nodiscard]] bool has(std::string_view key_path) const;

	/**
	 * @brief Gets the last status returned by any operation.
	 * @return Status of the last error, or Status::ok if last operation was successful.
	 */
	[[nodiscard]] Status last_status() const noexcept;

	// ── get ──────────────────────────────────────────────────────────────────

	/**
	 * @brief Queries a complete typed path or returns DatasetView.
	 * @param key_path Complete path (includes dataset).
	 * @return std::optional<T> with the value if it exists, std::nullopt otherwise.
	 * @note The user is responsible for the type T. If it does not match the data, behavior is undefined.
	 */
	// ── get: DatasetView (default, no template arg needed) ──────────
	[[nodiscard]] std::optional<DatasetView> get(std::string_view key_path) const {
		return get_dataset_view(key_path);
	}

	// ── get: DatasetView (explicit template) ────────────────────────
	template<typename T>
		requires std::is_same_v<T, DatasetView>
	[[nodiscard]] std::optional<DatasetView> get(std::string_view key_path) const {
		return get_dataset_view(key_path);
	}

	// ── get: bool ───────────────────────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, bool>
	[[nodiscard]] std::optional<bool> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_bool(*view);
	}

	// ── get: int64_t (native integer) ───────────────────────────────
	template<typename T>
		requires std::is_same_v<T, std::int64_t>
	[[nodiscard]] std::optional<std::int64_t> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_integer<std::int64_t>(*view);
	}

	// ── get: any other integer → read int64_t + range check ─────────
	template<typename T>
		requires (std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, std::int64_t>)
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_integer<T>(*view);
	}

	// ── get: double (native float) ──────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, double>
	[[nodiscard]] std::optional<double> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_floating<double>(*view);
	}

	// ── get: any other float → read double + cast ───────────────────
	template<typename T>
		requires (std::is_floating_point_v<T> && !std::is_same_v<T, double>)
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_floating<T>(*view);
	}

	// ── get: std::string ────────────────────────────────────────────
	template<typename T>
		requires std::is_same_v<T, std::string>
	[[nodiscard]] std::optional<std::string> get(std::string_view key_path) const {
		auto view = get_bytes_impl(key_path);
		if (!view.has_value()) return std::nullopt;
		return detail::decode_string(*view);
	}

	// ── get: SequentialSerializable<T> / ArbitrarySerializable<T> / Serializable<T> ──
	// Bodies defined in akasha/detail/store_serializable.hpp (needs BatchReader).
	template<typename T>
		requires IsSequentialSerializable<T>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const;

	template<typename T>
		requires IsArbitrarySerializable<T>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const;

	template<typename T>
		requires IsFixedSerializable<T>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const;

	// ── getorset ─────────────────────────────────────────────────────────────

	/**
	 * @brief Gets a typed value or sets it with a default if it doesn't exist.
	 * 
	 * If the key exists, returns its value. If not, sets the default value,
	 * persists it and returns that same value.
	 * 
	 * Useful for lazy initialization of configuration values.
	 * 
	 * @param key_path Complete path (includes dataset).
	 * @param default_value Default value to set if it doesn't exist.
	 * @return std::optional<T> with the found or default value set.
	 */
	template<typename T>
	[[nodiscard]] std::optional<T> getorset(std::string_view key_path, const T& default_value) {
		auto existing = get<T>(key_path);
		if (existing.has_value()) {
			return existing;
		}
		
		const auto set_status = set<T>(key_path, default_value);
		if (set_status == Status::ok) {
			return default_value;
		}
		
		return std::nullopt;
	}

private:
	struct Source {
		std::string id;
		std::string file_path;
		std::shared_ptr<MappedFileStorage> storage;
		std::shared_ptr<detail::FileLockMutex> file_lock;
		void* dataset_map{nullptr};
		Store* store{nullptr};
	};

	friend class BatchStruct;
	friend class BatchWriter;
	friend class BatchReader;

	[[nodiscard]] Source* find_source(std::string_view source_id);
	[[nodiscard]] const Source* find_source(std::string_view source_id) const;
	[[nodiscard]] std::optional<DatasetView> get_dataset_view(std::string_view key_path) const;
	[[nodiscard]] Status set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size, detail::TypeTag tag);
	[[nodiscard]] Status set_bytes_no_lock(Source*& source, std::string_view dataset_id, std::string_view subkey, const void* bytes, std::size_t size, detail::TypeTag tag);
	[[nodiscard]] Status set_datasetview_impl(std::string_view key_path, const DatasetView& view);
	[[nodiscard]] std::optional<std::string_view> get_bytes_impl(std::string_view key_path) const;
	[[nodiscard]] std::optional<std::string_view> get_bytes_no_lock(const Source* source, std::string_view subkey) const;
	[[nodiscard]] bool flush_source(Source* source) const;
	[[nodiscard]] bool has_key_no_lock(const Source* source, std::string_view subkey) const;
	[[nodiscard]] bool has_subkeys_no_lock(const Source* source, std::string_view subkey_prefix) const;
	[[nodiscard]] std::vector<std::string> get_subkeys_no_lock(const Source* source, std::string_view subkey_prefix) const;
	void clear_no_lock(Source* source, std::string_view subkey);
	[[nodiscard]] std::shared_ptr<detail::FileLockMutex> get_or_create_file_lock(const std::string& file_path) const;
	[[nodiscard]] bool grow_and_remap_sources_for_path(const std::string& file_path, std::size_t grow_by_bytes);
	[[nodiscard]] bool shrink_and_remap_sources_for_path(const std::string& file_path);
	[[nodiscard]] bool compact_and_remap_sources_for_path(const std::string& file_path);
	[[nodiscard]] std::optional<std::vector<std::size_t>> prepare_remap(const std::string& file_path);

	struct SourceSnapshot {
		std::vector<std::pair<std::string, std::string>> entries;
		uint32_t version = 0;
		std::size_t data_size = 0;
	};
	[[nodiscard]] std::optional<SourceSnapshot> extract_source_snapshot(const std::string& file_path) const;
	[[nodiscard]] bool rebuild_file_from_snapshot(const std::string& file_path, const SourceSnapshot& snapshot);
	[[nodiscard]] bool find_and_cleanup_sources_for_path(const std::string& file_path, std::vector<std::size_t>& affected_indexes);
	[[nodiscard]] bool reload_sources_for_path(const std::string& file_path, const std::vector<std::size_t>& affected_indexes, bool use_construct);
	[[nodiscard]] Status migrate(std::shared_ptr<MappedFileStorage>& storage, uint32_t current_version);

	std::vector<Source> sources_;
	mutable detail::FileLockMutex sources_mutex_;
	mutable std::mutex file_locks_mutex_;
	mutable std::unordered_map<std::string, std::shared_ptr<detail::FileLockMutex>> file_locks_;
	std::atomic<std::size_t> initial_mapped_file_size_{64 * 1024};
	std::atomic<std::size_t> initial_grow_step_{(64 * 1024) / 2};
	std::atomic<int> max_grow_retries_{8};
	mutable Status last_status_{Status::ok};
};

}  // namespace akasha
