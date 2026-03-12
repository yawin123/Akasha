#include "akasha.hpp"

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

namespace bip = boost::interprocess;

using InternalValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;
using SegmentManager = bip::managed_mapped_file::segment_manager;
using CharAllocator = bip::allocator<char, SegmentManager>;
using InterprocessString = bip::basic_string<char, std::char_traits<char>, CharAllocator>;
using InterprocessValue = InterprocessString;

struct InterprocessStringLess {
    bool operator()(const InterprocessString& a, const InterprocessString& b) const {
        return std::string_view(a.c_str(), a.size()) < std::string_view(b.c_str(), b.size());
    }
};

using MapAllocator = bip::allocator<std::pair<const InterprocessString, InterprocessValue>, SegmentManager>;
using InterprocessDatasetMap = bip::map<InterprocessString, InterprocessValue, InterprocessStringLess, MapAllocator>;

constexpr const char* kDatasetMapName = "akasha_root";
constexpr std::size_t kDefaultInitialMappedFileSize = 64 * 1024;
constexpr std::size_t kDefaultInitialGrowStep = kDefaultInitialMappedFileSize / 2;
constexpr int kDefaultMaxGrowRetries = 8;

}  // namespace

namespace akasha {

struct Store::MappedFileStorage {
    explicit MappedFileStorage(const std::string& path, std::size_t initial_size)
        : file(bip::open_or_create, path.c_str(), initial_size) {
    }

    bip::managed_mapped_file file;
};

inline InterprocessDatasetMap* as_dataset_map(void* ptr) {
    return static_cast<InterprocessDatasetMap*>(ptr);
}

inline const InterprocessDatasetMap* as_dataset_map(const void* ptr) {
    return static_cast<const InterprocessDatasetMap*>(ptr);
}

bool Store::split_key_path(std::string_view key_path, std::vector<std::string_view>& segments) {
    segments.clear();
    if (key_path.empty()) {
        return false;
    }

    std::size_t start = 0;
    while (start < key_path.size()) {
        const std::size_t dot = key_path.find('.', start);
        const std::size_t end = dot == std::string_view::npos ? key_path.size() : dot;
        if (end == start) {
            return false;
        }

        segments.emplace_back(key_path.substr(start, end - start));
        if (dot == std::string_view::npos) {
            return true;
        }
        start = dot + 1;
    }

    return false;
}

void Store::set_performance_tuning(const PerformanceTuning& tuning) noexcept {
    const std::size_t next_initial_size = tuning.initial_mapped_file_size == 0
        ? kDefaultInitialMappedFileSize
        : tuning.initial_mapped_file_size;

    const std::size_t next_grow_step = tuning.initial_grow_step == 0
        ? kDefaultInitialGrowStep
        : tuning.initial_grow_step;

    const int next_max_retries = tuning.max_grow_retries <= 0
        ? kDefaultMaxGrowRetries
        : tuning.max_grow_retries;

    initial_mapped_file_size_.store(next_initial_size, std::memory_order_relaxed);
    initial_grow_step_.store(next_grow_step, std::memory_order_relaxed);
    max_grow_retries_.store(next_max_retries, std::memory_order_relaxed);
}

PerformanceTuning Store::performance_tuning() const noexcept {
    PerformanceTuning tuning;
    tuning.initial_mapped_file_size = initial_mapped_file_size_.load(std::memory_order_relaxed);
    tuning.initial_grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    tuning.max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);
    return tuning;
}

Store::Source* Store::find_source(std::string_view source_id) {
    auto it = std::find_if(sources_.begin(), sources_.end(), [source_id](const Source& source) {
        return source.id == source_id;
    });

    if (it == sources_.end()) {
        return nullptr;
    }

    return &(*it);
}

const Store::Source* Store::find_source(std::string_view source_id) const {
    auto it = std::find_if(sources_.begin(), sources_.end(), [source_id](const Source& source) {
        return source.id == source_id;
    });

    if (it == sources_.end()) {
        return nullptr;
    }

    return &(*it);
}

std::shared_ptr<std::shared_mutex> Store::get_or_create_file_lock(const std::string& file_path) const {
    std::lock_guard<std::mutex> guard(file_locks_mutex_);

    const auto it = file_locks_.find(file_path);
    if (it != file_locks_.end()) {
        return it->second;
    }

    auto file_lock = std::make_shared<std::shared_mutex>();
    file_locks_.emplace(file_path, file_lock);
    return file_lock;
}

