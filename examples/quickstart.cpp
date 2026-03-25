#include <iostream>
#include "akasha.hpp"

int main() {
    akasha::Store store;

    auto tmp_status = store.load("config", "/tmp/config.data", akasha::FileOptions::create_if_missing | akasha::FileOptions::migrate_if_incompatible);
    if (tmp_status != akasha::Status::ok) {
        std::cerr << "Error loading config: " << static_cast<int>(tmp_status) << '\n';
        return 1;
    }
    (void)store.unload("config");
    
    // Load a file (created if it doesn't exist)
    auto status = store.load("config", "/tmp/myconfig.db", akasha::FileOptions::create_if_missing | akasha::FileOptions::migrate_if_incompatible);
    if (status != akasha::Status::ok) {
        std::cerr << "Error loading config: " << static_cast<int>(status) << '\n';
        return 1;
    }
    
    // Write values
    status = store.set<int64_t>("config.timeout", 30);
    if (status != akasha::Status::ok) {
        std::cerr << "Error setting timeout\n";
        return 1;
    }
    
    status = store.set<bool>("config.debug", true);
    if (status != akasha::Status::ok) {
        std::cerr << "Error setting debug\n";
        return 1;
    }
    
    status = store.set<std::string>("config.name", "MyApp");
    if (status != akasha::Status::ok) {
        std::cerr << "Error setting name\n";
        return 1;
    }
    
    // Read values
    auto timeout = store.get<int64_t>("config.timeout");
    if (timeout.has_value()) {
        std::cout << "Timeout: " << timeout.value() << " seconds\n";
    }
    
    auto debug = store.get<bool>("config.debug");
    if (debug.has_value()) {
        std::cout << "Debug: " << (debug.value() ? "enabled" : "disabled") << '\n';
    }
    
    auto name = store.get<std::string>("config.name");
    if (name.has_value()) {
        std::cout << "App name: " << name.value() << '\n';
    }
    
    // Get or set default
    auto max_retries = store.getorset<int64_t>("config.max_retries", 5);
    if (max_retries.has_value()) {
        std::cout << "Max retries: " << max_retries.value() << '\n';
    }
    
    // Unload the dataset when done
    std::cout << "\nUnloading dataset...\n";
    status = store.unload("config");
    if (status == akasha::Status::ok) {
        std::cout << "✓ Dataset unloaded\n";
    }
    
    return 0;
}
