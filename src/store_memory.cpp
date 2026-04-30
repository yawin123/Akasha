#include "store_internal.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace akasha {

// ── prepare_remap ────────────────────────────────────────────────────────────

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

// ── find_and_cleanup_sources_for_path ────────────────────────────────────────

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

    for (const std::size_t index : affected_indexes) {
        sources_[index].dataset_map = nullptr;
        sources_[index].storage.reset();
    }

    return true;
}

// ── reload_sources_for_path ──────────────────────────────────────────────────

bool Store::reload_sources_for_path(const std::string& file_path, const std::vector<std::size_t>& affected_indexes, bool use_construct) {
    for (const std::size_t index : affected_indexes) {
        try {
            auto storage = std::make_shared<MappedFileStorage>(
                file_path,
                initial_mapped_file_size_.load(std::memory_order_relaxed)
            );

            InterprocessDatasetMap* dataset_map = nullptr;
            if (use_construct) {
                dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
                    kDatasetMapName
                )(storage->file.get_segment_manager());
            } else {
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

// ── grow_and_remap_sources_for_path ──────────────────────────────────────────

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

// ── shrink_and_remap_sources_for_path ────────────────────────────────────────

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

// ── extract_source_snapshot ──────────────────────────────────────────────────

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

// ── rebuild_file_from_snapshot ───────────────────────────────────────────────

bool Store::rebuild_file_from_snapshot(const std::string& file_path, const SourceSnapshot& snapshot) {
    try {
        bip::file_mapping::remove(file_path.c_str());
    } catch (...) {
        return false;
    }

    constexpr std::size_t kPerEntryOverhead = 256;
    const std::size_t estimated_size = std::max(
        initial_mapped_file_size_.load(std::memory_order_relaxed),
        snapshot.data_size * 2 + snapshot.entries.size() * kPerEntryOverhead + 65536
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

        // ── Filter stale entries ──
        std::unordered_map<std::string, std::int64_t> seq_rules;
        std::unordered_map<std::string, std::unordered_set<std::string>> arb_rules;

        constexpr std::string_view count_suffix    = "/__count__";
        constexpr std::string_view children_suffix = "/__children__";

        for (const auto& [key, value] : snapshot.entries) {
            const std::string_view kv = key;

            if (kv.ends_with(count_suffix) && kv.size() > count_suffix.size()) {
                if (value.size() == 9 && static_cast<uint8_t>(value[0]) == 0x02) {
                    std::int64_t n; std::memcpy(&n, value.data() + 1, sizeof(n));
                    seq_rules.emplace(std::string(kv.substr(0, kv.size() - count_suffix.size())), n);
                }
            } else if (kv.ends_with(children_suffix) && kv.size() > children_suffix.size()) {
                if (value.size() >= 1 + sizeof(std::size_t) && static_cast<uint8_t>(value[0]) == 0x04) {
                    std::size_t len; std::memcpy(&len, value.data() + 1, sizeof(len));
                    if (value.size() == 1 + sizeof(std::size_t) + len) {
                        std::string_view content(value.data() + 1 + sizeof(std::size_t), len);
                        std::unordered_set<std::string> valid;
                        valid.insert("__children__");
                        std::size_t pos = 0;
                        while (pos < content.size()) {
                            const auto nl = content.find('\n', pos);
                            if (nl == std::string_view::npos) {
                                valid.emplace(content.substr(pos)); break;
                            }
                            valid.emplace(content.substr(pos, nl - pos));
                            pos = nl + 1;
                        }
                        arb_rules.emplace(std::string(kv.substr(0, kv.size() - children_suffix.size())), std::move(valid));
                    }
                }
            }
        }

        std::unordered_set<std::string> stale_subtrees;
        for (const auto& [key, value] : snapshot.entries) {
            const auto slash = key.rfind('/');
            if (slash == std::string::npos) continue;
            const std::string parent(key.substr(0, slash));
            const std::string last_seg(key.substr(slash + 1));
            if (last_seg == "__count__" || last_seg == "__children__") continue;

            if (const auto it = seq_rules.find(parent); it != seq_rules.end()) {
                std::int64_t num;
                const auto [ptr, ec] = std::from_chars(last_seg.data(), last_seg.data() + last_seg.size(), num);
                if (ec == std::errc{} && ptr == last_seg.data() + last_seg.size() && num >= it->second)
                    stale_subtrees.insert(key);
            } else if (const auto it2 = arb_rules.find(parent); it2 != arb_rules.end()) {
                if (!it2->second.count(last_seg))
                    stale_subtrees.insert(key);
            }
        }

        for (const auto& [key, value] : snapshot.entries) {
            bool stale = stale_subtrees.count(key) > 0;
            if (!stale) {
                std::string_view sv = key;
                while (!stale) {
                    const auto s = sv.rfind('/');
                    if (s == std::string_view::npos) break;
                    sv = sv.substr(0, s);
                    stale = stale_subtrees.count(std::string(sv)) > 0;
                }
            }
            if (stale) continue;

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
        // Not critical
    }

    return true;
}

// ── compact_and_remap_sources_for_path ───────────────────────────────────────

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

// ── compact ──────────────────────────────────────────────────────────────────

Status Store::compact(std::string_view dataset_id) {
    std::unique_lock<detail::FileLockMutex> sources_guard(sources_mutex_);

    if (dataset_id.empty()) {
        std::unordered_set<std::string> processed_paths;

        for (const Source& source : sources_) {
            if (!source.file_lock) {
                return last_status_ = Status::file_write_error;
            }

            if (!processed_paths.insert(source.file_path).second) {
                continue;
            }

            std::unique_lock<detail::FileLockMutex> write_guard(*source.file_lock);
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

    std::unique_lock<detail::FileLockMutex> write_guard(*source->file_lock);
    if (!compact_and_remap_sources_for_path(source->file_path)) {
        return last_status_ = Status::file_write_error;
    }

    return last_status_ = Status::ok;
}

}  // namespace akasha