bool Store::grow_and_remap_sources_for_path(const std::string& file_path, std::size_t grow_by_bytes) {
    if (file_path.empty() || grow_by_bytes == 0) {
        return false;
    }

    std::vector<std::size_t> affected_indexes;
    affected_indexes.reserve(sources_.size());
    for (std::size_t index = 0; index < sources_.size(); ++index) {
        if (sources_[index].file_path == file_path) {
            affected_indexes.push_back(index);
        }
    }

    if (affected_indexes.empty()) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        sources_[index].dataset_map = nullptr;
        sources_[index].storage.reset();
    }

    try {
        bip::managed_mapped_file::grow(file_path.c_str(), grow_by_bytes);
    } catch (...) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        try {
            auto storage = std::make_shared<MappedFileStorage>(
                file_path,
                initial_mapped_file_size_.load(std::memory_order_relaxed)
            );
            const auto found = storage->file.find<InterprocessDatasetMap>(kDatasetMapName);
            if (found.first == nullptr) {
                return false;
            }

            sources_[index].storage = std::move(storage);
            sources_[index].dataset_map = found.first;
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool Store::shrink_and_remap_sources_for_path(const std::string& file_path) {
    if (file_path.empty()) {
        return false;
    }

    std::vector<std::size_t> affected_indexes;
    affected_indexes.reserve(sources_.size());
    for (std::size_t index = 0; index < sources_.size(); ++index) {
        if (sources_[index].file_path == file_path) {
            affected_indexes.push_back(index);
        }
    }

    if (affected_indexes.empty()) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        sources_[index].dataset_map = nullptr;
        sources_[index].storage.reset();
    }

    try {
        (void)bip::file_mapping::remove(file_path.c_str());
    } catch (...) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        try {
            auto storage = std::make_shared<MappedFileStorage>(
                file_path,
                initial_mapped_file_size_.load(std::memory_order_relaxed)
            );
            auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
                kDatasetMapName
            )(storage->file.get_segment_manager());

            if (dataset_map == nullptr) {
                return false;
            }

            sources_[index].storage = std::move(storage);
            sources_[index].dataset_map = dataset_map;
        } catch (...) {
            return false;
        }
    }

    return true;
}

bool Store::compact_and_remap_sources_for_path(const std::string& file_path) {
    if (file_path.empty()) {
        return false;
    }

    std::vector<std::size_t> affected_indexes;
    affected_indexes.reserve(sources_.size());
    for (std::size_t index = 0; index < sources_.size(); ++index) {
        if (sources_[index].file_path == file_path) {
            affected_indexes.push_back(index);
        }
    }

    if (affected_indexes.empty()) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        sources_[index].dataset_map = nullptr;
        sources_[index].storage.reset();
    }

    try {
        if (!bip::managed_mapped_file::shrink_to_fit(file_path.c_str())) {
            return false;
        }
    } catch (...) {
        return false;
    }

    for (const std::size_t index : affected_indexes) {
        try {
            auto storage = std::make_shared<MappedFileStorage>(
                file_path,
                initial_mapped_file_size_.load(std::memory_order_relaxed)
            );
            const auto found = storage->file.find<InterprocessDatasetMap>(kDatasetMapName);
            if (found.first == nullptr) {
                return false;
            }

            sources_[index].storage = std::move(storage);
            sources_[index].dataset_map = found.first;
        } catch (...) {
            return false;
        }
    }

    return true;
}

