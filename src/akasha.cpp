#include "akasha.hpp"

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/throw_exception.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace {

namespace bip = boost::interprocess;

using SegmentManager = bip::managed_mapped_file::segment_manager;
using CharAllocator = bip::allocator<char, SegmentManager>;
using InterprocessString = bip::basic_string<char, std::char_traits<char>, CharAllocator>;
using InterprocessValue = InterprocessString;

struct InterprocessStringLess {
    bool operator()(const InterprocessString& a, const InterprocessString& b) const {
        return std::string_view(a.c_str(), a.size()) < std::string_view(b.c_str(), b.size());
    }
    
    // Heterogeneous lookup: InterprocessString vs std::string_view
    bool operator()(const InterprocessString& a, std::string_view b) const {
        return std::string_view(a.c_str(), a.size()) < b;
    }
    
    bool operator()(std::string_view a, const InterprocessString& b) const {
        return a < std::string_view(b.c_str(), b.size());
    }
    
    using is_transparent = void;  // Enable heterogeneous lookup
};

using MapAllocator = bip::allocator<std::pair<const InterprocessString, InterprocessValue>, SegmentManager>;
using InterprocessDatasetMap = bip::map<InterprocessString, InterprocessValue, InterprocessStringLess, MapAllocator>;

constexpr const char* kDatasetMapName = "akasha_root";
constexpr const char* kFormatVersionKeyName = "akasha_version";
constexpr uint32_t kFormatVersion = 1u;
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

// Migration functions: v0 -> v1, v1 -> v2, etc.
// Each function migrates from version N to N+1 in-place

namespace {
// Migration from v0 (no version marker) to v1 (with version marker)
Status migrate_v0_to_v1(std::shared_ptr<Store::MappedFileStorage>& storage) {
    // v0 -> v1: Simply ensures version marker exists and is set to 1
    // No data transformation needed, format is identical
    try {
        auto* version = storage->file.find_or_construct<uint32_t>(kFormatVersionKeyName)(kFormatVersion);
        if (!version) {
            return Status::file_write_error;
        }
        
        if (!storage->file.flush()) {
            return Status::file_write_error;
        }
        
        return Status::ok;
    } catch (...) {
        return Status::file_write_error;
    }
}

// Migration chain: array of function pointers, index N migrates from version N to N+1
using MigrationFunction = Status(*)(std::shared_ptr<Store::MappedFileStorage>&);
constexpr MigrationFunction kMigrationChain[] = {
    migrate_v0_to_v1,
};

// Parse key_path into dataset_id and subkey (single pass, no allocations)
// Returns (dataset_id, subkey) where subkey is everything after the first dot,
// or "__root__" if there's no dot.
// Validates:
// - No empty segments (no double dots or leading/trailing dots)
// - At least one character in dataset_id
// - If there's a dot, at least one character in subkey
std::optional<std::pair<std::string_view, std::string_view>> parse_key_path_simple(std::string_view key_path) {
    if (key_path.empty()) {
        return std::nullopt;
    }
    
    // Check for leading dot
    if (key_path[0] == '.') {
        return std::nullopt;
    }
    
    // Check for trailing dot
    if (key_path[key_path.size() - 1] == '.') {
        return std::nullopt;
    }
    
    // Check for consecutive dots
    if (key_path.find("..") != std::string_view::npos) {
        return std::nullopt;
    }
    
    const std::size_t dot_pos = key_path.find('.');
    if (dot_pos == std::string_view::npos) {
        // No dot: dataset root
        return std::pair<std::string_view, std::string_view>{key_path, std::string_view("__root__")};
    } else {
        // Has dot: split at the dot
        return std::pair<std::string_view, std::string_view>{
            key_path.substr(0, dot_pos),
            key_path.substr(dot_pos + 1)
        };
    }
}

} // namespace

constexpr std::size_t kMigrationChainSize = std::size(kMigrationChain);

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

const Store::Source* Store::find_source(std::string_view source_id) const {
    auto it = std::find_if(sources_.begin(), sources_.end(), [source_id](const Source& source) {
        return source.id == source_id;
    });
    return it == sources_.end() ? nullptr : &(*it);
}

Store::Source* Store::find_source(std::string_view source_id) {
    return const_cast<Source*>(std::as_const(*this).find_source(source_id));
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

// Helper: Find affected source indexes and cleanup storage (extract common pattern)
bool Store::find_and_cleanup_sources_for_path(const std::string& file_path, std::vector<std::size_t>& affected_indexes) {
    affected_indexes.clear();
    affected_indexes.reserve(sources_.size());
    for (std::size_t index = 0; index < sources_.size(); ++index) {
        if (sources_[index].file_path == file_path) {
            affected_indexes.push_back(index);
        }
    }

    if (affected_indexes.empty()) {
        return false;
    }

    // Cleanup: reset storage and dataset_map pointers
    for (const std::size_t index : affected_indexes) {
        sources_[index].dataset_map = nullptr;
        sources_[index].storage.reset();
    }

    return true;
}

// Helper: Reload storage and dataset_map for affected sources
// use_construct: if true, use find_or_construct for dataset_map; if false, use find
bool Store::reload_sources_for_path(const std::string& file_path, const std::vector<std::size_t>& affected_indexes, bool use_construct) {
    for (const std::size_t index : affected_indexes) {
        try {
            auto storage = std::make_shared<MappedFileStorage>(
                file_path,
                initial_mapped_file_size_.load(std::memory_order_relaxed)
            );

            InterprocessDatasetMap* dataset_map = nullptr;
            if (use_construct) {
                // Used by shrink: need to reconstruct if missing
                dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
                    kDatasetMapName
                )(storage->file.get_segment_manager());
            } else {
                // Used by grow/compact: dataset must already exist
                const auto found = storage->file.find<InterprocessDatasetMap>(kDatasetMapName);
                dataset_map = found.first;
            }

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

std::optional<std::vector<std::size_t>> Store::prepare_remap(const std::string& file_path) {
    if (file_path.empty()) {
        return std::nullopt;
    }

    std::vector<std::size_t> affected_indexes;
    if (!find_and_cleanup_sources_for_path(file_path, affected_indexes)) {
        return std::nullopt;
    }

    return affected_indexes;
}

bool Store::grow_and_remap_sources_for_path(const std::string& file_path, std::size_t grow_by_bytes) {
    if (grow_by_bytes == 0) {
        return false;
    }

    auto affected = prepare_remap(file_path);
    if (!affected) {
        return false;
    }

    try {
        bip::managed_mapped_file::grow(file_path.c_str(), grow_by_bytes);
    } catch (...) {
        return false;
    }

    return reload_sources_for_path(file_path, *affected, false);
}

bool Store::shrink_and_remap_sources_for_path(const std::string& file_path) {
    auto affected = prepare_remap(file_path);
    if (!affected) {
        return false;
    }

    try {
        bip::file_mapping::remove(file_path.c_str());
    } catch (...) {
        return false;
    }

    return reload_sources_for_path(file_path, *affected, true);
}

std::optional<Store::SourceSnapshot> Store::extract_source_snapshot(const std::string& file_path) const {
    for (const auto& source : sources_) {
        if (source.file_path == file_path && source.storage && source.dataset_map) {
            SourceSnapshot snapshot;

            auto* map = as_dataset_map(source.dataset_map);
            for (const auto& [key, value] : *map) {
                std::string k(key.c_str(), key.size());
                std::string v(value.c_str(), value.size());
                snapshot.data_size += k.size() + v.size();
                snapshot.entries.emplace_back(std::move(k), std::move(v));
            }

            auto version_result = source.storage->file.find<uint32_t>(kFormatVersionKeyName);
            if (version_result.first) {
                snapshot.version = *version_result.first;
            }

            return snapshot;
        }
    }
    return std::nullopt;
}

bool Store::rebuild_file_from_snapshot(const std::string& file_path, const SourceSnapshot& snapshot) {
    try {
        bip::file_mapping::remove(file_path.c_str());
    } catch (...) {
        return false;
    }

    const std::size_t estimated_size = std::max(
        initial_mapped_file_size_.load(std::memory_order_relaxed),
        snapshot.data_size * 2 + 4096
    );

    try {
        auto storage = std::make_shared<MappedFileStorage>(file_path, estimated_size);

        auto* ver = storage->file.find_or_construct<uint32_t>(kFormatVersionKeyName)(snapshot.version);
        if (!ver) { return false; }

        auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
            kDatasetMapName
        )(storage->file.get_segment_manager());
        if (!dataset_map) { return false; }

        auto* segment_mgr = storage->file.get_segment_manager();
        auto allocator = segment_mgr->get_allocator<char>();

        for (const auto& [key, value] : snapshot.entries) {
            InterprocessString ipc_key(key.c_str(), key.size(), allocator);
            InterprocessValue ipc_value(value.c_str(), value.size(), allocator);
            dataset_map->emplace(std::move(ipc_key), std::move(ipc_value));
        }

        storage->file.flush();
    } catch (...) {
        return false;
    }

    try {
        bip::managed_mapped_file::shrink_to_fit(file_path.c_str());
    } catch (...) {
        // Not critical — the file is already defragmented
    }

    return true;
}

bool Store::compact_and_remap_sources_for_path(const std::string& file_path) {
    auto snapshot = extract_source_snapshot(file_path);
    if (!snapshot) {
        return false;
    }

    auto affected = prepare_remap(file_path);
    if (!affected) {
        return false;
    }

    if (!rebuild_file_from_snapshot(file_path, *snapshot)) {
        return false;
    }

    return reload_sources_for_path(file_path, *affected, false);
}

Status Store::load(std::string_view source_id, std::string_view file_path, FileOptions options) {
    // Step 1: Validate parameters
    if (source_id.empty()) {
        return last_status_ = Status::invalid_key_path;
    }
    if (file_path.empty()) {
        return last_status_ = Status::invalid_file_path;
    }

    // Validate path contains no control characters (0x00-0x1F, 0x7F)
    for (unsigned char c : file_path) {
        if (c < 0x20 || c == 0x7F) {
            return last_status_ = Status::invalid_file_path;
        }
    }

    const std::string path{file_path};
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    // Step 2: Verify source_id is not already loaded
    if (find_source(source_id) != nullptr) {
        return last_status_ = Status::key_conflict;
    }

    // Also verify file_path is not already loaded by another source_id
    for (const auto& source : sources_) {
        if(source.file_path == path) {
            return last_status_ = Status::source_already_loaded;
        }
    }

    const std::size_t initial_size = initial_mapped_file_size_.load(std::memory_order_relaxed);
    const auto file_lock = get_or_create_file_lock(path);

    // Step 3: Check if file exists
    bool file_exists = std::filesystem::exists(path);
    
    // Step 3.1/3.2: Handle file creation or missing file
    if (!file_exists) {
        bool create_enabled = (options & FileOptions::create_if_missing) == FileOptions::create_if_missing;
        if (!create_enabled) {
            return last_status_ = Status::file_not_found;
        }
    }

    std::unique_lock<std::shared_mutex> write_guard(*file_lock);

    // Step 4: Open or create the file
    std::shared_ptr<MappedFileStorage> storage;
    try {
        // MappedFileStorage uses bip::open_or_create, so if file doesn't exist,
        // it will be created with initial_size
        storage = std::make_shared<MappedFileStorage>(path, initial_size);
    } catch (...) {
        return last_status_ = Status::file_read_error;
    }

    // Find or create dataset map
    auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
        kDatasetMapName
    )(storage->file.get_segment_manager());

    if (!dataset_map) {
        return last_status_ = Status::file_read_error;
    }

    // Step 5: Verify version
    auto version_result = storage->file.find<uint32_t>(kFormatVersionKeyName);
    uint32_t current_version = (version_result.first != nullptr) ? *version_result.first : 0u;

    // If file didn't exist before, create it now with version 1
    if (current_version == 0u && !file_exists) {
        auto* version = storage->file.find_or_construct<uint32_t>(kFormatVersionKeyName)(kFormatVersion);
        if (!version) {
            return last_status_ = Status::file_write_error;
        }
        try {
            storage->file.flush();
        } catch (...) {
            return last_status_ = Status::file_write_error;
        }
        current_version = kFormatVersion;
    }

    // Step 5.1/5.2: Handle version mismatch
    if (current_version != kFormatVersion) {
        if (current_version < kFormatVersion) {
            // File is older, check if migration is enabled
            bool migrate_enabled = (options & FileOptions::migrate_if_incompatible) == FileOptions::migrate_if_incompatible;
            if (!migrate_enabled) {
                return last_status_ = Status::incompatible_format;
            }
            // Execute migration chain: v0->v1->v2->...->current
            Status migrate_status = migrate(storage, current_version);
            if (migrate_status != Status::ok) {
                return last_status_ = migrate_status;
            }
        } else {
            // File is newer, unsupported
            return last_status_ = Status::incompatible_format;
        }
    }

    // Step 6: Load the file completely and register source
    try {
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

Status Store::unload(std::string_view source_id) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    auto source_it = std::find_if(sources_.begin(), sources_.end(), 
        [source_id](const Source& source) {
            return source.id == source_id;
        });

    if (source_it == sources_.end()) {
        return last_status_ = Status::dataset_not_found;
    }

    if (source_it->file_lock) {
        std::unique_lock<std::shared_mutex> file_guard(*source_it->file_lock);
        source_it->dataset_map = nullptr;
        source_it->storage = nullptr;
        source_it->file_lock = nullptr;
    }

    sources_.erase(source_it);
    
    return last_status_ = Status::ok;
}

Status Store::set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size) {
    auto key_parts = parse_key_path_simple(key_path);
    if (!key_parts.has_value()) {
        return last_status_ = Status::invalid_key_path;
    }

    const auto& [dataset_id, subkey] = key_parts.value();

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

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

    std::size_t grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    const int max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);

    // Helper: grow file and remap after allocation failure
    auto handle_grow_retry = [&](int attempt) -> bool {
        if (attempt == max_grow_retries) {
            return false;
        }

        const std::string source_file_path = source->file_path;
        const std::size_t current_file_size = source->storage->file.get_size();
        const std::size_t base_step = initial_grow_step_.load(std::memory_order_relaxed);
        const std::size_t dynamic_step = std::max(grow_step, std::max(base_step, current_file_size / 2));

        if (!grow_and_remap_sources_for_path(source_file_path, dynamic_step)) {
            return false;
        }

        source = find_source(dataset_id);
        if (source == nullptr || !source->storage || !source->dataset_map) {
            return false;
        }

        grow_step = dynamic_step * 2;
        return true;
    };

    for (int attempt = 0; attempt <= max_grow_retries; ++attempt) {
        try {
            auto* segment_mgr = source->storage->file.get_segment_manager();
            auto* map = as_dataset_map(source->dataset_map);
            auto allocator = segment_mgr->get_allocator<char>();

            // Create InterprocessValue (InterprocessString) with the bytes
            InterprocessValue ipc_value(static_cast<const char*>(bytes), size, allocator);

            // Insert or replace in the map (direct mmap write)
            // Use heterogeneous lookup with string_view to avoid creating ipc_key for find
            auto it = map->find(subkey);
            if (it == map->end()) {
                // Key doesn't exist, create ipc_key only for insertion
                InterprocessString ipc_key(subkey.data(), subkey.size(), allocator);
                map->emplace(ipc_key, ipc_value);
            } else {
                // Key exists: simple assignment
                // Note: Boost.Interprocess may leave fragmented space when size shrinks significantly.
                // Call store.compact() explicitly to reclaim space.
                it->second = ipc_value;
            }

            if (!source->storage->file.flush()) {
                return last_status_ = Status::file_write_error;
            }

            return last_status_ = Status::ok;
        } catch (const bip::interprocess_exception&) {
            if (!handle_grow_retry(attempt)) {
                return last_status_ = Status::file_write_error;
            }
        } catch (const boost::container::length_error&) {
            if (!handle_grow_retry(attempt)) {
                return last_status_ = Status::file_write_error;
            }
        } catch (const std::length_error&) {
            // MSVC: Boost compiled with BOOST_CONTAINER_USE_STD_EXCEPTIONS
            // throws std::length_error instead of boost::container::length_error
            if (!handle_grow_retry(attempt)) {
                return last_status_ = Status::file_write_error;
            }
        }
    }

    return last_status_ = Status::file_write_error;
}

std::optional<std::string_view> Store::get_bytes_impl(std::string_view key_path) const {
    auto key_parts = parse_key_path_simple(key_path);
    if (!key_parts.has_value()) {
        last_status_ = Status::invalid_key_path;
        return std::nullopt;
    }

    const auto& [dataset_id, subkey] = key_parts.value();

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const Source* source = find_source(dataset_id);
    if (source == nullptr) {
        last_status_ = Status::dataset_not_found;
        return std::nullopt;
    }

    if (!source->file_lock) {
        last_status_ = Status::file_read_error;
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        last_status_ = Status::file_read_error;
        return std::nullopt;
    }

    auto* map = as_dataset_map(source->dataset_map);
    auto it = map->find(subkey);
    if (it != map->end()) {
        const auto& data_bytes = it->second;
        return std::string_view(data_bytes.c_str(), data_bytes.size());
    }

    last_status_ = Status::key_not_found;
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

    auto key_parts = parse_key_path_simple(key_path);
    if (!key_parts.has_value()) {
        return last_status_ = Status::invalid_key_path;
    }

    const auto& [dataset_id, subkey] = key_parts.value();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    if (!source->storage || !source->dataset_map || !source->file_lock) {
        return last_status_ = Status::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);
    auto* map = as_dataset_map(source->dataset_map);

    if (subkey == std::string_view("__root__")) {
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

    // If it's a subkey, use it as the prefix
    const std::string prefix_with_dot = std::string(subkey) + ".";

    for (auto it = map->begin(); it != map->end();) {
        const std::string_view current_key(it->first.c_str(), it->first.size());
        if (current_key == subkey || current_key.starts_with(prefix_with_dot)) {
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

Status Store::migrate(std::shared_ptr<MappedFileStorage>& storage, uint32_t current_version) {
    const uint32_t target_version = kFormatVersion;
    
    if (current_version == target_version) {
        // Already at target version
        return Status::ok;
    } else if (current_version > target_version) {
        // File is from a newer version, incompatible
        return Status::incompatible_format;
    }

    // Programming error: target version defined but migration functions not implemented
    assert(target_version <= kMigrationChainSize && "Missing migration functions for target version");
    if (target_version > kMigrationChainSize) {
        return Status::incompatible_format;
    }

    // Sequentially apply migrations: v0->v1, v1->v2, etc.
    for (uint32_t v = current_version; v < target_version; ++v) {
        // Execute migration v -> v+1
        Status status = kMigrationChain[v](storage);
        if (status != Status::ok) {
            return status;
        }
    }
    
    return Status::ok;
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
    auto key_parts = parse_key_path_simple(key_path);
    if (!key_parts.has_value()) {
        return last_status_ = Status::invalid_key_path;
    }

    const auto& [dest_dataset_id, dest_subkey] = key_parts.value();

    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    Source* dest_source_ptr = find_source(dest_dataset_id);
    if (dest_source_ptr == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    Source& dest_source = *dest_source_ptr;
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

    // Build the destination key (empty if __root__, otherwise use subkey)
    std::string dest_key = (dest_subkey == std::string_view("__root__")) ? "" : std::string(dest_subkey);

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
    auto key_parts = parse_key_path_simple(key_path);
    if (!key_parts.has_value()) {
        return std::nullopt;
    }

    const auto& [dataset_id, subkey] = key_parts.value();

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return std::nullopt;
    }

    if (!source->file_lock) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        return std::nullopt;
    }

    auto* map = as_dataset_map(source->dataset_map);

    // Case 1: Dataset only (no specific key)
    if (subkey == std::string_view("__root__")) {
        return DatasetView{source, ""};
    }

    // Case 2+: Specific key
    std::string key(subkey);

    // First check if there's an exact value (O(log n) lookup, heterogeneous lookup without allocation)
    auto it = map->find(subkey);
    if (it != map->end()) {
        return DatasetView{source, key};
    }

    std::string prefix = key + ".";
    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        if (full_key.starts_with(prefix)) {
            return DatasetView{source, key};
        }
    }

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

    // If prefix_ is empty, it's dataset root -> search for "__root__" (heterogeneous lookup)
    if (prefix_.empty()) {
        auto it = map->find(std::string_view("__root__"));
        return it != map->end();
    }

    // For any other key, search for the exact key (heterogeneous lookup)
    auto it = map->find(std::string_view(prefix_));
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
                // The map is sorted, so identical prefixes are contiguous
                std::string_view first_segment = relative_key.substr(0, dot_pos);
                if (result.empty() || result.back() != first_segment) {
                    result.emplace_back(first_segment);
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
