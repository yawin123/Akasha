#include "akasha.hpp"
#include <iostream>

/**
 * Demonstrates hierarchical navigation and introspection capabilities.
 * 
 * Questions answered:
 * - Is there a way to know what a dataset contains?
 * - Are there nested data structures?
 * - has() to check existence
 * - keys() to list contents
 */

// Helper to print a value (tries multiple types)
void print_value(akasha::Store& store, const std::string& full_key) {
	// Try string first (most common)
	auto val_str = store.get<std::string>(full_key);
	if (val_str.has_value()) {
		std::cout << " = \"" << val_str.value() << "\"";
		return;
	}
	
	// Then try numeric types
	auto val_int = store.get<int64_t>(full_key);
	if (val_int.has_value()) {
		std::cout << " = " << val_int.value();
		return;
	}
	
	auto val_bool = store.get<bool>(full_key);
	if (val_bool.has_value()) {
		std::cout << " = " << (val_bool.value() ? "true" : "false");
		return;
	}
	
	auto val_double = store.get<double>(full_key);
	if (val_double.has_value()) {
		std::cout << " = " << val_double.value();
		return;
	}
	
	std::cout << " = ???";
}

int main() {
	akasha::Store store;
	
	std::cout << "=== Navigation and Introspection Demo ===\n\n";
	
	// Load and create nested data structure
	std::string file_path = "/tmp/akasha_navigation.db";
	auto status = store.load("settings", file_path, true);
	if (status != akasha::Status::ok) {
		std::cerr << "Failed to load settings\n";
		return 1;
	}
	
	// Create nested structure:
	// settings
	//   ├─ server
	//   │  ├─ host
	//   │  ├─ port
	//   │  └─ timeout
	//   └─ database
	//      ├─ host
	//      ├─ port
	//      └─ pool_size
	
	std::cout << "Creating nested structure:\n";
	auto s1 = store.set<std::string>("settings.server.host", "localhost");
	auto s2 = store.set<int64_t>("settings.server.port", 8080);
	auto s3 = store.set<int64_t>("settings.server.timeout", 30);
	
	auto s4 = store.set<std::string>("settings.database.host", "db.example.com");
	auto s5 = store.set<int64_t>("settings.database.port", 5432);
	auto s6 = store.set<int64_t>("settings.database.pool_size", 10);
	
	bool all_ok = (s1 == akasha::Status::ok && s2 == akasha::Status::ok && 
	               s3 == akasha::Status::ok && s4 == akasha::Status::ok &&
	               s5 == akasha::Status::ok && s6 == akasha::Status::ok);
	
	if (all_ok) {
		std::cout << "✓ Nested data created\n\n";
	} else {
		std::cerr << "✗ Failed to create nested data\n";
		std::cerr << "  Last status: " << (int)store.last_status() << "\n";
		return 1;
	}
	
	// Check existence with has()
	std::cout << "=== Checking existence with has() ===\n";
	std::cout << "  has(\"settings.server.host\") = " 
		<< (store.has("settings.server.host") ? "true" : "false") << "\n";
	std::cout << "  has(\"settings.server.port\") = " 
		<< (store.has("settings.server.port") ? "true" : "false") << "\n";
	std::cout << "  has(\"settings.cache.key\") = " 
		<< (store.has("settings.cache.key") ? "true" : "false") << "\n\n";
	
	// Read values with get() - direct navigation
	std::cout << "=== Reading with get() ===\n\n";
	
	std::cout << "Server configuration:\n";
	auto host = store.get<std::string>("settings.server.host");
	auto port = store.get<int64_t>("settings.server.port");
	auto timeout = store.get<int64_t>("settings.server.timeout");
	
	std::cout << "  host     = " << host.value_or("N/A") << "\n";
	std::cout << "  port     = " << port.value_or(0) << "\n";
	std::cout << "  timeout  = " << timeout.value_or(0) << "\n\n";
	
	std::cout << "Database configuration:\n";
	auto db_host = store.get<std::string>("settings.database.host");
	auto db_port = store.get<int64_t>("settings.database.port");
	auto pool_size = store.get<int64_t>("settings.database.pool_size");
	
	std::cout << "  host      = " << db_host.value_or("N/A") << "\n";
	std::cout << "  port      = " << db_port.value_or(0) << "\n";
	std::cout << "  pool_size = " << pool_size.value_or(0) << "\n\n";
	
	// List keys with DatasetView::keys()
	std::cout << "=== Listing keys with DatasetView::keys() ===\n\n";
	
	// See what's in each branch using a view
	auto server_view = store.get<akasha::Store::DatasetView>("settings.server");
	if (server_view.has_value()) {
		auto server_keys = server_view->keys();
		std::cout << "Keys in 'settings.server':\n";
		for (const auto& key : server_keys) {
			std::cout << "  - " << key << "\n";
		}
		std::cout << "\n";
	}
	
	auto database_view = store.get<akasha::Store::DatasetView>("settings.database");
	if (database_view.has_value()) {
		auto db_keys = database_view->keys();
		std::cout << "Keys in 'settings.database':\n";
		for (const auto& key : db_keys) {
			std::cout << "  - " << key << "\n";
		}
		std::cout << "\n";
	}
	
	// Display complete dataset contents
	std::cout << "=== Complete dataset contents ===\n\n";
	
	auto root_view = store.get<akasha::Store::DatasetView>("settings");
	if (root_view.has_value()) {
		auto root_keys = root_view->keys();
		std::cout << "Dataset 'settings' contains:\n";
		
		for (const auto& branch : root_keys) {
			std::cout << "  ├─ " << branch << "\n";
			
			// Get content of each branch
			auto branch_view = store.get<akasha::Store::DatasetView>(
				std::string("settings.") + branch
			);
			if (branch_view.has_value()) {
				auto branch_keys = branch_view->keys();
				for (std::size_t i = 0; i < branch_keys.size(); ++i) {
					const auto& is_last = (i == branch_keys.size() - 1);
					const auto& key = branch_keys[i];
					std::cout << "  │  " << (is_last ? "└─" : "├─") << " " << key;
					print_value(store, std::string("settings.") + branch + "." + key);
					std::cout << "\n";
				}
			}
		}
		std::cout << "\n";
	}
	
	return 0;
}