Status Store::load(std::string_view source_id, std::string_view file_path, bool create_if_missing) {
    if (source_id.empty()) {
        return last_status_ = Status::invalid_key_path;
    }

    if (file_path.empty()) {
        return last_status_ = Status::file_read_error;
    }

    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    // Verify source does not already exist
    if (find_source(source_id) != nullptr) {
        return last_status_ = Status::source_already_loaded;
    }

    const std::string path{file_path};
    const std::size_t initial_size = initial_mapped_file_size_.load(std::memory_order_relaxed);

    const auto file_lock = get_or_create_file_lock(path);
    std::unique_lock<std::shared_mutex> write_guard(*file_lock);

    try {
        auto storage = std::make_shared<MappedFileStorage>(path, initial_size);

        // Try to find or create the dataset map in the managed_mapped_file
        auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
            kDatasetMapName
        )(storage->file.get_segment_manager());

        if (!dataset_map) {
            return last_status_ = Status::file_read_error;
        }

        // Create and store the Source
        Source new_source;
        new_source.id = std::string{source_id};
        new_source.file_path = path;
        new_source.storage = storage;
        new_source.file_lock = file_lock;
        new_source.dataset_map = dataset_map;
        new_source.store = this;

        sources_.push_back(std::move(new_source));
        return last_status_ = Status::ok;

    } catch (const bip::interprocess_exception& e) {
        if (create_if_missing) {
            // Try to create new file
            try {
                bip::file_mapping::remove(path.c_str());
                auto storage = std::make_shared<MappedFileStorage>(path, initial_size);

                auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
                    kDatasetMapName
                )(storage->file.get_segment_manager());

                if (!dataset_map) {
                    return last_status_ = Status::file_read_error;
                }

                Source new_source;
                new_source.id = std::string{source_id};
                new_source.file_path = path;
                new_source.storage = storage;
                new_source.file_lock = file_lock;
                new_source.dataset_map = dataset_map;
                new_source.store = this;

                sources_.push_back(std::move(new_source));
                return last_status_ = Status::ok;
            } catch (...) {
                return last_status_ = Status::file_read_error;
            }
        }
        return last_status_ = Status::file_read_error;
    }
}

Status Store::unload(std::string_view source_id) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    auto source_it = std::find_if(sources_.begin(), sources_.end(), 
        [source_id](const Source& source) {
            return source.id == source_id;
        });

    if (source_it == sources_.end()) {
        return last_status_ = Status::dataset_not_found;
    }

    // Acquire lock on the file being unloaded to ensure no operations in progress
    if (source_it->file_lock) {
        std::unique_lock<std::shared_mutex> file_guard(*source_it->file_lock);
        
        // Clear the dataset map reference
        source_it->dataset_map = nullptr;
        source_it->storage = nullptr;
        source_it->file_lock = nullptr;
    }

    // Remove the source from the vector
    sources_.erase(source_it);
    
    return last_status_ = Status::ok;
}

Status Store::set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size) {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return last_status_ = Status::invalid_key_path;
    }

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dataset_id = segments.front();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    if (!source->file_lock) {
        return last_status_ = Status::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        return last_status_ = Status::file_write_error;
    }

    // Build the key without the dataset prefix
    // If there is only 1 segment (dataset root), use "__root__"
    std::string key;
    if (segments.size() == 1) {
        key = "__root__";
    } else {
        for (std::size_t i = 1; i < segments.size(); ++i) {
            if (i > 1) {
                key += '.';
            }
            key += segments[i];
        }
    }

    std::size_t grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    const int max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);

    for (int attempt = 0; attempt <= max_grow_retries; ++attempt) {
        try {
            auto* segment_mgr = source->storage->file.get_segment_manager();
            auto* map = as_dataset_map(source->dataset_map);
            auto allocator = segment_mgr->get_allocator<char>();

            // Create InterprocessString for the key
            InterprocessString ipc_key(key.c_str(), allocator);

            // Create InterprocessValue (InterprocessString) with the bytes
            InterprocessValue ipc_value(static_cast<const char*>(bytes), size, allocator);

            // Insert or replace in the map (direct mmap write)
            auto it = map->find(ipc_key);
            if (it == map->end()) {
                map->emplace(ipc_key, ipc_value);
            } else {
                it->second = ipc_value;
            }

            if (!source->storage->file.flush()) {
                return last_status_ = Status::file_write_error;
            }

            return last_status_ = Status::ok;
        } catch (const bip::interprocess_exception&) {
            if (attempt == max_grow_retries) {
                return last_status_ = Status::file_write_error;
            }

            const std::string source_file_path = source->file_path;
            const std::size_t current_file_size = source->storage->file.get_size();
            const std::size_t base_step = initial_grow_step_.load(std::memory_order_relaxed);
            const std::size_t dynamic_step = std::max(grow_step, std::max(base_step, current_file_size / 2));

            if (!grow_and_remap_sources_for_path(source_file_path, dynamic_step)) {
                return last_status_ = Status::file_write_error;
            }

            source = find_source(dataset_id);
            if (source == nullptr || !source->storage || !source->dataset_map) {
                return last_status_ = Status::file_write_error;
            }

            grow_step = dynamic_step * 2;
        }
    }

    return last_status_ = Status::file_write_error;
}

