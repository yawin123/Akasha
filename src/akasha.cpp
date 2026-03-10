#include "akasha.hpp"

#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <flatbuffers/flexbuffers.h>

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using AkashaInterprocessTag [[maybe_unused]] = boost::interprocess::offset_ptr<void>;
using AkashaFlexBuilder [[maybe_unused]] = flexbuffers::Builder;

[[nodiscard]] std::optional<akasha::ValueView> make_value_view(const akasha::Value& value) {
    return std::visit(
        [](const auto& typed_value) -> akasha::ValueView {
            using TypedValue = std::decay_t<decltype(typed_value)>;
            if constexpr (std::is_same_v<TypedValue, std::string>) {
                return std::string_view{typed_value};
            } else {
                return typed_value;
            }
        },
        value
    );
}

}  // namespace

namespace akasha {

struct Store::MappedFileStorage {
    explicit MappedFileStorage(const std::string& path)
        : mapping(path.c_str(), boost::interprocess::read_only),
          region(mapping, boost::interprocess::read_only) {
    }

    boost::interprocess::file_mapping mapping;
    boost::interprocess::mapped_region region;
};

Store::DatasetView::DatasetView(const Node* node) noexcept : node_{node} {
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

std::optional<Store::QueryResult> Store::get_from_node(const Node& base, std::string_view key_path) {
    std::vector<std::string_view> segments;
    if (!split_key_path(key_path, segments)) {
        return std::nullopt;
    }

    const Node* current = &base;
    for (const std::string_view segment : segments) {
        const auto iterator = current->children.find(std::string{segment});
        if (iterator == current->children.end()) {
            return std::nullopt;
        }
        current = &iterator->second;
    }

    if (current->value.has_value()) {
        return make_value_view(*current->value);
    }

    if (current->mapped_value.has_value()) {
        const MappedValueRef& mapped_value = *current->mapped_value;
        if (!mapped_value.storage || mapped_value.storage->region.get_address() == nullptr) {
            return std::nullopt;
        }

        if (mapped_value.storage->region.get_size() < 3) {
            return std::nullopt;
        }

        const auto* buffer = static_cast<const std::uint8_t*>(mapped_value.storage->region.get_address());
        flexbuffers::Reference reference = flexbuffers::GetRoot(buffer, mapped_value.storage->region.get_size());
        for (const std::string& segment : mapped_value.segments) {
            if (!reference.IsMap()) {
                return std::nullopt;
            }

            reference = reference.AsMap()[segment];
            if (reference.IsNull()) {
                return std::nullopt;
            }
        }

        if (reference.IsBool()) {
            return QueryResult{ValueView{reference.AsBool()}};
        }

        if (reference.IsInt()) {
            return QueryResult{ValueView{reference.AsInt64()}};
        }

        if (reference.IsUInt()) {
            return QueryResult{ValueView{reference.AsUInt64()}};
        }

        if (reference.IsFloat()) {
            return QueryResult{ValueView{reference.AsDouble()}};
        }

        if (reference.IsString()) {
            const flexbuffers::String mapped_string = reference.AsString();
            return QueryResult{ValueView{std::string_view{mapped_string.c_str(), mapped_string.length()}}};
        }

        return std::nullopt;
    }

    return DatasetView{current};
}

LoadStatus Store::load(const DatasetSource& source) {
    Node draft = root_;

    std::vector<std::string_view> segments;
    for (const auto& [key_path, value] : source) {
        if (!split_key_path(key_path, segments) || segments.size() < 2) {
            return LoadStatus::invalid_key_path;
        }

        Node* current = &draft;
        for (const std::string_view segment : segments) {
            if (current->value.has_value() || current->mapped_value.has_value()) {
                return LoadStatus::key_conflict;
            }

            auto [iterator, inserted] = current->children.try_emplace(std::string{segment});
            (void)inserted;
            current = &iterator->second;
        }

        if (!current->children.empty()) {
            return LoadStatus::key_conflict;
        }

        current->value = value;
        current->mapped_value.reset();
    }

    root_ = std::move(draft);
    return LoadStatus::ok;
}

LoadStatus Store::load_flexbuffer_file(std::string_view file_path) {
    if (file_path.empty()) {
        return LoadStatus::file_read_error;
    }

    const std::string path{file_path};

    try {
        auto storage = std::make_shared<MappedFileStorage>(path);

        if (storage->region.get_address() == nullptr) {
            return LoadStatus::file_read_error;
        }

        if (storage->region.get_size() < 3) {
            return LoadStatus::parse_error;
        }

        const auto* buffer = static_cast<const std::uint8_t*>(storage->region.get_address());
        const flexbuffers::Reference root = flexbuffers::GetRoot(buffer, storage->region.get_size());
        if (!root.IsMap()) {
            return LoadStatus::parse_error;
        }

        Node draft = root_;
        std::vector<std::string> path_segments;

        std::function<LoadStatus(Node&, const flexbuffers::Reference&)> ingest_reference;
        ingest_reference = [&](Node& current, const flexbuffers::Reference& reference) -> LoadStatus {
            if (reference.IsMap()) {
                if (current.value.has_value() || current.mapped_value.has_value()) {
                    return LoadStatus::key_conflict;
                }

                const flexbuffers::Map map = reference.AsMap();
                const flexbuffers::TypedVector keys = map.Keys();
                const flexbuffers::Vector values = map.Values();
                for (std::size_t index = 0; index < keys.size(); ++index) {
                    const char* raw_key = keys[index].AsKey();
                    if (raw_key == nullptr || raw_key[0] == '\0') {
                        return LoadStatus::parse_error;
                    }

                    const std::string_view key_view{raw_key};
                    if (key_view.find('.') != std::string_view::npos) {
                        return LoadStatus::invalid_key_path;
                    }

                    path_segments.emplace_back(key_view);
                    auto [iterator, inserted] = current.children.try_emplace(std::string{key_view});
                    (void)inserted;

                    const LoadStatus child_status = ingest_reference(iterator->second, values[index]);
                    path_segments.pop_back();
                    if (child_status != LoadStatus::ok) {
                        return child_status;
                    }
                }

                return LoadStatus::ok;
            }

            if (path_segments.size() < 2) {
                return LoadStatus::invalid_key_path;
            }

            if (!current.children.empty()) {
                return LoadStatus::key_conflict;
            }

            if (!reference.IsBool() && !reference.IsInt() && !reference.IsUInt() && !reference.IsFloat() && !reference.IsString()) {
                return LoadStatus::unsupported_value_type;
            }

            current.value.reset();
            current.mapped_value = MappedValueRef{storage, path_segments};
            return LoadStatus::ok;
        };

        const LoadStatus parse_status = ingest_reference(draft, root);
        if (parse_status != LoadStatus::ok) {
            return parse_status;
        }

        root_ = std::move(draft);
        return LoadStatus::ok;
    } catch (const boost::interprocess::interprocess_exception&) {
        return LoadStatus::file_read_error;
    }
}

bool Store::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

std::optional<Store::QueryResult> Store::get(std::string_view key_path) const {
    return get_from_node(root_, key_path);
}

bool Store::DatasetView::has(std::string_view key_path) const {
    return get(key_path).has_value();
}

std::optional<std::variant<ValueView, Store::DatasetView>> Store::DatasetView::get(std::string_view key_path) const {
    if (node_ == nullptr) {
        return std::nullopt;
    }

    return Store::get_from_node(*node_, key_path);
}

std::vector<std::string> Store::DatasetView::keys() const {
    std::vector<std::string> result;
    if (node_ != nullptr) {
        result.reserve(node_->children.size());
        for (const auto& [key, child] : node_->children) {
            (void)child;
            result.push_back(key);
        }
    }
    return result;
}

}  // namespace akasha
