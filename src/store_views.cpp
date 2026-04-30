#include "store_internal.hpp"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace akasha {

// ── DatasetView non-template methods ─────────────────────────────────────────

bool Store::DatasetView::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

bool Store::DatasetView::has_value() const {
    if (source_ == nullptr || !source_->file_lock) return false;
    std::shared_lock<detail::FileLockMutex> read_guard(*source_->file_lock);
    const std::string_view key = prefix_.empty() ? "__root__" : std::string_view(prefix_);
    return source_->store->get_bytes_no_lock(source_, key).has_value();
}

bool Store::DatasetView::has_keys() const {
    if (source_ == nullptr || !source_->file_lock) return false;
    std::shared_lock<detail::FileLockMutex> read_guard(*source_->file_lock);
    return source_->store->has_subkeys_no_lock(source_, prefix_);
}

std::vector<std::string> Store::DatasetView::keys() const {
    if (source_ == nullptr || !source_->file_lock) return {};
    std::shared_lock<detail::FileLockMutex> read_guard(*source_->file_lock);
    return source_->store->get_subkeys_no_lock(source_, prefix_);
}

// ── get_dataset_view ─────────────────────────────────────────────────────────

std::optional<Store::DatasetView> Store::get_dataset_view(std::string_view key_path) const {
    auto key_parts = parse_key_path(key_path);
    if (!key_parts.has_value()) {
        return std::nullopt;
    }

    const auto& [dataset_id, subkey] = key_parts.value();

    std::shared_lock<detail::FileLockMutex> sources_guard(sources_mutex_);

    const Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return std::nullopt;
    }

    if (!source->file_lock) {
        return std::nullopt;
    }

    std::shared_lock<detail::FileLockMutex> read_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        return std::nullopt;
    }

    auto* map = as_dataset_map(source->dataset_map);

    // subkey always starts with "__root__" now (injected by parse_key_path).
    // For the root case ("__root__"), DatasetView prefix IS "__root__".
    std::string key(subkey);

    if (subkey == std::string_view("__root__")) {
        // Root always exists if the dataset is loaded.
        return DatasetView{source, key};
    }

    auto it = map->find(subkey);
    if (it != map->end()) {
        return DatasetView{source, key};
    }

    std::string prefix = key + "/";
    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        if (full_key.starts_with(prefix)) {
            return DatasetView{source, key};
        }
    }

    return std::nullopt;
}

// ── set_datasetview_impl ─────────────────────────────────────────────────────

Status Store::set_datasetview_impl(std::string_view key_path, const DatasetView& view) {
    if (view.source_ == nullptr) {
        return last_status_ = Status::invalid_key_path;
    }

    auto key_parts = parse_key_path(key_path);
    if (!key_parts.has_value()) {
        return last_status_ = Status::invalid_key_path;
    }

    const auto& [dest_dataset_id, dest_subkey] = key_parts.value();

    std::unique_lock<detail::FileLockMutex> sources_guard(sources_mutex_);

    Source* dest_source_ptr = find_source(dest_dataset_id);
    if (dest_source_ptr == nullptr) {
        return last_status_ = Status::dataset_not_found;
    }

    Source& dest_source = *dest_source_ptr;
    if (!dest_source.file_lock) {
        return last_status_ = Status::file_not_found;
    }

    std::unique_lock<detail::FileLockMutex> guard_first;
    std::unique_lock<detail::FileLockMutex> guard_second;

    if (&dest_source == view.source_) {
        guard_first = std::unique_lock<detail::FileLockMutex>(*dest_source.file_lock);
    } else {
        if (dest_source.file_lock.get() < view.source_->file_lock.get()) {
            guard_first = std::unique_lock<detail::FileLockMutex>(*dest_source.file_lock);
            guard_second = std::unique_lock<detail::FileLockMutex>(*view.source_->file_lock);
        } else {
            guard_first = std::unique_lock<detail::FileLockMutex>(*view.source_->file_lock);
            guard_second = std::unique_lock<detail::FileLockMutex>(*dest_source.file_lock);
        }
    }

    if (!dest_source.dataset_map || !dest_source.storage || !view.source_->dataset_map || !view.source_->storage) {
        return last_status_ = Status::file_not_found;
    }

    auto* dest_map = as_dataset_map(dest_source.dataset_map);
    auto* src_map = as_dataset_map(view.source_->dataset_map);

    // dest_subkey already contains __root__ prefix (injected by parse_key_path).
    std::string dest_key(dest_subkey);

    // src_prefix already contains __root__ prefix from DatasetView construction.
    const std::string& src_prefix = view.prefix_;
    std::string src_pattern = src_prefix + "/";

    // 1. Delete destination if it exists
    {
        dest_map->erase(InterprocessString(dest_key.c_str(), dest_source.storage->file.get_segment_manager()->get_allocator<char>()));
        
        std::string dest_pattern = dest_key + "/";
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
        
        bool matches = false;
        std::string new_key;

        if (src_full_key == src_prefix || src_full_key.starts_with(src_pattern)) {
            matches = true;
            if (src_full_key == src_prefix) {
                new_key = dest_key;
            } else {
                std::string_view relative = src_full_key.substr(src_pattern.size());
                new_key = dest_key + "/" + std::string(relative);
            }
        }

        if (matches) {
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

}  // namespace akasha