std::optional<std::vector<char>> Store::get_bytes_impl(std::string_view key_path, std::size_t expected_size) const {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dataset_id = segments.front();
    auto source_it = std::find_if(sources_.begin(), sources_.end(), [dataset_id](const Source& source) {
        return source.id == dataset_id;
    });

    if (source_it == sources_.end()) {
        return std::nullopt;
    }

    const Source& source = *source_it;
    if (!source.file_lock) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source.file_lock);

    if (!source.dataset_map || !source.storage) {
        return std::nullopt;
    }

    // Build the key
    // If there is only 1 segment (dataset root), use "__root__"
    std::string key;
    if (segments.size() == 1) {
        key = "__root__";
    } else {
        for (std::size_t i = 1; i < segments.size(); ++i) {
            if (i > 1) {
                key += '.';
            }
            key += segments[i];
        }
    }

    // Buscar clave exacta
    auto* map = as_dataset_map(source.dataset_map);
    auto it = map->find(InterprocessString(key.c_str(), source.storage->file.get_segment_manager()->get_allocator<char>()));
    if (it != map->end()) {
        // it->second is the InterprocessValue (InterprocessString)
        const auto& data_bytes = it->second;
        
        // Return the bytes (size verification is the user's responsibility)
        std::vector<char> result(data_bytes.c_str(), data_bytes.c_str() + data_bytes.size());
        return result;
    }

    return std::nullopt;
}

Status Store::clear(std::string_view key_path) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    if (key_path.empty()) {
        std::unordered_set<std::string> processed_paths;

        for (const Source& source : sources_) {
            if (!source.storage || !source.dataset_map || !source.file_lock) {
                return last_status_ = Status::file_write_error;
            }

            if (!processed_paths.insert(source.file_path).second) {
                continue;
            }

            std::unique_lock<std::shared_mutex> write_guard(*source.file_lock);
            auto* map = as_dataset_map(source.dataset_map);
            map->clear();

            if (!source.storage->file.flush()) {
                return last_status_ = Status::file_write_error;
            }

            if (!shrink_and_remap_sources_for_path(source.file_path)) {
                return last_status_ = Status::file_write_error;
            }
        }

        return last_status_ = Status::ok;
    }

    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return last_status_ = Status::invalid_key_path;
    }

    const std::string_view dataset_id = segments.front();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    if (!source->storage || !source->dataset_map || !source->file_lock) {
        return last_status_ = Status::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);
    auto* map = as_dataset_map(source->dataset_map);

    if (segments.size() == 1) {
        // If it's just the dataset, delete EVERYTHING (includes __root__ and any other key)
        map->clear();
        if (!source->storage->file.flush()) {
            return last_status_ = Status::file_write_error;
        }

        if (!shrink_and_remap_sources_for_path(source->file_path)) {
            return last_status_ = Status::file_write_error;
        }

        return last_status_ = Status::ok;
    }

    // If it's a subkey, build the prefix normally
    std::string prefix;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            prefix += '.';
        }
        prefix += segments[i];
    }

    const std::string prefix_with_dot = prefix + ".";

    for (auto it = map->begin(); it != map->end();) {
        const std::string_view current_key(it->first.c_str(), it->first.size());
        if (current_key == prefix || current_key.starts_with(prefix_with_dot)) {
            it = map->erase(it);
        } else {
            ++it;
        }
    }

    if (!source->storage->file.flush()) {
        return last_status_ = Status::file_write_error;
    }

    if (map->empty()) {
        if (!shrink_and_remap_sources_for_path(source->file_path)) {
            return last_status_ = Status::file_write_error;
        }
    }

    return last_status_ = Status::ok;
}

Status Store::compact(std::string_view dataset_id) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    if (dataset_id.empty()) {
        std::unordered_set<std::string> processed_paths;

        for (const Source& source : sources_) {
            if (!source.file_lock) {
                return last_status_ = Status::file_write_error;
            }

            if (!processed_paths.insert(source.file_path).second) {
                continue;
            }

            std::unique_lock<std::shared_mutex> write_guard(*source.file_lock);
            if (!compact_and_remap_sources_for_path(source.file_path)) {
                return last_status_ = Status::file_write_error;
            }
        }

        return last_status_ = Status::ok;
    }

    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    if (!source->file_lock) {
        return last_status_ = Status::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);
    if (!compact_and_remap_sources_for_path(source->file_path)) {
        return last_status_ = Status::file_write_error;
    }

    return last_status_ = Status::ok;
}

bool Store::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

