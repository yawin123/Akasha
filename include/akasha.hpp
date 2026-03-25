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

// Helper: detect if T is std::vector<U> where U is trivially copyable (but not bool, handled separately)
template<typename T>
struct is_trivially_copyable_vector : std::false_type {};

template<typename T>
struct is_trivially_copyable_vector<std::vector<T>> 
	: std::bool_constant<std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>> {};

template<typename T>
static constexpr bool is_trivially_copyable_vector_v = is_trivially_copyable_vector<T>::value;

/** @brief Result of any operation (load, write, etc). */
enum class Status {
	ok,
	invalid_key_path,      // Empty key or invalid format
	invalid_file_path,	   // File path is invalid (e.g. directory, invalid characters)
	key_conflict,          // Conflict in hierarchical structure
	file_read_error,       // Error reading memory-mapped file
	file_write_error,      // Error writing to file
	file_not_found,        // File does not exist and create_if_missing=false
	file_full,             // File full after growth retry attempts
	parse_error,           // Error parsing internal data
	dataset_not_found,     // Dataset (first segment) does not exist
	key_not_found,         // Key (full path) does not exist
	source_already_loaded, // Dataset already loaded
	incompatible_format,   // File format version incompatible with this library version
};

/** @brief Options for file operations. */
enum class FileOptions {
	none = 0,
	create_if_missing = 1, // Create the file if it does not exist (only for load)
	migrate_if_incompatible = 2, // If format version is incompatible, attempt to migrate (not implemented yet)
};

