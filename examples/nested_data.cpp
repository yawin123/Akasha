#include "akasha.hpp"
#include <iostream>
#include <cstring>

/**
 * Demonstrates storage and retrieval of nested data structures.
 * 
 * Question: Are there nested structures?
 * Answer: Yes, you can store trivially copyable structs, including nested ones.
 */

// Internal structure (trivially copyable)
struct Location {
	double latitude;
	double longitude;
	
	bool operator==(const Location& other) const {
		return latitude == other.latitude && longitude == other.longitude;
	}
};

// Complex structure containing other structures (trivially copyable)
struct User {
	int64_t id;
	int32_t age;
	char name[64];  // Fixed-size buffer para ser trivially copyable
	Location home;
	
	bool operator==(const User& other) const {
		return id == other.id && age == other.age && 
		       std::strcmp(name, other.name) == 0 && 
		       home == other.home;
	}
};

// Even more complex structure
struct UserProfile {
	User user;
	int64_t signup_timestamp;
	int32_t login_count;
	
	bool operator==(const UserProfile& other) const {
		return user == other.user && 
		       signup_timestamp == other.signup_timestamp &&
		       login_count == other.login_count;
	}
};

int main() {
	akasha::Store store;
	
	std::cout << "=== Nested Structures Demo ===\n\n";
	
	// Load dataset
	std::string file_path = "/tmp/akasha_nested.db";
	auto status = store.load("profiles", file_path, akasha::FileOptions::create_if_missing);
	if (status != akasha::Status::ok) {
		std::cerr << "Failed to load dataset\n";
		return 1;
	}
	
	// Create nested structure
	std::cout << "Creating nested structure (UserProfile -> User -> Location):\n\n";
	
	Location home = {41.3851, 2.1734};  // Barcelona
	User user = {1001, 30, "Alice", home};
	UserProfile profile = {user, 1577836800, 42};  // 1577836800 = 2020-01-01
	
	// Store complete nested structure
	std::cout << "Storing UserProfile struct:\n";
	auto status_set = store.set<UserProfile>("profiles.alice.data", profile);
	if (status_set != akasha::Status::ok) {
		std::cerr << "✗ Failed to store UserProfile: " << (int)status_set << "\n";
		return 1;
	}
	std::cout << "  user.id = " << profile.user.id << "\n";
	std::cout << "  user.name = " << profile.user.name << "\n";
	std::cout << "  user.age = " << profile.user.age << "\n";
	std::cout << "  user.home = (" << profile.user.home.latitude 
	          << ", " << profile.user.home.longitude << ")\n";
	std::cout << "  login_count = " << profile.login_count << "\n\n";
	
	// Retrieve complete structure
	std::cout << "Retrieving UserProfile struct:\n";
	auto retrieved = store.get<UserProfile>("profiles.alice.data");
	
	if (retrieved.has_value()) {
		std::cout << "✓ Successfully retrieved nested structure\n";
		std::cout << "  user.id = " << retrieved->user.id << "\n";
		std::cout << "  user.name = " << retrieved->user.name << "\n";
		std::cout << "  user.age = " << retrieved->user.age << "\n";
		std::cout << "  user.home = (" << retrieved->user.home.latitude 
		          << ", " << retrieved->user.home.longitude << ")\n";
		std::cout << "  signup_timestamp = " << retrieved->signup_timestamp << "\n";
		std::cout << "  login_count = " << retrieved->login_count << "\n\n";
		
		// Verify that data is identical
		if (*retrieved == profile) {
			std::cout << "✓ Data integrity verified (retrieved == original)\n\n";
		} else {
			std::cout << "✗ Data mismatch!\n\n";
		}
	} else {
		std::cerr << "✗ Failed to retrieve data\n";
		return 1;
	}
	
	// We can also store just parts
	std::cout << "Storing inner struct (User) separately:\n";
	User bob = {1002, 25, "Bob", {48.8566, 2.3522}};  // Paris
	auto bob_status = store.set<User>("profiles.bob.user", bob);
	if (bob_status != akasha::Status::ok) {
		std::cerr << "✗ Failed to store User: " << (int)bob_status << "\n";
		return 1;
	}
	std::cout << "  bob.id = " << bob.id << "\n";
	std::cout << "  bob.name = " << bob.name << "\n\n";
	
	// Y recuperar solo la parte
	auto bob_user = store.get<User>("profiles.bob.user");
	if (bob_user.has_value()) {
		std::cout << "✓ Retrieved User struct\n";
		std::cout << "  bob.id = " << bob_user->id << "\n";
		std::cout << "  bob.home = (" << bob_user->home.latitude 
		          << ", " << bob_user->home.longitude << ")\n\n";
	}
	
	// We can even store just the Location
	std::cout << "Storing innermost struct (Location) separately:\n";
	Location tokyo = {35.6762, 139.6503};
	auto tokyo_status = store.set<Location>("profiles.charlie.location", tokyo);
	if (tokyo_status != akasha::Status::ok) {
		std::cerr << "✗ Failed to store Location: " << (int)tokyo_status << "\n";
		return 1;
	}
	std::cout << "  tokyo = (" << tokyo.latitude << ", " << tokyo.longitude << ")\n\n";
	
	auto tokyo_loc = store.get<Location>("profiles.charlie.location");
	if (tokyo_loc.has_value()) {
		std::cout << "✓ Retrieved Location struct\n";
		std::cout << "  tokyo = (" << tokyo_loc->latitude 
		          << ", " << tokyo_loc->longitude << ")\n";
	}
	
	return 0;
}