Status Store::set_datasetview_impl(std::string_view key_path, const DatasetView& view) {
    // Verify the DatasetView has a source
    if (view.source_ == nullptr) {
        return last_status_ = Status::invalid_key_path;
    }

    // Validate and parse destination key_path
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return last_status_ = Status::invalid_key_path;
    }

    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dest_dataset_id = segments.front();
    auto dest_source_it = std::find_if(sources_.begin(), sources_.end(), [dest_dataset_id](const Source& source) {
        return source.id == dest_dataset_id;
    });

    if (dest_source_it == sources_.end()) {
        return last_status_ = Status::dataset_not_found;
    }

    Source& dest_source = *dest_source_it;
    if (!dest_source.file_lock) {
        return last_status_ = Status::file_not_found;
    }

    // Acquire locks in a consistent order to avoid deadlock
    // If source and destination are the same, acquire the lock only once
    std::unique_lock<std::shared_mutex> guard_first;
    std::unique_lock<std::shared_mutex> guard_second;

    if (&dest_source == view.source_) {
        // Same dataset - acquire lock once
        guard_first = std::unique_lock<std::shared_mutex>(*dest_source.file_lock);
    } else {
        // Different datasets - acquire in order of pointer address to avoid deadlock
        if (dest_source.file_lock.get() < view.source_->file_lock.get()) {
            guard_first = std::unique_lock<std::shared_mutex>(*dest_source.file_lock);
            guard_second = std::unique_lock<std::shared_mutex>(*view.source_->file_lock);
        } else {
            guard_first = std::unique_lock<std::shared_mutex>(*view.source_->file_lock);
            guard_second = std::unique_lock<std::shared_mutex>(*dest_source.file_lock);
        }
    }

    if (!dest_source.dataset_map || !dest_source.storage || !view.source_->dataset_map || !view.source_->storage) {
        return last_status_ = Status::file_not_found;
    }

    auto* dest_map = as_dataset_map(dest_source.dataset_map);
    auto* src_map = as_dataset_map(view.source_->dataset_map);

    // Build the destination key
    std::string dest_key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) dest_key += '.';
        dest_key += segments[i];
    }

    // Get the prefix from the view (source key)
    const std::string& src_prefix = view.prefix_;
    std::string src_pattern = src_prefix.empty() ? "" : src_prefix + ".";

    // 1. Delete destination if it exists
    if (!dest_key.empty()) {
        // Delete the exact key if it exists
        dest_map->erase(InterprocessString(dest_key.c_str(), dest_source.storage->file.get_segment_manager()->get_allocator<char>()));
        
        // Delete all keys under the destination prefix
        std::string dest_pattern = dest_key + ".";
        auto it = dest_map->begin();
        while (it != dest_map->end()) {
            std::string_view key(it->first.c_str(), it->first.size());
            if (key.starts_with(dest_pattern)) {
                it = dest_map->erase(it);
            } else {
                ++it;
            }
        }
    }

    // 2. Copy all keys from source to destination
    for (const auto& [src_ipc_key, src_ipc_value] : *src_map) {
        std::string_view src_full_key(src_ipc_key.c_str(), src_ipc_key.size());
        
        // Verify if this key belongs to the subtree of the view
        bool matches = false;
        std::string new_key;

        if (src_prefix.empty()) {
            // The view is the dataset root, copy everything
            matches = true;
            new_key = dest_key.empty() ? std::string(src_full_key) : dest_key + "." + std::string(src_full_key);
        } else {
            // The view is a subtree, copy only matching keys
            if (src_full_key == src_prefix || src_full_key.starts_with(src_pattern)) {
                matches = true;
                if (src_full_key == src_prefix) {
                    // It's the exact key (node with value)
                    new_key = dest_key;
                } else {
                    // It's a descendant, extract the relative part
                    std::string_view relative = src_full_key.substr(src_pattern.size());
                    new_key = dest_key.empty() ? std::string(relative) : dest_key + "." + std::string(relative);
                }
            }
        }

        if (matches) {
            // Copy the value
            auto segment_mgr = dest_source.storage->file.get_segment_manager();
            auto allocator = segment_mgr->get_allocator<char>();
            
            InterprocessString new_key_ipc(new_key.c_str(), allocator);
            InterprocessValue new_value_ipc(src_ipc_value.c_str(), src_ipc_value.size(), allocator);

            auto* dest_map_typed = as_dataset_map(dest_source.dataset_map);
            dest_map_typed->insert_or_assign(new_key_ipc, new_value_ipc);
        }
    }

    return last_status_ = Status::ok;
}

