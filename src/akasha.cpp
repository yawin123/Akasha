#include "akasha.hpp"

#include <boost/interprocess/offset_ptr.hpp>
#include <flatbuffers/flexbuffers.h>

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
            if (current->value.has_value()) {
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
    }

    root_ = std::move(draft);
    return LoadStatus::ok;
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
