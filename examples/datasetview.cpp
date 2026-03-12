#include <akasha.hpp>
#include <iostream>

using namespace akasha;

int main() {
    Store store;
    
    // Load configuration
    auto status = store.load("config", "/tmp/datasetview_example.db", true);
    if (status != Status::ok) {
        std::cerr << "Error loading config\n";
        return 1;
    }

    // Clear any existing data
    auto clear_status = store.clear("config");
    auto compact_status = store.compact();
    if (clear_status != Status::ok || compact_status != Status::ok) {
        std::cerr << "Error clearing or compacting config\n";
        return 1;
    }
    
    std::cout << "=== Akasha DatasetView Example ===\n\n";
    
    // 1. Set up a hierarchical configuration
    std::cout << "1. Setting up hierarchical configuration:\n";
    (void)store.set<int32_t>("config.server.port", 8080);
    (void)store.set<std::string>("config.server.host", "localhost");
    (void)store.set<bool>("config.server.ssl", true);
    
    (void)store.set<int32_t>("config.database.port", 5432);
    (void)store.set<std::string>("config.database.host", "db.example.com");
    (void)store.set<std::string>("config.database.user", "admin");
    
    (void)store.set<int32_t>("config.cache.ttl", 3600);
    (void)store.set<std::string>("config.cache.backend", "redis");
    
    std::cout << "   ✓ Created config.server.*\n";
    std::cout << "   ✓ Created config.database.*\n";
    std::cout << "   ✓ Created config.cache.*\n\n";
    
    // 2. Get a DatasetView of a subtree
    std::cout << "2. Getting DatasetView of 'config.server':\n";
    auto server_view = store.get<Store::DatasetView>("config.server");
    
    if (server_view) {
        std::cout << "   - View exists: YES\n";
        std::cout << "   - has_value(): " << (server_view->has_value() ? "YES" : "NO") 
                  << " (root node has no direct value)\n";
        std::cout << "   - has_keys(): " << (server_view->has_keys() ? "YES" : "NO") 
                  << " (has children)\n";
        
        auto keys = server_view->keys();
        std::cout << "   - Child keys (" << keys.size() << "):\n";
        for (const auto& key : keys) {
            std::cout << "       • " << key << "\n";
        }
    }
    std::cout << "\n";
    
    // 3. Relative path navigation from DatasetView
    std::cout << "3. Relative path navigation from server view:\n";
    if (server_view) {
        auto port = server_view->template get<int32_t>("port");
        auto host = server_view->template get<std::string>("host");
        auto ssl = server_view->template get<bool>("ssl");
        
        std::cout << "   server.port: " << (port ? std::to_string(*port) : "not found") << "\n";
        std::cout << "   server.host: " << (host ? *host : "not found") << "\n";
        std::cout << "   server.ssl: " << (ssl ? (*ssl ? "true" : "false") : "not found") << "\n";
    }
    std::cout << "\n";
    
    // 4. Copy a subtree to a new location
    std::cout << "4. Copying subtree (backup):\n";
    if (server_view) {
        auto copy_status = store.set<Store::DatasetView>("config.server_backup", *server_view);
        if (copy_status == Status::ok) {
            std::cout << "   ✓ config.server copied to config.server_backup\n";
            
            auto backup = store.get<Store::DatasetView>("config.server_backup");
            if (backup && backup->has_keys()) {
                std::cout << "   ✓ Backup has " << backup->keys().size() << " keys\n";
                
                auto backup_port = backup->template get<int32_t>("port");
                if (backup_port) {
                    std::cout << "   ✓ backup.port = " << *backup_port << "\n";
                }
            }
        }
    }
    std::cout << "\n";
    
    // 5. Introspect different nodes
    std::cout << "5. Introspecting different nodes:\n";
    
    struct NodeInfo {
        std::string path;
    };
    
    std::vector<NodeInfo> nodes = {
        {"config"},
        {"config.server"},
        {"config.server.port"},
        {"config.database"},
        {"config.cache.ttl"}
    };
    
    for (const auto& node : nodes) {
        auto view = store.get<Store::DatasetView>(node.path);
        if (view) {
            std::cout << "   " << node.path << ":\n";
            std::cout << "      has_value=" << (view->has_value() ? "YES" : "NO") << ", ";
            std::cout << "has_keys=" << (view->has_keys() ? "YES" : "NO");
            if (view->has_keys()) {
                std::cout << " (keys: " << view->keys().size() << ")";
            }
            std::cout << "\n";
        }
    }
    std::cout << "\n";
    
    // 6. Modify through DatasetView and verify changes
    std::cout << "6. Modifying and verifying changes:\n";
    (void)store.set<int32_t>("config.server.port", 9000);
    
    auto updated_server = store.get<Store::DatasetView>("config.server");
    if (updated_server) {
        auto new_port = updated_server->template get<int32_t>("port");
        std::cout << "   After changing port to 9000:\n";
        std::cout << "   server.port = " << (new_port ? std::to_string(*new_port) : "not found") << "\n";
    }
    std::cout << "\n";
    
    // 7. Clone entire configuration
    std::cout << "7. Cloning entire configuration:\n";
    
    // First, load the destination dataset
    auto clone_load_status = store.load("config_clone", "/tmp/datasetview_clone.db", true);
    if (clone_load_status != Status::ok) {
        std::cout << "   ERROR: Could not load clone dataset\n";
    } else {
        auto root_view = store.get<Store::DatasetView>("config");
        if (root_view) {
            if (root_view->has_keys()) {
                auto clone_status = store.set<Store::DatasetView>("config_clone", *root_view);
                if (clone_status == Status::ok) {
                    std::cout << "   ✓ Full config cloned to config_clone\n";
                    
                    auto cloned_root = store.get<Store::DatasetView>("config_clone");
                    if (cloned_root) {
                        std::cout << "   ✓ Clone has " << cloned_root->keys().size() 
                                  << " root keys\n";
                        
                        // Verify nested structure exists
                        auto cloned_server = store.get<Store::DatasetView>("config_clone.server");
                        if (cloned_server && cloned_server->has_keys()) {
                            std::cout << "   ✓ Nested structure preserved: "
                                      << "config_clone.server has " 
                                      << cloned_server->keys().size() << " keys\n";
                        }
                    }
                }
            }
        }
    }
    std::cout << "\n";
    
    // 8. Use getorset<DatasetView> for lazy initialization with defaults
    std::cout << "8. Using getorset<DatasetView> with defaults:\n";
    
    // First, create a defaults dataset
    auto defaults_status = store.load("defaults", "/tmp/datasetview_defaults.db", true);
    if (defaults_status == Status::ok) {
        // Set default values for a new service
        (void)store.set<int32_t>("defaults.cache.ttl", 1800);
        (void)store.set<std::string>("defaults.cache.backend", "memcached");
        
        // Get the defaults view
        auto defaults_cache = store.get<Store::DatasetView>("defaults.cache");
        
        if (defaults_cache) {
            // Now, use getorset to get or initialize a production cache config
            // If config.production.cache doesn't exist, use the defaults
            auto prod_cache = store.getorset<Store::DatasetView>(
                "config.production.cache",
                *defaults_cache
            );
            
            if (prod_cache) {
                std::cout << "   ✓ config.production.cache initialized\n";
                std::cout << "   ✓ TTL (from defaults): " 
                          << prod_cache->template get<int32_t>("ttl").value_or(0) << "\n";
                std::cout << "   ✓ Backend (from defaults): " 
                          << prod_cache->template get<std::string>("backend").value_or("") << "\n";
                
                // Second call to getorset should return existing (not copy over defaults again)
                auto prod_cache_again = store.getorset<Store::DatasetView>(
                    "config.production.cache",
                    *defaults_cache
                );
                if (prod_cache_again && prod_cache_again->has_keys()) {
                    std::cout << "   ✓ Second getorset returned existing config\n";
                }
            }
        }
    }
    
    std::cout << "\n=== DatasetView Example Complete ===\n";
    
    return 0;
}