std::optional<akasha::Store::DatasetView> Store::get_dataset_view(std::string_view key_path) const {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dataset_id = segments.front();
    auto source_it = std::find_if(sources_.begin(), sources_.end(), [dataset_id](const Source& source) {
        return source.id == dataset_id;
    });

    if (source_it == sources_.end()) {
        return std::nullopt;
    }

    const Source& source = *source_it;
    if (!source.file_lock) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source.file_lock);

    if (!source.dataset_map || !source.storage) {
        return std::nullopt;
    }

    auto* map = as_dataset_map(source.dataset_map);

    // Case 1: Dataset only (no specific key)
    if (segments.size() == 1) {
        // Always return a view of the dataset (even if empty)
        // Allows virtual navigation
        // NOTE: prefix_ must be empty for dataset root, because the map
        // is already filtered by dataset and contains keys without the dataset prefix
        return DatasetView{&source, ""};
    }

    // Case 2+: Specific key -> build the complete path
    std::string key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            key += '.';
        }
        key += segments[i];
    }

    // First check if there's an exact value (O(log n) lookup)
    auto it = map->find(InterprocessString(key.c_str(), source.storage->file.get_segment_manager()->get_allocator<char>()));
    if (it != map->end()) {
        // Has exact value -> return view immediately
        return DatasetView{&source, key};
    }

    // No exact value found, check if it's a prefix (existing subtree)
    std::string prefix = key + ".";
    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        if (full_key.starts_with(prefix)) {
            // There are keys under this prefix -> return View
            return DatasetView{&source, key};
        }
    }

    // Key doesn't exist as value or prefix -> return nullopt
    return std::nullopt;
}

bool Store::DatasetView::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

bool Store::DatasetView::has_value() const {
    if (source_ == nullptr || !source_->file_lock) {
        return false;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source_->file_lock);

    if (!source_->dataset_map || !source_->storage) {
        return false;
    }

    auto* map = as_dataset_map(source_->dataset_map);

    // If prefix_ is empty, it's dataset root -> search for "__root__"
    if (prefix_.empty()) {
        auto it = map->find(InterprocessString("__root__", source_->storage->file.get_segment_manager()->get_allocator<char>()));
        return it != map->end();
    }

    // For any other key, search for the exact key
    auto it = map->find(InterprocessString(prefix_.c_str(), source_->storage->file.get_segment_manager()->get_allocator<char>()));
    return it != map->end();
}

bool Store::DatasetView::has_keys() const {
    if (source_ == nullptr || !source_->file_lock) {
        return false;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source_->file_lock);

    if (!source_->dataset_map || !source_->storage) {
        return false;
    }

    auto* map = as_dataset_map(source_->dataset_map);

    // Check if there are keys under this prefix
    std::string prefix = prefix_.empty() ? "" : prefix_ + ".";
    std::size_t prefix_len = prefix.size();

    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());

        if (prefix.empty()) {
            // In the dataset root, search for any key that is not "__root__"
            if (full_key != "__root__") {
                return true;
            }
        } else if (full_key.starts_with(prefix)) {
            // There are keys under this prefix
            return true;
        }
    }

    return false;
}

std::vector<std::string> Store::DatasetView::keys() const {
    std::vector<std::string> result;
    
    if (source_ == nullptr || !source_->file_lock) {
        return result;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source_->file_lock);

    if (!source_->dataset_map || !source_->storage) {
        return result;
    }

    auto* map = as_dataset_map(source_->dataset_map);

    // Find all keys under this prefix
    std::string prefix = prefix_.empty() ? "" : prefix_ + ".";
    std::size_t prefix_len = prefix.size();

    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        
        if (prefix.empty() || full_key.starts_with(prefix)) {
            std::string_view relative_key = full_key.substr(prefix_len);
            
            // Only include first-level keys (no additional dots)
            std::size_t dot_pos = relative_key.find('.');
            if (dot_pos == std::string_view::npos) {
                // It's a direct key
                result.emplace_back(relative_key);
            } else {
                // It's a subkey, add only the prefix if not already
                std::string first_segment(relative_key.substr(0, dot_pos));
                if (std::find(result.begin(), result.end(), first_segment) == result.end()) {
                    result.push_back(std::move(first_segment));
                }
            }
        }
    }

    return result;
}

Status Store::last_status() const noexcept {
    return last_status_;
}

}  // namespace akasha
