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

// INTERNAL: Tipo Value solo para uso interno (no expuesto al usuario)
using InternalValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

// Segment manager para el archivo mapeado
using SegmentManager = bip::managed_mapped_file::segment_manager;

// Allocator para caracteres sobre la región mapeada
using CharAllocator = bip::allocator<char, SegmentManager>;

// String que vive en el archivo mapeado
using InterprocessString = bip::basic_string<char, std::char_traits<char>, CharAllocator>;

// Valor = simplemente InterprocessString (bytes crudos)
using InterprocessValue = InterprocessString;

// Comparador para InterprocessString
struct InterprocessStringLess {
    bool operator()(const InterprocessString& a, const InterprocessString& b) const {
        return std::string_view(a.c_str(), a.size()) < std::string_view(b.c_str(), b.size());
    }
};

// Map que vive en el archivo mapeado (flat key-value por dataset)
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

// Helper para convertir void* a InterprocessDatasetMap*
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

LoadStatus Store::load(std::string_view source_id, std::string_view file_path, bool create_if_missing) {
    if (source_id.empty()) {
        return LoadStatus::invalid_key_path;
    }

    if (file_path.empty()) {
        return LoadStatus::file_read_error;
    }

    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    // Validar que no existe
    if (find_source(source_id) != nullptr) {
        return LoadStatus::source_already_loaded;
    }

    const std::string path{file_path};
    const std::size_t initial_size = initial_mapped_file_size_.load(std::memory_order_relaxed);

    const auto file_lock = get_or_create_file_lock(path);
    std::unique_lock<std::shared_mutex> write_guard(*file_lock);

    try {
        auto storage = std::make_shared<MappedFileStorage>(path, initial_size);

        // Intentar encontrar o crear el map del dataset en el managed_mapped_file
        auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
            kDatasetMapName
        )(storage->file.get_segment_manager());

        if (!dataset_map) {
            return LoadStatus::file_read_error;
        }

        // Crear la Source y almacenarla
        Source new_source;
        new_source.id = std::string{source_id};
        new_source.file_path = path;
        new_source.storage = storage;
        new_source.file_lock = file_lock;
        new_source.dataset_map = dataset_map;
        new_source.store = this;

        sources_.push_back(std::move(new_source));
        return LoadStatus::ok;

    } catch (const bip::interprocess_exception& e) {
        if (create_if_missing) {
            // Intentar crear archivo nuevo
            try {
                bip::file_mapping::remove(path.c_str());
                auto storage = std::make_shared<MappedFileStorage>(path, initial_size);

                auto* dataset_map = storage->file.find_or_construct<InterprocessDatasetMap>(
                    kDatasetMapName
                )(storage->file.get_segment_manager());

                if (!dataset_map) {
                    return LoadStatus::file_read_error;
                }

                Source new_source;
                new_source.id = std::string{source_id};
                new_source.file_path = path;
                new_source.storage = storage;
                new_source.file_lock = file_lock;
                new_source.dataset_map = dataset_map;
                new_source.store = this;

                sources_.push_back(std::move(new_source));
                return LoadStatus::ok;
            } catch (...) {
                return LoadStatus::file_read_error;
            }
        }
        return LoadStatus::file_read_error;
    }
}

WriteStatus Store::set_bytes_impl(std::string_view key_path, const void* bytes, std::size_t size) {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.size() < 2) {
        return WriteStatus::invalid_key_path;
    }

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dataset_id = segments.front();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return WriteStatus::dataset_not_found;
    }

    if (!source->file_lock) {
        return WriteStatus::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        return WriteStatus::file_write_error;
    }

    // Construir la clave sin el prefijo del dataset
    std::string key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            key += '.';
        }
        key += segments[i];
    }

    std::size_t grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    const int max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);

    for (int attempt = 0; attempt <= max_grow_retries; ++attempt) {
        try {
            auto* segment_mgr = source->storage->file.get_segment_manager();
            auto* map = as_dataset_map(source->dataset_map);
            auto allocator = segment_mgr->get_allocator<char>();

            // Crear InterprocessString para la clave
            InterprocessString ipc_key(key.c_str(), allocator);

            // Crear InterprocessValue (InterprocessString) con los bytes
            InterprocessValue ipc_value(static_cast<const char*>(bytes), size, allocator);

            // Insertar o reemplazar en el map (escritura directa en mmap)
            auto it = map->find(ipc_key);
            if (it == map->end()) {
                map->emplace(ipc_key, ipc_value);
            } else {
                it->second = ipc_value;
            }

            if (!source->storage->file.flush()) {
                return WriteStatus::file_write_error;
            }

            return WriteStatus::ok;
        } catch (const bip::interprocess_exception&) {
            if (attempt == max_grow_retries) {
                return WriteStatus::file_write_error;
            }

            const std::string source_file_path = source->file_path;
            const std::size_t current_file_size = source->storage->file.get_size();
            const std::size_t base_step = initial_grow_step_.load(std::memory_order_relaxed);
            const std::size_t dynamic_step = std::max(grow_step, std::max(base_step, current_file_size / 2));

            if (!grow_and_remap_sources_for_path(source_file_path, dynamic_step)) {
                return WriteStatus::file_write_error;
            }

            source = find_source(dataset_id);
            if (source == nullptr || !source->storage || !source->dataset_map) {
                return WriteStatus::file_write_error;
            }

            grow_step = dynamic_step * 2;
        }
    }

    return WriteStatus::file_write_error;
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

    // Construir la clave
    std::string key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            key += '.';
        }
        key += segments[i];
    }

    // Buscar clave exacta
    auto* map = as_dataset_map(source.dataset_map);
    auto it = map->find(InterprocessString(key.c_str(), source.storage->file.get_segment_manager()->get_allocator<char>()));
    if (it != map->end()) {
        // it->second es el InterprocessValue (InterprocessString)
        const auto& data_bytes = it->second;
        
        // Retornar los bytes (sin verificar tamaño - responsabilidad del usuario)
        std::vector<char> result(data_bytes.c_str(), data_bytes.c_str() + data_bytes.size());
        return result;
    }

    return std::nullopt;
}

