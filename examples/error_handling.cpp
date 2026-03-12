#include <iostream>
#include "akasha.hpp"

void print_status(akasha::Status status) {
    switch (status) {
        case akasha::Status::ok:
            std::cout << "✓ OK\n";
            break;
        case akasha::Status::invalid_key_path:
            std::cout << "✗ Invalid key path (format: dataset.key)\n";
            break;
        case akasha::Status::key_conflict:
            std::cout << "✗ Key conflict (hierarchy mismatch)\n";
            break;
        case akasha::Status::file_read_error:
            std::cout << "✗ File read error\n";
            break;
        case akasha::Status::file_write_error:
            std::cout << "✗ File write error\n";
            break;
        case akasha::Status::file_not_found:
            std::cout << "✗ File not found\n";
            break;
        case akasha::Status::file_full:
            std::cout << "✗ File full (growth limit exceeded)\n";
            break;
        case akasha::Status::parse_error:
            std::cout << "✗ Parse error (corrupted data)\n";
            break;
        case akasha::Status::dataset_not_found:
            std::cout << "✗ Dataset not found\n";
            break;
        case akasha::Status::source_already_loaded:
            std::cout << "✗ Source already loaded\n";
            break;
    }
}

int main() {
    akasha::Store store;
    
    // Try to load a non-existent file without create_if_missing
    std::cout << "Loading non-existent file without create flag:\n";
    auto status = store.load("config", "/tmp/nonexistent.db", false);
    print_status(status);
    std::cout << "Last status: ";
    print_status(store.last_status());
    
    // Load correctly
    std::cout << "\nLoading (with create flag):\n";
    status = store.load("config", "/tmp/error_test.db", true);
    print_status(status);
    
    // Invalid key (no dataset)
    std::cout << "\nSetting with invalid key path:\n";
    status = store.set<int64_t>("invalid_key", 42);
    print_status(status);
    std::cout << "Last status: ";
    print_status(store.last_status());
    
    // Successful operation
    std::cout << "\nSetting valid key:\n";
    status = store.set<int64_t>("config.timeout", 30);
    print_status(status);
    std::cout << "Last status: ";
    print_status(store.last_status());
    
    // Try to load same dataset twice
    std::cout << "\nLoading same dataset twice:\n";
    status = store.load("config", "/tmp/another.db", true);
    print_status(status);
    std::cout << "Last status: ";
    print_status(store.last_status());
    
    return 0;
}
