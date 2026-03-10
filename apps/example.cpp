#include <fstream>
#include <iostream>

#include <flatbuffers/flexbuffers.h>

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

    flexbuffers::Builder builder;
    const std::size_t root = builder.StartMap();
    const std::size_t core = builder.StartMap("core");
    builder.Int("timeout", 30);
    builder.UInt("max_request_id", static_cast<std::uint64_t>(9223372036854775810ull));
    const std::size_t settings = builder.StartMap("settings");
    builder.Int("retries", 78);
    builder.Bool("enabled", true);
    builder.EndMap(settings);
    builder.EndMap(core);

    const std::size_t service = builder.StartMap("service");
    builder.String("host", "localhost");
    builder.EndMap(service);
    builder.EndMap(root);
    builder.Finish();

    const std::string config_path = "build/example_config.flexbuf";
    {
        std::ofstream output{config_path, std::ios::binary | std::ios::trunc};
        if (!output.is_open()) {
            std::cerr << "failed to create FlexBuffers file: " << config_path << '\n';
            return 1;
        }

        const auto& serialized = builder.GetBuffer();
        output.write(
            reinterpret_cast<const char*>(serialized.data()),
            static_cast<std::streamsize>(serialized.size())
        );
        if (!output.good()) {
            std::cerr << "failed to write FlexBuffers data to: " << config_path << '\n';
            return 1;
        }
    }

    const akasha::LoadStatus status = store.load_flexbuffer_file(config_path);
    if (status != akasha::LoadStatus::ok) {
        std::cerr << "FlexBuffers load failed with status: " << static_cast<int>(status) << '\n';
        return 1;
    }

    std::cout << "akasha version: " << akasha::version() << '\n';
    std::cout << "loaded from: " << config_path << '\n';
    std::cout << "has core.timeout: " << std::boolalpha << store.has("core.timeout") << '\n';

    if (const auto timeout_entry = store.get("core.settings.retries"); timeout_entry.has_value()) {
        if (const auto* timeout_value = std::get_if<akasha::ValueView>(&*timeout_entry)) {
            print_value("core.settings.retries", *timeout_value);
        }
    }

    if (const auto max_request_id_entry = store.get("core.max_request_id"); max_request_id_entry.has_value()) {
        if (const auto* max_request_id_value = std::get_if<akasha::ValueView>(&*max_request_id_entry)) {
            print_value("core.max_request_id", *max_request_id_value);
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
