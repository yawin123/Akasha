#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace akasha {

/**
 * @brief Semantic version of the library.
 * @return Immutable view with the current version (from compilation).
 */
[[nodiscard]] inline std::string_view version() noexcept {
#ifdef AKASHA_VERSION
	return AKASHA_VERSION;
#else
	return "0.0.0-dev";  // fallback if not compiled with CMake
#endif
}

/**
 * @brief Hierarchical path in dot notation.
 *
 * Format rules for loadable keys:
 * - Minimum 1 segment: "dataset" (root value of dataset) or "dataset.something" (nested value).
 * - The first segment identifies the dataset.
 * - The rest identifies the key within the dataset.
 * - If there is only 1 segment, it is treated as the root value of the entire dataset.
 *
 * Valid examples: "config" (root), "core.timeout", "core.settings.retries".
 */
using KeyPath = std::string;

/** @brief Result of any operation (load, write, etc). */
enum class Status {
	ok,
	invalid_key_path,      // Empty key or invalid format
	key_conflict,          // Conflict in hierarchical structure
	file_read_error,       // Error reading memory-mapped file
	file_write_error,      // Error writing to file
	file_not_found,        // File does not exist and create_if_missing=false
	file_full,             // File full after growth retry attempts
	parse_error,           // Error parsing internal data
	dataset_not_found,     // Dataset (first segment) does not exist
	source_already_loaded, // Dataset already loaded
};

struct PerformanceTuning {
	std::size_t initial_mapped_file_size{64 * 1024};
	std::size_t initial_grow_step{(64 * 1024) / 2};
	int max_grow_retries{8};
};

/**
	 * @brief Hierarchical configuration store.
 *
 * Store loads complete paths (including dataset) and allows:
 * - Query if a path exists with has.
 * - Get a leaf value or a navigable subset with get.
 */
class Store {
private:
	struct MappedFileStorage;
	struct Source;

public:
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
				full_key += '.';
				full_key += prefix_;
			}
			if (!key_path.empty()) {
				full_key += '.';
				full_key += key_path;
			}

			// If T is DatasetView, return a view
			if constexpr (std::is_same_v<T, DatasetView>) {
				return DatasetView(source_, full_key);
			} else {
			// For other types T, access the Store backend
				if (source_->store == nullptr) {
					return std::nullopt;
				}
				return source_->store->get<T>(full_key);
			}
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
		bool create_if_missing = false
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

	/**
	 * @brief Sets or replaces a value at a dataset-qualified key.
	 *
	 * Specialization for trivially copyable types:
	 * Example: set<int64_t>("user.core.timeout", 90)
	 * 
	 * Specialization for DatasetView:
	 * Copy the entire subtree to the new location, deleting destination if it exists.
	 * Example: set<DatasetView>("backup.servers", view_of_servers)
	 */
	template<typename T>
	[[nodiscard]] Status set(std::string_view key_path, const T& value) {
		if constexpr (std::is_same_v<T, DatasetView>) {
			// Special override for DatasetView: copy subtree
			return set_datasetview_impl(key_path, value);
		} else {
			static_assert(std::is_trivially_copyable_v<T>, 
				"Type T must be trivially copyable for akasha::Store::set<T>");
			
			const char* bytes = reinterpret_cast<const char*>(&value);
			return set_bytes_impl(key_path, bytes, sizeof(T));
		}
	}

	/**
	 * @brief Adjust local performance parameters for new grow/creations.
	 */
	void set_performance_tuning(const PerformanceTuning& tuning) noexcept;

	/**
	 * @brief Gets the current performance configuration.
	 */
	[[nodiscard]] PerformanceTuning performance_tuning() const noexcept;

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

	/**
	 * @brief Queries a complete typed path or returns DatasetView.
	 * @param key_path Complete path (includes dataset).
	 * @return std::optional<T> with the value if it exists, std::nullopt otherwise.
	 * @note The user is responsible for the type T. If it does not match the data, behavior is undefined.
	 */
	template<typename T = DatasetView>
	[[nodiscard]] std::optional<T> get(std::string_view key_path) const {
		// If T is DatasetView, return a view
		if constexpr (std::is_same_v<T, DatasetView>) {
			return get_dataset_view(key_path);
		} else {
			static_assert(std::is_trivially_copyable_v<T>, 
				"Type T must be trivially copyable for akasha::Store::get<T>");
			
			// Read generic bytes and interpret as T
			auto bytes = get_bytes_impl(key_path, sizeof(T));
			if (!bytes.has_value()) {
				return std::nullopt;
			}
			
			// Reinterpret bytes as T
			return *reinterpret_cast<const T*>(bytes->data());
		}
	}

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
		// Try to get the existing value
		auto existing = get<T>(key_path);
		if (existing.has_value()) {
			return existing;
		}
		
		// If not exists, write the default
		const auto set_status = set<T>(key_path, default_value);
		if (set_status == Status::ok) {
			return default_value;
		}
		
		// Error writing
		return std::nullopt;
	}