/*
WriteStatus Store::set_internal(std::string_view key_path, const InternalValue& value) {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.size() < 2) {
        return WriteStatus::invalid_key_path;
    }

    std::shared_lock<std::shared_mutex> sources_guard(sources_mutex_);

    const std::string_view dataset_id = segments.front();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return WriteStatus::dataset_not_found;
    }

    if (!source->file_lock) {
        return WriteStatus::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);

    if (!source->dataset_map || !source->storage) {
        return WriteStatus::file_write_error;
    }

    // Construir la clave sin el prefijo del dataset
    std::string key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            key += '.';
        }
        key += segments[i];
    }

    std::size_t grow_step = initial_grow_step_.load(std::memory_order_relaxed);
    const int max_grow_retries = max_grow_retries_.load(std::memory_order_relaxed);

    for (int attempt = 0; attempt <= max_grow_retries; ++attempt) {
        try {
            auto* segment_mgr = source->storage->file.get_segment_manager();
            auto* map = as_dataset_map(source->dataset_map);

            // Crear InterprocessString para la clave
            InterprocessString ipc_key(key.c_str(), segment_mgr->get_allocator<char>());

            // Insertar o reemplazar en el map (escritura directa en mmap)
            auto it = map->find(ipc_key);
            if (it == map->end()) {
                map->emplace(ipc_key, InterprocessValue(value, segment_mgr));
            } else {
                InterprocessValue next_value(value, segment_mgr);
                InterprocessValue previous_value = it->second;
                it->second = next_value;
                previous_value.destroy(segment_mgr);
            }

            if (!source->storage->file.flush()) {
                return WriteStatus::file_write_error;
            }

            return WriteStatus::ok;
        } catch (const bip::interprocess_exception&) {
            if (attempt == max_grow_retries) {
                return WriteStatus::file_write_error;
            }

            const std::string source_file_path = source->file_path;
            const std::size_t current_file_size = source->storage->file.get_size();
            const std::size_t base_step = initial_grow_step_.load(std::memory_order_relaxed);
            const std::size_t dynamic_step = std::max(grow_step, std::max(base_step, current_file_size / 2));

            if (!grow_and_remap_sources_for_path(source_file_path, dynamic_step)) {
                return WriteStatus::file_write_error;
            }

            source = find_source(dataset_id);
            if (source == nullptr || !source->storage || !source->dataset_map) {
                return WriteStatus::file_write_error;
            }

            grow_step = dynamic_step * 2;
        }
    }

    return WriteStatus::file_write_error;
}
*/

WriteStatus Store::clear(std::string_view key_path) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    if (key_path.empty()) {
        std::unordered_set<std::string> processed_paths;

        for (const Source& source : sources_) {
            if (!source.storage || !source.dataset_map || !source.file_lock) {
                return WriteStatus::file_write_error;
            }

            if (!processed_paths.insert(source.file_path).second) {
                continue;
            }

            std::unique_lock<std::shared_mutex> write_guard(*source.file_lock);
            auto* map = as_dataset_map(source.dataset_map);
            map->clear();

            if (!source.storage->file.flush()) {
                return WriteStatus::file_write_error;
            }

            if (!shrink_and_remap_sources_for_path(source.file_path)) {
                return WriteStatus::file_write_error;
            }
        }

        return WriteStatus::ok;
    }

    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments) || segments.empty()) {
        return WriteStatus::invalid_key_path;
    }

    const std::string_view dataset_id = segments.front();
    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return WriteStatus::dataset_not_found;
    }

    if (!source->storage || !source->dataset_map || !source->file_lock) {
        return WriteStatus::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);
    auto* map = as_dataset_map(source->dataset_map);

    if (segments.size() == 1) {
        map->clear();
        if (!source->storage->file.flush()) {
            return WriteStatus::file_write_error;
        }

        if (!shrink_and_remap_sources_for_path(source->file_path)) {
            return WriteStatus::file_write_error;
        }

        return WriteStatus::ok;
    }

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
        return WriteStatus::file_write_error;
    }

    if (map->empty()) {
        if (!shrink_and_remap_sources_for_path(source->file_path)) {
            return WriteStatus::file_write_error;
        }
    }

    return WriteStatus::ok;
}

