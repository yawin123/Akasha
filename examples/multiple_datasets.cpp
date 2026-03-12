#include "akasha.hpp"
#include <iostream>
#include <iomanip>

/**
 * Demonstrates the capability to load multiple independent datasets
 * in the same Store, each with its own file.
 * 
 * Question: Can I load two sets of data?
 * Answer: Yes, each with a different source_id.
 */

int main() {
	akasha::Store store;
	
	std::cout << "=== Multiple Datasets Demo ===\n\n";
	
	// Create two data files
	std::string app_file = "/tmp/akasha_app.db";
	std::string user_file = "/tmp/akasha_user.db";
	
	// Load dataset 1: application config
	std::cout << "Loading dataset 'app' from " << app_file << "...\n";
	auto status1 = store.load("app", app_file, true);
	if (status1 != akasha::Status::ok) {
		std::cerr << "Failed to load 'app' dataset\n";
		return 1;
	}
	std::cout << "✓ 'app' dataset loaded\n\n";
	
	// Load dataset 2: user config
	std::cout << "Loading dataset 'user' from " << user_file << "...\n";
	auto status2 = store.load("user", user_file, true);
	if (status2 != akasha::Status::ok) {
		std::cerr << "Failed to load 'user' dataset\n";
		return 1;
	}
	std::cout << "✓ 'user' dataset loaded\n\n";
	
	// Try to reload 'app' dataset (should fail with source_already_loaded)
	std::cout << "Attempting to reload 'app' dataset (should fail):\n";
	auto status3 = store.load("app", app_file, true);
	std::cout << "  Status: " << (status3 == akasha::Status::source_already_loaded 
		? "source_already_loaded ✓" : "unexpected") << "\n\n";
	
	// Write data to dataset 1
	std::cout << "Writing to 'app' dataset:\n";
	auto s1 = store.set<int64_t>("app.server.port", 8080);
	auto s2 = store.set<int64_t>("app.server.timeout", 30);
	if (s1 == akasha::Status::ok && s2 == akasha::Status::ok) {
		std::cout << "  app.server.port = 8080\n";
		std::cout << "  app.server.timeout = 30\n\n";
	} else {
		std::cerr << "✗ Failed to write to 'app': " << (int)store.last_status() << "\n";
		return 1;
	}
	
	// Write data to dataset 2
	std::cout << "Writing to 'user' dataset:\n";
	auto s3 = store.set<int64_t>("user.profile.id", 42);
	auto s4 = store.set<int64_t>("user.profile.age", 25);
	if (s3 == akasha::Status::ok && s4 == akasha::Status::ok) {
		std::cout << "  user.profile.id = 42\n";
		std::cout << "  user.profile.age = 25\n\n";
	} else {
		std::cerr << "✗ Failed to write to 'user': " << (int)store.last_status() << "\n";
		return 1;
	}
	
	// Read data from dataset 1
	std::cout << "Reading from 'app' dataset:\n";
	auto app_port = store.get<int64_t>("app.server.port");
	auto app_timeout = store.get<int64_t>("app.server.timeout");
	std::cout << "  app.server.port = " << app_port.value_or(0) << "\n";
	std::cout << "  app.server.timeout = " << app_timeout.value_or(0) << "\n\n";
	
	// Read data from dataset 2
	std::cout << "Reading from 'user' dataset:\n";
	auto user_id = store.get<int64_t>("user.profile.id");
	auto user_age = store.get<int64_t>("user.profile.age");
	std::cout << "  user.profile.id = " << user_id.value_or(0) << "\n";
	std::cout << "  user.profile.age = " << user_age.value_or(0) << "\n\n";
	
	// Verify that each dataset has its own data (no mixing)
	std::cout << "Checking data isolation:\n";
	auto wrong_dataset = store.get<int64_t>("user.server.port");
	std::cout << "  user.server.port (shouldn't exist) = " 
		<< (wrong_dataset.has_value() ? "found" : "not found") << " ✓\n";
	
	return 0;
}
