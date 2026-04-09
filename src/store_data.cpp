#include "store_internal.hpp"

#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace akasha {

// ── set_bytes_no_lock ────────────────────────────────────────────────────────

Status Store::set_bytes_no_lock(Source*& source, std::string_view dataset_id,
                                 std::string_view subkey, const void* bytes,
                                 std::size_t size, detail::TypeTag tag) {
    std::size_t grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    const int max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);

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

            const char tag_byte = static_cast<char>(static_cast<uint8_t>(tag));

            auto it = map->find(subkey);
            if (it != map->end()) {
                auto& entry = it->second;
                const std::size_t new_size = size + 1;
                if (entry.size() != new_size) {
                    entry.resize(new_size);
                }
                entry[0] = tag_byte;
                if (size > 0) {
                    std::memcpy(&entry[1], static_cast<const char*>(bytes), size);
                }
            } else {
                InterprocessString ipc_key(subkey.data(), subkey.size(), allocator);
                InterprocessValue ipc_value(allocator);
                ipc_value.reserve(size + 1);
                ipc_value.push_back(tag_byte);
                ipc_value.append(static_cast<const char*>(bytes), size);
                map->emplace(std::move(ipc_key), std::move(ipc_value));
            }

            return Status::ok;
        } catch (const bip::interprocess_exception&) {
            if (!handle_grow_retry(attempt)) {
                return Status::file_write_error;
            }
        } catch (const boost::container::length_error&) {
            if (!handle_grow_retry(attempt)) {
                return Status::file_write_error;
            }
        } catch (const std::length_error&) {
            if (!handle_grow_retry(attempt)) {
                return Status::file_write_error;
            }
        }
    }

    return Status::file_write_error;
}

// ── set_bytes_impl ───────────────────────────────────────────────────────────

Status Store::set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size, detail::TypeTag tag) {
    auto key_parts = parse_key_path(key_path);
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

    const Status st = set_bytes_no_lock(source, dataset_id, subkey, bytes, size, tag);
    if (st != Status::ok) {
        return last_status_ = st;
    }

    if (!source->storage->file.flush()) {
        return last_status_ = Status::file_write_error;
    }

    return last_status_ = Status::ok;
}

// ── set_null ─────────────────────────────────────────────────────────────────

Status Store::set_null(std::string_view key_path) {
    return set_bytes_impl(key_path, "", 0, detail::TypeTag::null_type);
}

// ── get_bytes_no_lock ────────────────────────────────────────────────────────

std::optional<std::string_view> Store::get_bytes_no_lock(const Source* source,
                                                          std::string_view subkey) const {
    auto* map = as_dataset_map(source->dataset_map);
    auto it = map->find(subkey);
    if (it == map->end()) {
        last_status_ = Status::key_not_found;
        return std::nullopt;
    }
    const auto& data_bytes = it->second;
    std::string_view full_view(data_bytes.c_str(), data_bytes.size());
    auto [tag, payload] = deserialize_tagged(full_view);
    return payload;
}

// ── get_bytes_impl ───────────────────────────────────────────────────────────

std::optional<std::string_view> Store::get_bytes_impl(std::string_view key_path) const {
    auto key_parts = parse_key_path(key_path);
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

    return get_bytes_no_lock(source, subkey);
}

// ── has ──────────────────────────────────────────────────────────────────────

bool Store::has(std::string_view key_path) const {
    auto key_parts = parse_key_path(key_path);
    if (!key_parts.has_value()) return false;
    const auto& [dataset_id, subkey] = key_parts.value();

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);
    const Source* source = find_source(dataset_id);
    if (!source || !source->file_lock) return false;

    if (subkey == std::string_view("__root__")) {
        return true;
    }

    std::shared_lock<std::shared_mutex> file_guard(*source->file_lock);
    return has_key_no_lock(source, subkey);
}

// ── flush_source ─────────────────────────────────────────────────────────────

bool Store::flush_source(Source* source) const {
    if (!source || !source->storage) return false;
    try {
        return source->storage->file.flush();
    } catch (...) {
        return false;
    }
}

// ── has_key_no_lock ──────────────────────────────────────────────────────────

bool Store::has_key_no_lock(const Source* source, std::string_view subkey) const {
    if (!source || !source->dataset_map || !source->storage) return false;
    const auto* map = as_dataset_map(source->dataset_map);
    if (map->find(subkey) != map->end()) return true;
    const std::string prefix = std::string(subkey) + '/';
    for (const auto& [k, v] : *map) {
        if (std::string_view(k.c_str(), k.size()).starts_with(prefix)) return true;
    }
    return false;
}

// ── has_subkeys_no_lock ──────────────────────────────────────────────────────

bool Store::has_subkeys_no_lock(const Source* source, std::string_view subkey_prefix) const {
    if (!source || !source->dataset_map || !source->storage) return false;
    const auto* map = as_dataset_map(source->dataset_map);
    if (subkey_prefix.empty()) {
        for (const auto& [k, v] : *map)
            if (std::string_view(k.c_str(), k.size()) != "__root__") return true;
        return false;
    }
    const std::string prefix = std::string(subkey_prefix) + '/';
    for (const auto& [k, v] : *map)
        if (std::string_view(k.c_str(), k.size()).starts_with(prefix)) return true;
    return false;
}

// ── get_subkeys_no_lock ──────────────────────────────────────────────────────

std::vector<std::string> Store::get_subkeys_no_lock(const Source* source, std::string_view subkey_prefix) const {
    std::vector<std::string> result;
    if (!source || !source->dataset_map || !source->storage) return result;
    const auto* map = as_dataset_map(source->dataset_map);

    const std::string prefix = subkey_prefix.empty() ? "" : std::string(subkey_prefix) + '/';
    const std::size_t prefix_len = prefix.size();

    for (const auto& [k, v] : *map) {
        std::string_view sv(k.c_str(), k.size());
        if (prefix.empty()) {
            if (sv == "__root__") continue;
        } else {
            if (!sv.starts_with(prefix)) continue;
        }
        std::string_view rel = sv.substr(prefix_len);
        const std::size_t slash = rel.find('/');
        if (slash == std::string_view::npos) {
            result.emplace_back(rel);
        } else {
            std::string_view seg = rel.substr(0, slash);
            if (result.empty() || result.back() != seg)
                result.emplace_back(seg);
        }
    }
    return result;
}

// ── clear_no_lock ────────────────────────────────────────────────────────────

void Store::clear_no_lock(Source* source, std::string_view subkey) {
    if (!source || !source->dataset_map || !source->storage) return;
    auto* map = as_dataset_map(source->dataset_map);
    const std::string prefix_with_slash = std::string(subkey) + '/';
    for (auto it = map->begin(); it != map->end();) {
        const std::string_view current_key(it->first.c_str(), it->first.size());
        if (current_key == subkey || current_key.starts_with(prefix_with_slash)) {
            it = map->erase(it);
        } else {
            ++it;
        }
    }
}

// ── clear ────────────────────────────────────────────────────────────────────

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

    auto key_parts = parse_key_path(key_path);
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
        map->clear();
        if (!source->storage->file.flush()) {
            return last_status_ = Status::file_write_error;
        }

        if (!shrink_and_remap_sources_for_path(source->file_path)) {
            return last_status_ = Status::file_write_error;
        }

        return last_status_ = Status::ok;
    }

    clear_no_lock(source, subkey);

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

}  // namespace akasha
