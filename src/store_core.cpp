#include "store_internal.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <mutex>
#include <shared_mutex>

namespace {

// Migration from v0 (no version marker) to v1 (with version marker, data untagged)
akasha::Status migrate_v0_to_v1(std::shared_ptr<akasha::Store::MappedFileStorage>& storage) {
    // v0→v1: Add version marker. Data remains untagged (raw bytes).
    try {
        auto* version = storage->file.find_or_construct<uint32_t>(kFormatVersionKeyName)(1u);
        if (!version) {
            return akasha::Status::file_write_error;
        }
        
        if (!storage->file.flush()) {
            return akasha::Status::file_write_error;
        }
        
        return akasha::Status::ok;
    } catch (...) {
        return akasha::Status::file_write_error;
    }
}

// Migration from v1 (version marker, untagged data) to v2 (version marker, tagged data)
// Strategy: Backup old file, clear incompatible data, mark as v2.
// Data content is not preserved (raw v1 bytes not interpretable in v2).
akasha::Status migrate_v1_to_v2(std::shared_ptr<akasha::Store::MappedFileStorage>& storage) {
    try {
        namespace fs = std::filesystem;
        
        // 1. Create backup to .bak
        const std::string backup_path = storage->file_path + ".bak";
        try {
            fs::copy_file(storage->file_path, backup_path, fs::copy_options::overwrite_existing);
        } catch (...) {
            // Backup failure is not fatal, continue anyway
        }
        
        // 2. Reconstruct the map to clear all v1 data
        auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
            kDatasetMapName
        )(storage->file.get_segment_manager());
        
        if (!dataset_map) {
            return akasha::Status::file_write_error;
        }
        
        // Clear all entries from the map (removes v1 untagged data)
        dataset_map->clear();
        
        // 3. Update version to v2
        auto version_result = storage->file.find<uint32_t>(kFormatVersionKeyName);
        if (version_result.first == nullptr) {
            // No version marker found, create one
            auto* version = storage->file.find_or_construct<uint32_t>(kFormatVersionKeyName)(2u);
            if (!version) {
                return akasha::Status::file_write_error;
            }
        } else {
            // Update existing version marker to 2
            *const_cast<uint32_t*>(version_result.first) = 2u;
        }
        
        if (!storage->file.flush()) {
            return akasha::Status::file_write_error;
        }
        
        return akasha::Status::ok;
    } catch (...) {
        return akasha::Status::file_write_error;
    }
}

using MigrationFunction = akasha::Status(*)(std::shared_ptr<akasha::Store::MappedFileStorage>&);
constexpr MigrationFunction kMigrationChain[] = {
    migrate_v0_to_v1,
    migrate_v1_to_v2,
};
constexpr std::size_t kMigrationChainSize = std::size(kMigrationChain);

}  // namespace

