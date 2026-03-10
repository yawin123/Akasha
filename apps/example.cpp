#include <iostream>

#include "akasha.hpp"

namespace {

void print_value(std::string_view key_path, const akasha::ValueView& value) {
    std::visit(
        [key_path](const auto& typed_value) {
            std::cout << key_path << " = " << typed_value << '\n';
        },
        value
    );
}

}  // namespace

int main() {
    akasha::Store store;
    const akasha::DatasetSource source{
        {"core.timeout", std::int64_t{30}},
        {"core.settings.retries", std::int64_t{5}},
        {"core.settings.enabled", true},
        {"service.host", std::string{"localhost"}},
    };

    const akasha::LoadStatus status = store.load(source);
    if (status != akasha::LoadStatus::ok) {
        std::cerr << "source load failed\n";
        return 1;
    }

    std::cout << "akasha version: " << akasha::version() << '\n';
    std::cout << "has core.timeout: " << std::boolalpha << store.has("core.timeout") << '\n';

    if (const auto timeout_entry = store.get("core.settings.retries"); timeout_entry.has_value()) {
        if (const auto* timeout_value = std::get_if<akasha::ValueView>(&*timeout_entry)) {
            print_value("core.settings.retries", *timeout_value);
        }
    }

    if (const auto settings_entry = store.get("core.settings"); settings_entry.has_value()) {
        if (const auto* settings_view = std::get_if<akasha::Store::DatasetView>(&*settings_entry)) {
            std::cout << "core.settings contains:\n";
            for (const auto& key : settings_view->keys()) {
                if (const auto entry = settings_view->get(key); entry.has_value()) {
                    if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
                        std::cout << "  " << key << " -> ";
                        std::visit([](const auto& v) { std::cout << v; }, *value);
                        std::cout << '\n';
                    } else if (const auto* subview = std::get_if<akasha::Store::DatasetView>(&*entry)) {
                        std::cout << "  " << key << " -> [" << subview->keys().size() << " keys]\n";
                    }
                }
            }
        }
    }

    return 0;
}
