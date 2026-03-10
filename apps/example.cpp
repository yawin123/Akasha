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

    // ========== Crear archivo de configuración "defaults" ==========
    flexbuffers::Builder default_builder;
    {
        const std::size_t root = default_builder.StartMap();
        const std::size_t core = default_builder.StartMap("core");
        default_builder.Int("timeout", 30);
        default_builder.UInt("max_request_id", static_cast<std::uint64_t>(1000ull));
        const std::size_t settings = default_builder.StartMap("settings");
        default_builder.Int("retries", 3);
        default_builder.Bool("enabled", true);
        default_builder.EndMap(settings);
        default_builder.EndMap(core);

        const std::size_t service = default_builder.StartMap("service");
        default_builder.String("host", "localhost");
        default_builder.Int("port", 8080);
        default_builder.EndMap(service);
        default_builder.EndMap(root);
        default_builder.Finish();
    }

    const std::string defaults_path = "build/defaults.flexbuf";
    {
        std::ofstream output{defaults_path, std::ios::binary | std::ios::trunc};
        if (!output.is_open()) {
            std::cerr << "failed to create FlexBuffers file: " << defaults_path << '\n';
            return 1;
        }

        const auto& serialized = default_builder.GetBuffer();
        output.write(
            reinterpret_cast<const char*>(serialized.data()),
            static_cast<std::streamsize>(serialized.size())
        );
        if (!output.good()) {
            std::cerr << "failed to write FlexBuffers data to: " << defaults_path << '\n';
            return 1;
        }
    }

    // ========== Crear archivo de configuración "user" que sobrescribe algunos valores ==========
    flexbuffers::Builder user_builder;
    {
        const std::size_t root = user_builder.StartMap();
        const std::size_t core = user_builder.StartMap("core");
        user_builder.Int("timeout", 60);  // sobrescribe defaults
        user_builder.UInt("max_request_id", static_cast<std::uint64_t>(9223372036854775810ull));  // sobrescribe defaults
        const std::size_t settings = user_builder.StartMap("settings");
        user_builder.Int("retries", 78);  // sobrescribe defaults
        // enabled no está en user, debería venir de defaults
        user_builder.EndMap(settings);
        user_builder.EndMap(core);

        const std::size_t service = user_builder.StartMap("service");
        user_builder.String("host", "production.example.com");  // sobrescribe defaults
        // port no está en user, debería venir de defaults
        user_builder.EndMap(service);
        user_builder.EndMap(root);
        user_builder.Finish();
    }

    const std::string user_path = "build/user.flexbuf";
    {
        std::ofstream output{user_path, std::ios::binary | std::ios::trunc};
        if (!output.is_open()) {
            std::cerr << "failed to create FlexBuffers file: " << user_path << '\n';
            return 1;
        }

        const auto& serialized = user_builder.GetBuffer();
        output.write(
            reinterpret_cast<const char*>(serialized.data()),
            static_cast<std::streamsize>(serialized.size())
        );
        if (!output.good()) {
            std::cerr << "failed to write FlexBuffers data to: " << user_path << '\n';
            return 1;
        }
    }

    // ========== Cargar ambas fuentes: defaults primero, user después (último gana) ==========
    {
        const akasha::LoadStatus status = store.load("defaults", defaults_path);
        if (status != akasha::LoadStatus::ok) {
            std::cerr << "FlexBuffers load failed for defaults with status: " << static_cast<int>(status) << '\n';
            return 1;
        }
    }

    {
        const akasha::LoadStatus status = store.load("user", user_path);
        if (status != akasha::LoadStatus::ok) {
            std::cerr << "FlexBuffers load failed for user with status: " << static_cast<int>(status) << '\n';
            return 1;
        }
    }

    // ========== Intentar cargar la misma fuente de nuevo (debe fallar) ==========
    {
        const akasha::LoadStatus status = store.load("user", user_path);
        if (status == akasha::LoadStatus::source_already_loaded) {
            std::cout << "\n[EXPECTED] Intento de recargar 'user': source_already_loaded\n";
        } else {
            std::cerr << "ERROR: Se esperaba source_already_loaded pero se obtuvo: " << static_cast<int>(status) << '\n';
            return 1;
        }
    }

    // ========== Intentar cargar archivo que no existe sin create_if_missing (debe fallar) ==========
    {
        const akasha::LoadStatus status = store.load("nonexistent", "build/does_not_exist.flexbuf", false);
        if (status == akasha::LoadStatus::file_read_error) {
            std::cout << "[EXPECTED] Intento de cargar archivo inexistente con create_if_missing=false: file_read_error\n";
        } else {
            std::cerr << "ERROR: Se esperaba file_read_error pero se obtuvo: " << static_cast<int>(status) << '\n';
            return 1;
        }
    }

    // ========== Cargar archivo que no existe con create_if_missing=true (debe crear vacío) ==========
    {
        const akasha::LoadStatus status = store.load("auto_created", "build/auto_created.flexbuf", true);
        if (status == akasha::LoadStatus::ok) {
            std::cout << "[EXPECTED] Archivo 'auto_created' no existía, se creó vacío: ok\n";
            if (const auto entry = store.get("auto_created.anything"); !entry.has_value()) {
                std::cout << "[EXPECTED] Archivo vacío no contiene claves: ok\n";
            }
        } else {
            std::cerr << "ERROR: Se esperaba ok pero se obtuvo: " << static_cast<int>(status) << '\n';
            return 1;
        }
    }

    std::cout << "akasha version: " << akasha::version() << '\n';
    std::cout << "loaded defaults from: " << defaults_path << '\n';
    std::cout << "loaded user overrides from: " << user_path << '\n';
    std::cout << '\n';

    // ========== Consultas explícitas por dataset ==========
    std::cout << "=== Dataset-qualified queries ===\n";

    // timeout en dataset user
    if (const auto entry = store.get("user.core.timeout"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("user.core.timeout", *value);
        }
    }

    // max_request_id en dataset user
    if (const auto entry = store.get("user.core.max_request_id"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("user.core.max_request_id", *value);
        }
    }

    // retries en dataset user
    if (const auto entry = store.get("user.core.settings.retries"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("user.core.settings.retries", *value);
        }
    }

    // enabled en dataset defaults
    if (const auto entry = store.get("defaults.core.settings.enabled"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("defaults.core.settings.enabled", *value);
        }
    }

    // host en dataset user
    if (const auto entry = store.get("user.service.host"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("user.service.host", *value);
        }
    }

    // port en dataset defaults
    if (const auto entry = store.get("defaults.service.port"); entry.has_value()) {
        if (const auto* value = std::get_if<akasha::ValueView>(&*entry)) {
            print_value("defaults.service.port", *value);
        }
    }

    if (!store.get("core.timeout").has_value()) {
        std::cout << "[EXPECTED] core.timeout -> nullopt (dataset no calificado)\n";
    }

    std::cout << '\n';

    // Navegar por DatasetView dentro de un dataset concreto
    if (const auto settings_entry = store.get("user.core.settings"); settings_entry.has_value()) {
        if (const auto* settings_view = std::get_if<akasha::Store::DatasetView>(&*settings_entry)) {
            std::cout << "user.core.settings contains:\n";
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