namespace akasha {

// ── find_source ──────────────────────────────────────────────────────────────

const Store::Source* Store::find_source(std::string_view source_id) const {
    auto it = std::find_if(sources_.begin(), sources_.end(), [source_id](const Source& source) {
        return source.id == source_id;
    });
    return it == sources_.end() ? nullptr : &(*it);
}

Store::Source* Store::find_source(std::string_view source_id) {
    return const_cast<Source*>(std::as_const(*this).find_source(source_id));
}

// ── Performance tuning ───────────────────────────────────────────────────────

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

Status Store::last_status() const noexcept {
    return last_status_;
}

// ── File locks ───────────────────────────────────────────────────────────────

std::shared_ptr<detail::FileLockMutex> Store::get_or_create_file_lock(const std::string& file_path) const {
    std::lock_guard<std::mutex> guard(file_locks_mutex_);

    const auto it = file_locks_.find(file_path);
    if (it != file_locks_.end()) {
        return it->second;
    }

    auto file_lock = std::make_shared<detail::FileLockMutex>();
    file_locks_.emplace(file_path, file_lock);
    return file_lock;
}

// ── load ─────────────────────────────────────────────────────────────────────

Status Store::load(std::string_view source_id, std::string_view file_path, FileOptions options) {
    if (source_id.empty()) {
        return last_status_ = Status::invalid_key_path;
    }
    if (file_path.empty()) {
        return last_status_ = Status::invalid_file_path;
    }

    for (unsigned char c : file_path) {
        if (c < 0x20 || c == 0x7F) {
            return last_status_ = Status::invalid_file_path;
        }
    }

    const std::string path{file_path};
    std::unique_lock<detail::FileLockMutex> sources_guard(sources_mutex_);

    if (find_source(source_id) != nullptr) {
        return last_status_ = Status::key_conflict;
    }

    for (const auto& source : sources_) {
        if(source.file_path == path) {
            return last_status_ = Status::source_already_loaded;
        }
    }

    const std::size_t initial_size = initial_mapped_file_size_.load(std::memory_order_relaxed);
    const auto file_lock = get_or_create_file_lock(path);

    bool file_exists = std::filesystem::exists(path);
    
    if (!file_exists) {
        bool create_enabled = (options & FileOptions::create_if_missing) == FileOptions::create_if_missing;
        if (!create_enabled) {
            return last_status_ = Status::file_not_found;
        }
    }

    std::unique_lock<detail::FileLockMutex> write_guard(*file_lock);

    std::shared_ptr<MappedFileStorage> storage;
    try {
        storage = std::make_shared<MappedFileStorage>(path, initial_size);
    } catch (...) {
        return last_status_ = Status::file_read_error;
    }

    auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
        kDatasetMapName
    )(storage->file.get_segment_manager());

    if (!dataset_map) {
        return last_status_ = Status::file_read_error;
    }

    auto version_result = storage->file.find<uint32_t>(kFormatVersionKeyName);
    uint32_t current_version = (version_result.first != nullptr) ? *version_result.first : 0u;

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

    if (current_version != kFormatVersion) {
        if (current_version < kFormatVersion) {
            bool migrate_enabled = (options & FileOptions::migrate_if_incompatible) == FileOptions::migrate_if_incompatible;
            if (!migrate_enabled) {
                return last_status_ = Status::incompatible_format;
            }
            Status migrate_status = migrate(storage, current_version);
            if (migrate_status != Status::ok) {
                return last_status_ = migrate_status;
            }
        } else {
            return last_status_ = Status::incompatible_format;
        }
    }

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

// ── unload ───────────────────────────────────────────────────────────────────

Status Store::unload(std::string_view source_id) {
    std::unique_lock<detail::FileLockMutex> sources_guard(sources_mutex_);

    auto source_it = std::find_if(sources_.begin(), sources_.end(), 
        [source_id](const Source& source) {
            return source.id == source_id;
        });

    if (source_it == sources_.end()) {
        return last_status_ = Status::dataset_not_found;
    }

    if (source_it->file_lock) {
        std::unique_lock<detail::FileLockMutex> file_guard(*source_it->file_lock);
        source_it->dataset_map = nullptr;
        source_it->storage = nullptr;
        source_it->file_lock = nullptr;
    }

    sources_.erase(source_it);
    
    return last_status_ = Status::ok;
}

// ── migrate ──────────────────────────────────────────────────────────────────

Status Store::migrate(std::shared_ptr<MappedFileStorage>& storage, uint32_t current_version) {
    const uint32_t target_version = kFormatVersion;
    
    if (current_version == target_version) {
        return Status::ok;
    } else if (current_version > target_version) {
        return Status::incompatible_format;
    }

    assert(target_version <= kMigrationChainSize && "Missing migration functions for target version");
    if (target_version > kMigrationChainSize) {
        return Status::incompatible_format;
    }

    for (uint32_t v = current_version; v < target_version; ++v) {
        Status status = kMigrationChain[v](storage);
        if (status != Status::ok) {
            return status;
        }
    }
    
    return Status::ok;
}

}  // namespace akasha