// Bitwise operators for FileOptions enum class
inline FileOptions operator|(FileOptions lhs, FileOptions rhs) {
	return static_cast<FileOptions>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

inline FileOptions operator&(FileOptions lhs, FileOptions rhs) {
	return static_cast<FileOptions>(static_cast<int>(lhs) & static_cast<int>(rhs));
}

inline FileOptions operator~(FileOptions x) {
	return static_cast<FileOptions>(~static_cast<int>(x));
}

inline FileOptions& operator|=(FileOptions& lhs, FileOptions rhs) {
	lhs = lhs | rhs;
	return lhs;
}

inline FileOptions& operator&=(FileOptions& lhs, FileOptions rhs) {
	lhs = lhs & rhs;
	return lhs;
}

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
	struct Source;

public:
	// Forward declaration - inherent implementation detail
	struct MappedFileStorage;

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

	/**
	 * @brief Sets or replaces a value at a dataset-qualified key.
	 *
	 * Specialization for trivially copyable types:
	 * Example: set<int64_t>("user.core.timeout", 90)
	 * 
	 * Specialization for std::vector<T> where T is trivially copyable:
	 * Example: set<std::vector<int>>("sensors.readings", {1, 2, 3})
	 * 
	 * Specialization for std::vector<bool> (serialized as byte array):
	 * Example: set<std::vector<bool>>("flags.state", {true, false, true})
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
		} else if constexpr (std::is_same_v<T, std::vector<bool>>) {
			// Specialization for std::vector<bool> (special case, no .data() method)
			// Format: [size_t count][uint8_t elem0][uint8_t elem1]...[uint8_t elemN]
			// Each bool serialized as one byte (0 or 1)
			std::vector<char> buffer;
			std::size_t count = value.size();
			
			// Serialize count
			const char* count_bytes = reinterpret_cast<const char*>(&count);
			buffer.insert(buffer.end(), count_bytes, count_bytes + sizeof(std::size_t));
			
			// Serialize each bool as one byte
			for (bool elem : value) {
				buffer.push_back(static_cast<char>(elem ? 1 : 0));
			}
			
			return set_bytes_impl(key_path, buffer.data(), buffer.size());
		} else if constexpr (is_trivially_copyable_vector_v<T>) {
			// Specialization for std::vector<U> where U is trivially copyable
			// Format: [size_t count][U elem0][U elem1]...[U elemN]
			using elem_type = typename T::value_type;
			std::vector<char> buffer;
			std::size_t count = value.size();
			
			// Serialize count
			const char* count_bytes = reinterpret_cast<const char*>(&count);
			buffer.insert(buffer.end(), count_bytes, count_bytes + sizeof(std::size_t));
			
			// Serialize elements with memcpy
			if (count > 0) {
				const char* elem_bytes = reinterpret_cast<const char*>(value.data());
				buffer.insert(buffer.end(), elem_bytes, elem_bytes + count * sizeof(elem_type));
			}
			
			return set_bytes_impl(key_path, buffer.data(), buffer.size());
		} else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
			// Specialization for std::vector<std::string>
			// Format: [size_t count][size_t len0][chars0...][size_t len1][chars1...]...
			std::vector<char> buffer;
			std::size_t count = value.size();
			
			// Serialize count
			const char* count_bytes = reinterpret_cast<const char*>(&count);
			buffer.insert(buffer.end(), count_bytes, count_bytes + sizeof(std::size_t));
			
			// Serialize each string
			for (const auto& str : value) {
				std::size_t len = str.size();
				const char* len_bytes = reinterpret_cast<const char*>(&len);
				buffer.insert(buffer.end(), len_bytes, len_bytes + sizeof(std::size_t));
				buffer.insert(buffer.end(), str.begin(), str.end());
			}
			
			return set_bytes_impl(key_path, buffer.data(), buffer.size());
		} else {
			static_assert(std::is_trivially_copyable_v<T>, 
				"Type T must be trivially copyable (or std::vector<U> with trivially copyable U) for akasha::Store::set<T>");
			
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
		} else if constexpr (std::is_same_v<T, std::vector<bool>>) {
			// Specialization for std::vector<bool> (special case, stored as byte array)
			// Format: [size_t count][uint8_t elem0][uint8_t elem1]...[uint8_t elemN]
			auto view = get_bytes_impl(key_path);
			if (!view.has_value() || view->size() < sizeof(std::size_t)) {
				return std::nullopt;
			}
			
			// Read the count
			std::size_t count = *reinterpret_cast<const std::size_t*>(view->data());
			
			// Verify that the size is consistent
			if (view->size() != sizeof(std::size_t) + count) {
				return std::nullopt;
			}
			
			if (count == 0) {
				return std::vector<bool>();
			}
			
			// Deserialize elements: each byte becomes a bool
			std::vector<bool> result;
			const char* data = view->data() + sizeof(std::size_t);
			for (std::size_t i = 0; i < count; ++i) {
				result.push_back(data[i] != 0);
			}
			return result;
		} else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
			// Specialization for std::vector<std::string>
			// Format: [size_t count][size_t len0][chars0...][size_t len1][chars1...]...
			auto view = get_bytes_impl(key_path);
			if (!view.has_value() || view->size() < sizeof(std::size_t)) {
				return std::nullopt;
			}
			
			// Read the count
			std::size_t count = *reinterpret_cast<const std::size_t*>(view->data());
			
			if (count == 0) {
				return std::vector<std::string>();
			}
			
			// Deserialize each string
			std::vector<std::string> result;
			std::size_t offset = sizeof(std::size_t);
			const char* data = view->data();
			
			for (std::size_t i = 0; i < count; ++i) {
				// Check if we can read the length
				if (offset + sizeof(std::size_t) > view->size()) {
					return std::nullopt;
				}
				
				// Read string length
				std::size_t len = *reinterpret_cast<const std::size_t*>(data + offset);
				offset += sizeof(std::size_t);
				
				// Check if we can read the string data
				if (offset + len > view->size()) {
					return std::nullopt;
				}
				
				// Create string from bytes
				result.push_back(std::string(data + offset, data + offset + len));
				offset += len;
			}
			
			return result;
		} else if constexpr (is_trivially_copyable_vector_v<T>) {
			// Specialization for std::vector<U> where U is trivially copyable
			// Format: [size_t count][U elem0][U elem1]...[U elemN]
			using elem_type = typename T::value_type;
			auto view = get_bytes_impl(key_path);
			if (!view.has_value() || view->size() < sizeof(std::size_t)) {
				return std::nullopt;
			}
			
			// Read the count
			std::size_t count = *reinterpret_cast<const std::size_t*>(view->data());
			
			// Verify that the size is consistent
			if (view->size() != sizeof(std::size_t) + count * sizeof(elem_type)) {
				return std::nullopt;
			}
			
			if (count == 0) {
				return T();
			}
			
			// Deserialize elements: create vector from raw bytes
			const elem_type* elem_ptr = reinterpret_cast<const elem_type*>(view->data() + sizeof(std::size_t));
			return T(elem_ptr, elem_ptr + count);
		} else {
			static_assert(std::is_trivially_copyable_v<T>, 
				"Type T must be trivially copyable (or std::vector<U> with trivially copyable U) for akasha::Store::get<T>");
			
			// Use view-based read to avoid unnecessary copy from mmap
			auto view = get_bytes_impl(key_path);
			if (!view.has_value() || view->size() != sizeof(T)) {
				return std::nullopt;
			}
			
			// Reinterpret bytes as T (no copy)
			return *reinterpret_cast<const T*>(view->data());
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

	[[nodiscard]] Source* find_source(std::string_view source_id);
	[[nodiscard]] const Source* find_source(std::string_view source_id) const;
	[[nodiscard]] std::optional<DatasetView> get_dataset_view(std::string_view key_path) const;
	[[nodiscard]] Status set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size);
	[[nodiscard]] Status set_datasetview_impl(std::string_view key_path, const DatasetView& view);
	[[nodiscard]] std::optional<std::string_view> get_bytes_impl(std::string_view key_path) const;
	[[nodiscard]] std::shared_ptr<std::shared_mutex> get_or_create_file_lock(const std::string& file_path) const;
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
	auto view = get_bytes_impl(key_path);
	if (!view.has_value() || view->size() < sizeof(std::size_t)) {
		return std::nullopt;
	}
	
	// Read the length
	std::size_t len = *reinterpret_cast<const std::size_t*>(view->data());
	
	// Verify that the size is consistent
	if (view->size() != sizeof(std::size_t) + len) {
		return std::nullopt;
	}
	
	if (len == 0) {
		return std::string();
	}
	
	return std::string(view->data() + sizeof(std::size_t), view->data() + view->size());
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
