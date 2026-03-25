#include "akasha.hpp"
#include <iostream>

/**
 * Demonstrates cleanup and compaction operations.
 * 
 * Question: Can I delete a data set? In what ways?
 * Answer: Yes, with clear() at different granularities, and compact() to free space.
 */

int main() {
	akasha::Store store;
	
	std::cout << "=== Data Lifecycle: Clear and Compact Demo ===\n\n";
	
	// Load dataset
	std::string file_path = "/tmp/akasha_lifecycle.db";
	auto status = store.load("data", file_path, akasha::FileOptions::create_if_missing);
	if (status != akasha::Status::ok) {
		std::cerr << "Failed to load dataset\n";
		return 1;
	}
	
	// Create data structure
	std::cout << "Creating data structure:\n";
	auto s1 = store.set<int64_t>("data.counter.hits", 100);
	auto s2 = store.set<int64_t>("data.counter.misses", 20);
	auto s3 = store.set<std::string>("data.cache.key1", "value1");
	auto s4 = store.set<std::string>("data.cache.key2", "value2");
	auto s5 = store.set<std::string>("data.cache.key3", "value3");
	
	bool all_ok = (s1 == akasha::Status::ok && s2 == akasha::Status::ok && 
	               s3 == akasha::Status::ok && s4 == akasha::Status::ok &&
	               s5 == akasha::Status::ok);
	
	if (all_ok) {
		std::cout << "✓ Data created\n\n";
	} else {
		std::cerr << "✗ Failed to create data: " << (int)store.last_status() << "\n";
		return 1;
	}
	
	// Verificar datos
	std::cout << "Verifying data before delete:\n";
	std::cout << "  has(\"data.counter.hits\") = " 
		<< (store.has("data.counter.hits") ? "true" : "false") << "\n";
	std::cout << "  has(\"data.cache.key1\") = " 
		<< (store.has("data.cache.key1") ? "true" : "false") << "\n\n";
	
	// Example 1: Delete a specific branch (e.g., data.cache.*)
	std::cout << "=== Delete branch: clear(\"data.cache\") ===\n";
	status = store.clear("data.cache");
	if (status != akasha::Status::ok) {
		std::cerr << "✗ Failed to clear branch\n";
		return 1;
	}
	std::cout << "  Deleted branch 'data.cache'\n";
	std::cout << "  has(\"data.cache.key1\") = " 
		<< (store.has("data.cache.key1") ? "true" : "false") << " ✓\n";
	std::cout << "  has(\"data.counter.hits\") = " 
		<< (store.has("data.counter.hits") ? "true" : "false") 
		<< " (still exists) ✓\n\n";
	
	// Create more data to demonstrate clearing entire dataset
	std::cout << "Creating more data:\n";
	auto s6 = store.set<int64_t>("data.logs.count", 500);
	auto s7 = store.set<int64_t>("data.logs.errors", 10);
	if (s6 != akasha::Status::ok || s7 != akasha::Status::ok) {
		std::cerr << "✗ Failed to create log data\n";
		return 1;
	}
	std::cout << "  has(\"data.logs.count\") = " 
		<< (store.has("data.logs.count") ? "true" : "false") << "\n\n";
	
	// Example 2: Delete entire dataset
	std::cout << "=== Delete entire dataset: clear(\"data\") ===\n";
	status = store.clear("data");
	if (status != akasha::Status::ok) {
		std::cerr << "✗ Failed to clear dataset\n";
		return 1;
	}
	std::cout << "  Deleted entire 'data' dataset\n";
	std::cout << "  has(\"data.counter.hits\") = " 
		<< (store.has("data.counter.hits") ? "true" : "false") << " ✓\n";
	std::cout << "  has(\"data.logs.count\") = " 
		<< (store.has("data.logs.count") ? "true" : "false") << " ✓\n\n";
	
	// Recreate data to demonstrate compact
	std::cout << "Recreating data for compaction demo:\n";
	for (int i = 0; i < 100; ++i) {
		status = store.set<int64_t>("data.items.item" + std::to_string(i), i);
		if (status != akasha::Status::ok) {
			std::cerr << "✗ Failed to create item " << i << "\n";
			return 1;
		}
	}
	std::cout << "  Created 100 items\n\n";
	
	// Delete half of the data
	std::cout << "Deleting half of the items...\n";
	for (int i = 0; i < 50; ++i) {
		status = store.clear("data.items.item" + std::to_string(i));
		if (status != akasha::Status::ok) {
			std::cerr << "✗ Failed to delete item " << i << "\n";
			return 1;
		}
	}
	std::cout << "✓ 50 items deleted\n\n";
	
	// Example 3: Compact to free space
	std::cout << "=== Compacting to reclaim space: compact(\"data\") ===\n";
	auto compact_status = store.compact("data");
	if (compact_status == akasha::Status::ok) {
		std::cout << "✓ Compaction of 'data' dataset successful\n";
	} else {
		std::cout << "✗ Compaction failed with status\n";
	}
	std::cout << "\n";
	
	// Verify that valid data still exists
	std::cout << "Verifying remaining data after compaction:\n";
	auto item50 = store.get<int64_t>("data.items.item50");
	auto item99 = store.get<int64_t>("data.items.item99");
	std::cout << "  has(\"data.items.item50\") = " 
		<< (item50.has_value() ? "true ✓" : "false") << "\n";
	std::cout << "  has(\"data.items.item99\") = " 
		<< (item99.has_value() ? "true ✓" : "false") << "\n\n";
	
	// Example 4: Clear everything (clear without arguments)
	std::cout << "=== Clear everything: clear() ===\n";
	status = store.clear();
	if (status != akasha::Status::ok) {
		std::cerr << "✗ Failed to clear all data\n";
		return 1;
	}
	std::cout << "✓ All data cleared\n\n";
	
	return 0;
}