private:
	struct Source {
		std::string id;
		std::string file_path;
		std::shared_ptr<MappedFileStorage> storage;
		std::shared_ptr<std::shared_mutex> file_lock;
		void* dataset_map{nullptr};
		Store* store{nullptr};
	};

	[[nodiscard]] static bool split_key_path(std::string_view key_path, std::vector<std::string_view>& segments);
	[[nodiscard]] Source* find_source(std::string_view source_id);
	[[nodiscard]] const Source* find_source(std::string_view source_id) const;
	[[nodiscard]] std::optional<DatasetView> get_dataset_view(std::string_view key_path) const;
	[[nodiscard]] Status set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size);
	[[nodiscard]] Status set_datasetview_impl(std::string_view key_path, const DatasetView& view);
	[[nodiscard]] std::optional<std::vector<char>> get_bytes_impl(std::string_view key_path, std::size_t expected_size) const;
	[[nodiscard]] std::shared_ptr<std::shared_mutex> get_or_create_file_lock(const std::string& file_path) const;
	[[nodiscard]] bool grow_and_remap_sources_for_path(const std::string& file_path, std::size_t grow_by_bytes);
	[[nodiscard]] bool shrink_and_remap_sources_for_path(const std::string& file_path);
	[[nodiscard]] bool compact_and_remap_sources_for_path(const std::string& file_path);

	std::vector<Source> sources_;
	mutable std::shared_mutex sources_mutex_;
	mutable std::mutex file_locks_mutex_;
	mutable std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> file_locks_;
	std::atomic<std::size_t> initial_mapped_file_size_{64 * 1024};
	std::atomic<std::size_t> initial_grow_step_{(64 * 1024) / 2};
	std::atomic<int> max_grow_retries_{8};
	mutable Status last_status_{Status::ok};
};

// Template specializations for std::string outside the class scope

/**
 * @brief Specialization for Store::set() with std::string.
 * Serializes the string as [size_t length][char data].
 */
template<>
inline akasha::Status Store::set<std::string>(std::string_view key_path, const std::string& value) {
	std::vector<char> buffer;
	std::size_t len = value.size();
	
	const char* len_bytes = reinterpret_cast<const char*>(&len);
	buffer.insert(buffer.end(), len_bytes, len_bytes + sizeof(std::size_t));
	buffer.insert(buffer.end(), value.begin(), value.end());
	
	return set_bytes_impl(key_path, buffer.data(), buffer.size());
}

/**
 * @brief Specialization for Store::get() with std::string.
 * Deserializes the string from [size_t length][char data].
 */
template<>
inline std::optional<std::string> Store::get<std::string>(std::string_view key_path) const {
	// We need at least sizeof(size_t) for the length
	auto bytes = get_bytes_impl(key_path, sizeof(std::size_t));
	if (!bytes.has_value() || bytes->size() < sizeof(std::size_t)) {
		return std::nullopt;
	}
	
	// Read the length
	std::size_t len = *reinterpret_cast<const std::size_t*>(bytes->data());
	
	// Verify that the size is consistent
	if (bytes->size() != sizeof(std::size_t) + len) {
		return std::nullopt;
	}
	
	// Rebuild the string from the bytes
	if (len == 0) {
		return std::string();
	}
	
	return std::string(bytes->data() + sizeof(std::size_t), bytes->data() + bytes->size());
}

/**
 * @brief Specialization for Store::getorset() with std::string.
 * Gets the string or sets the default if it doesn't exist.
 */
template<>
inline std::optional<std::string> Store::getorset<std::string>(std::string_view key_path, const std::string& default_value) {
	// Try to get the existing value
	auto existing = get<std::string>(key_path);
	if (existing.has_value()) {
		return existing;
	}
	
	// If not exists, write the default
	const auto set_status = set<std::string>(key_path, default_value);
	if (set_status == Status::ok) {
		return default_value;
	}
	
	// Error writing
	return std::nullopt;
}

}  // namespace akasha
