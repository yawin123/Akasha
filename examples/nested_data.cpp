#include "akasha.hpp"
#include <iostream>

/**
 * Demonstrates storage and retrieval of nested data (objects with fields).
 * 
 * In v2.0.0, nested data is achieved through hierarchical key paths.
 * Users can build object structures using DatasetView for navigation.
 */

int main() {
	akasha::Store store;
	
	std::cout << "=== Nested Data Structures Demo ===\n\n";
	
	// Load dataset
	std::string file_path = "/tmp/akasha_nested.db";
	auto status = store.load("app", file_path, akasha::FileOptions::create_if_missing);
	if (status != akasha::Status::ok) {
		std::cerr << "Failed to load dataset\n";
		return 1;
	}
	
	// Create nested data through hierarchical paths
	std::cout << "1. Creating nested user profile with location:\n\n";
	
	// Store alice's data
	if (store.set<std::string>("app/users/alice/name", "Alice") != akasha::Status::ok ||
	    store.set<int64_t>("app/users/alice/age", 30) != akasha::Status::ok ||
	    store.set<double>("app/users/alice/location/latitude", 41.3851) != akasha::Status::ok ||
	    store.set<double>("app/users/alice/location/longitude", 2.1734) != akasha::Status::ok) {
		std::cerr << "✗ Failed to store alice's data\n";
		return 1;
	}
	
	std::cout << "  ✓ Stored alice with location: Barcelona (41.3851, 2.1734)\n\n";
	
	// Retrieve using hierarchical access
	std::cout << "2. Retrieving alice's profile through hierarchical paths:\n\n";
	
	auto alice_view = store.get<akasha::Store::DatasetView>("app/users/alice");
	if (alice_view.has_value()) {
		auto name = alice_view->get<std::string>("name");
		auto age = alice_view->get<int64_t>("age");
		auto lat = alice_view->get<double>("location/latitude");
		auto lon = alice_view->get<double>("location/longitude");
		
		if (name && age && lat && lon) {
			std::cout << "  ✓ Name: " << *name << "\n";
			std::cout << "  ✓ Age: " << *age << "\n";
			std::cout << "  ✓ Location: (" << *lat << ", " << *lon << ")\n\n";
		}
	}
	
	// Store more users
	std::cout << "3. Storing multiple users with different nested structures:\n\n";
	
	if (store.set<std::string>("app/users/bob/name", "Bob") != akasha::Status::ok ||
	    store.set<int64_t>("app/users/bob/age", 25) != akasha::Status::ok ||
	    store.set<double>("app/users/bob/location/latitude", 48.8566) != akasha::Status::ok ||
	    store.set<double>("app/users/bob/location/longitude", 2.3522) != akasha::Status::ok) {
		std::cerr << "Failed to store bob's data\n";
		return 1;
	}
	
	if (store.set<std::string>("app/users/charlie/name", "Charlie") != akasha::Status::ok ||
	    store.set<int64_t>("app/users/charlie/age", 35) != akasha::Status::ok ||
	    store.set<double>("app/users/charlie/location/latitude", 35.6762) != akasha::Status::ok ||
	    store.set<double>("app/users/charlie/location/longitude", 139.6503) != akasha::Status::ok) {
		std::cerr << "Failed to store charlie's data\n";
		return 1;
	}
	
	std::cout << "  ✓ Stored bob (Paris) and charlie (Tokyo)\n\n";
	
	// Navigate intermediate levels
	std::cout << "4. Navigating all users:\n\n";
	
	auto users_view = store.get<akasha::Store::DatasetView>("app/users");
	if (users_view.has_value()) {
		auto user_keys = users_view->keys();
		std::cout << "  Found " << user_keys.size() << " users:\n";
		for (const auto& key : user_keys) {
			auto user_view = users_view->get<akasha::Store::DatasetView>(key);
			if (user_view.has_value()) {
				auto name = user_view->get<std::string>("name");
				auto age = user_view->get<int64_t>("age");
				if (name && age) {
					std::cout << "    - " << *name << " (age: " << *age << ")\n";
				}
			}
		}
	}
	
	std::cout << "\n5. Direct access to nested location data:\n\n";
	
	// Access deeply nested individual values
	auto bob_lat = store.get<double>("app/users/bob/location/latitude");
	auto charlie_lon = store.get<double>("app/users/charlie/location/longitude");
	
	if (bob_lat && charlie_lon) {
		std::cout << "  ✓ Bob's latitude: " << *bob_lat << "\n";
		std::cout << "  ✓ Charlie's longitude: " << *charlie_lon << "\n";
	}
	
	return 0;
}