WriteStatus Store::compact(std::string_view dataset_id) {
    std::unique_lock<std::shared_mutex> sources_guard(sources_mutex_);

    if (dataset_id.empty()) {
        std::unordered_set<std::string> processed_paths;

        for (const Source& source : sources_) {
            if (!source.file_lock) {
                return WriteStatus::file_write_error;
            }

            if (!processed_paths.insert(source.file_path).second) {
                continue;
            }

            std::unique_lock<std::shared_mutex> write_guard(*source.file_lock);
            if (!compact_and_remap_sources_for_path(source.file_path)) {
                return WriteStatus::file_write_error;
            }
        }

        return WriteStatus::ok;
    }

    Source* source = find_source(dataset_id);
    if (source == nullptr) {
        return WriteStatus::dataset_not_found;
    }

    if (!source->file_lock) {
        return WriteStatus::file_write_error;
    }

    std::unique_lock<std::shared_mutex> write_guard(*source->file_lock);
    if (!compact_and_remap_sources_for_path(source->file_path)) {
        return WriteStatus::file_write_error;
    }

    return WriteStatus::ok;
}

bool Store::has(std::string_view key_path) const {
    return get(key_path).has_value();
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

    // Caso 1: Solo dataset (sin clave específica) -> retornar DatasetView
    if (segments.size() == 1) {
        return DatasetView{&source};
    }

    // Caso 2: Clave específica -> buscar en el map
    std::string key;
    for (std::size_t i = 1; i < segments.size(); ++i) {
        if (i > 1) {
            key += '.';
        }
        key += segments[i];
    }

    // Buscar clave exacta
    auto* map = as_dataset_map(source.dataset_map);
    auto it = map->find(InterprocessString(key.c_str(), source.storage->file.get_segment_manager()->get_allocator<char>()));
    if (it != map->end()) {
        // Encontrado como valor -> no hay View, solo bytes
        return std::nullopt;
    }

    // Caso 3: Puede ser un prefijo (subárbol) -> verificar si hay claves con este prefijo
    std::string prefix = key + ".";
    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        if (full_key.starts_with(prefix) || full_key == key) {
            // Hay al menos una clave con este prefijo -> es un sub<árbol
            return DatasetView{&source, key};
        }
    }

    return std::nullopt;
}

bool Store::DatasetView::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

// TODO: Implementar get_dataset_view() para DatasetView si es necesario
/*
std::optional<akasha::Store::DatasetView> Store::DatasetView::get_dataset_view(std::string_view key_path) const {
    if (source_ == nullptr || !source_->file_lock) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> read_guard(*source_->file_lock);

    if (!source_->dataset_map || !source_->storage) {
        return std::nullopt;
    }

    auto* map = as_dataset_map(source_->dataset_map);

    // Construir clave completa: prefix_ + key_path
    std::string full_key;
    if (!prefix_.empty()) {
        full_key = prefix_ + ".";
    }
    full_key += key_path;

    // Buscar clave exacta
    auto it = map->find(InterprocessString(full_key.c_str(), source_->storage->file.get_segment_manager()->get_allocator<char>()));
    if (it != map->end()) {
        // Encontrado como valor -> no hay View, solo bytes
        return std::nullopt;
    }

    // Verificar si es un prefijo (subárbol)
    std::string prefix = full_key + ".";
    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view k(ipc_key.c_str(), ipc_key.size());
        if (k.starts_with(prefix) || k == full_key) {
            return DatasetView{source_, full_key};
        }
    }

    return std::nullopt;
}
*/

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

    // Encontrar todas las claves bajo este prefijo
    std::string prefix = prefix_.empty() ? "" : prefix_ + ".";
    std::size_t prefix_len = prefix.size();

    for (const auto& [ipc_key, ipc_value] : *map) {
        std::string_view full_key(ipc_key.c_str(), ipc_key.size());
        
        if (prefix.empty() || full_key.starts_with(prefix)) {
            std::string_view relative_key = full_key.substr(prefix_len);
            
            // Solo incluir claves de primer nivel (sin puntos adicionales)
            std::size_t dot_pos = relative_key.find('.');
            if (dot_pos == std::string_view::npos) {
                // Es una clave directa
                result.emplace_back(relative_key);
            } else {
                // Es una subclave, agregar solo el prefijo si no está ya
                std::string first_segment(relative_key.substr(0, dot_pos));
                if (std::find(result.begin(), result.end(), first_segment) == result.end()) {
                    result.push_back(std::move(first_segment));
                }
            }
        }
    }

    return result;
}

}  // namespace akasha
