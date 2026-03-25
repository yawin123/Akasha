// Vector serialization and deserialization tests

#include "test_framework.hpp"
#include "test_common.hpp"

TEST(vector_int_basic) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<int> values = {10, 20, 30, 40, 50};
	ASSERT_EQ(store.set<std::vector<int>>("data.numbers", values), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<int>>("data.numbers");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(5));
	ASSERT_EQ(retrieved->at(0), 10);
	ASSERT_EQ(retrieved->at(4), 50);
}

TEST(vector_double) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<double> values = {1.5, 2.7, 3.14, 4.0};
	ASSERT_EQ(store.set<std::vector<double>>("data.floats", values), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<double>>("data.floats");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(4));
	ASSERT_NEAR(retrieved->at(0), 1.5, 0.0001);
	ASSERT_NEAR(retrieved->at(2), 3.14, 0.0001);
}

TEST(vector_empty) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<int64_t> empty;
	ASSERT_EQ(store.set<std::vector<int64_t>>("data.empty", empty), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<int64_t>>("data.empty");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EMPTY(*retrieved);
}

TEST(vector_bool) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<bool> values = {true, false, true, false, true};
	ASSERT_EQ(store.set<std::vector<bool>>("data.flags", values), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<bool>>("data.flags");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(5));
	ASSERT_EQ(retrieved->at(0), true);
	ASSERT_EQ(retrieved->at(1), false);
	ASSERT_EQ(retrieved->at(4), true);
}

TEST(vector_large) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<uint32_t> large_vec;
	for (uint32_t i = 0; i < 1000; ++i) {
		large_vec.push_back(i * 2);
	}
	
	ASSERT_EQ(store.set<std::vector<uint32_t>>("data.large", large_vec), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<uint32_t>>("data.large");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(1000));
	ASSERT_EQ(retrieved->at(0), uint32_t(0));
	ASSERT_EQ(retrieved->at(500), uint32_t(1000));
	ASSERT_EQ(retrieved->at(999), uint32_t(1998));
}

TEST(vector_persistence) {
	TempFile temp;
	
	// First session: set vector
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
		
		std::vector<int> values = {100, 200, 300};
		ASSERT_EQ(store.set<std::vector<int>>("data.persistent", values), akasha::Status::ok);
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Second session: retrieve vector
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<int>>("data.persistent");
		ASSERT_TRUE(retrieved.has_value());
		ASSERT_SIZE(*retrieved, size_t(3));
		ASSERT_EQ(retrieved->at(1), 200);
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
}

TEST(vector_coexistence) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	// Store scalar
	ASSERT_EQ(store.set<int64_t>("data.count", 42), akasha::Status::ok);
	
	// Store string
	ASSERT_EQ(store.set<std::string>("data.name", "test"), akasha::Status::ok);
	
	// Store vector
	std::vector<double> vec = {1.1, 2.2, 3.3};
	ASSERT_EQ(store.set<std::vector<double>>("data.values", vec), akasha::Status::ok);
	
	// Retrieve all
	auto count = store.get<int64_t>("data.count");
	ASSERT_TRUE(count.has_value() && count.value() == 42);
	
	auto name = store.get<std::string>("data.name");
	ASSERT_TRUE(name.has_value() && name.value() == "test");
	
	auto values = store.get<std::vector<double>>("data.values");
	ASSERT_TRUE(values.has_value() && values->size() == 3);
}

TEST(vector_getorset) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<int> defaults = {1, 2, 3};
	
	// First call: key doesn't exist, should set and return default
	auto result1 = store.getorset<std::vector<int>>("data.lazy", defaults);
	ASSERT_TRUE(result1.has_value());
	ASSERT_SIZE(*result1, size_t(3));
	ASSERT_EQ(result1->at(0), 1);
	
	// Second call: key exists, should return existing (not default)
	auto result2 = store.getorset<std::vector<int>>("data.lazy", std::vector<int>{9, 9, 9});
	ASSERT_TRUE(result2.has_value());
	ASSERT_SIZE(*result2, size_t(3));
	ASSERT_EQ(result2->at(0), 1);
}

TEST(vector_type_mismatch) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	// Set vector of ints
	std::vector<int> int_vec = {1, 2, 3};
	ASSERT_EQ(store.set<std::vector<int>>("data.mixed", int_vec), akasha::Status::ok);
	
	// Try to retrieve as vector of doubles (size mismatch)
	auto result = store.get<std::vector<double>>("data.mixed");
	ASSERT_TRUE(!result.has_value());
}

TEST(vector_string_basic) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<std::string> values = {"hello", "world", "akasha", "test"};
	ASSERT_EQ(store.set<std::vector<std::string>>("data.words", values), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<std::string>>("data.words");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(4));
	ASSERT_EQ(retrieved->at(0), "hello");
	ASSERT_EQ(retrieved->at(3), "test");
}

TEST(vector_string_empty) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<std::string> empty;
	ASSERT_EQ(store.set<std::vector<std::string>>("data.empty_strings", empty), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<std::string>>("data.empty_strings");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EMPTY(*retrieved);
}

TEST(vector_string_long) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	std::vector<std::string> values = {
		"short",
		"this is a much longer string with more characters",
		"x",
		"another medium-length string that is not too short and not too long"
	};
	ASSERT_EQ(store.set<std::vector<std::string>>("data.mixed_lengths", values), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<std::string>>("data.mixed_lengths");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_SIZE(*retrieved, size_t(4));
	ASSERT_EQ(retrieved->at(1), "this is a much longer string with more characters");
	ASSERT_EQ(retrieved->at(2), "x");
}

TEST(vector_string_persistence) {
	TempFile temp;
	
	// First session: set vector
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
		
		std::vector<std::string> values = {"persistent", "data", "test"};
		ASSERT_EQ(store.set<std::vector<std::string>>("data.words", values), akasha::Status::ok);
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Second session: retrieve vector
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<std::string>>("data.words");
		ASSERT_TRUE(retrieved.has_value());
		ASSERT_SIZE(*retrieved, size_t(3));
		ASSERT_EQ(retrieved->at(0), "persistent");
		ASSERT_EQ(retrieved->at(1), "data");
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
}

TEST(vector_string_coexistence) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	// Store scalar
	ASSERT_EQ(store.set<int64_t>("data.count", 42), akasha::Status::ok);
	
	// Store single string
	ASSERT_EQ(store.set<std::string>("data.name", "Alice"), akasha::Status::ok);
	
	// Store vector of strings
	std::vector<std::string> tags = {"important", "archived", "verified"};
	ASSERT_EQ(store.set<std::vector<std::string>>("data.tags", tags), akasha::Status::ok);
	
	// Store vector of ints
	std::vector<int> numbers = {1, 2, 3};
	ASSERT_EQ(store.set<std::vector<int>>("data.numbers", numbers), akasha::Status::ok);
	
	// Retrieve all
	auto count = store.get<int64_t>("data.count");
	ASSERT_TRUE(count.has_value() && count.value() == 42);
	
	auto name = store.get<std::string>("data.name");
	ASSERT_TRUE(name.has_value() && name.value() == "Alice");
	
	auto str_vec = store.get<std::vector<std::string>>("data.tags");
	ASSERT_TRUE(str_vec.has_value() && str_vec->size() == 3 && str_vec->at(0) == "important");
	
	auto int_vec = store.get<std::vector<int>>("data.numbers");
	ASSERT_TRUE(int_vec.has_value() && int_vec->size() == 3);
}

TEST(vector_string_large) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
	
	// Create large vector of strings
	std::vector<std::string> large_vec;
	for (int i = 0; i < 100; ++i) {
		large_vec.push_back("item_" + std::to_string(i));
	}
	
	ASSERT_EQ(store.set<std::vector<std::string>>("data.large", large_vec), akasha::Status::ok);
	
	auto retrieved = store.get<std::vector<std::string>>("data.large");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(retrieved->size(), size_t(100));
	ASSERT_EQ(retrieved->at(0), "item_0");
	ASSERT_EQ(retrieved->at(50), "item_50");
	ASSERT_EQ(retrieved->at(99), "item_99");
}

TEST(vector_int_resize_no_garbage) {
	TempFile temp;
	
	// Phase 1: Store initial vector with 20 elements
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
		
		std::vector<int> initial;
		for (int i = 0; i < 20; ++i) {
			initial.push_back(i);
		}
		ASSERT_EQ(store.set<std::vector<int>>("data.numbers", initial), akasha::Status::ok);
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Phase 2: Shrink vector from 20 to 10 elements
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<int>>("data.numbers");
		ASSERT_TRUE(retrieved.has_value());
		ASSERT_EQ(retrieved->size(), size_t(20));
		
		// Shrink by erasing
		retrieved->resize(10);
		ASSERT_EQ(retrieved->size(), size_t(10));
		
		// Verify content before saving
		for (int i = 0; i < 10; ++i) {
			ASSERT_EQ(retrieved->at(i), i);
		}
		
		// Save the shrunken vector
		ASSERT_EQ(store.set<std::vector<int>>("data.numbers", *retrieved), akasha::Status::ok);
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Phase 3: Verify the vector has exactly 10 elements (no garbage from original 20)
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<int>>("data.numbers");
		ASSERT_TRUE(retrieved.has_value());
		// This is the critical check: size must be exactly 10, not 20 or any other garbage value
		ASSERT_EQ(retrieved->size(), size_t(10));
		
		for (int i = 0; i < 10; ++i) {
			ASSERT_EQ(retrieved->at(i), i);
		}
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Phase 4: Grow vector from 10 to 25 elements
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<int>>("data.numbers");
		ASSERT_TRUE(retrieved.has_value());
		ASSERT_EQ(retrieved->size(), size_t(10));
		
		// Grow by adding new elements
		for (int i = 10; i < 25; ++i) {
			retrieved->push_back(i);
		}
		ASSERT_EQ(retrieved->size(), size_t(25));
		
		// Save the grown vector
		ASSERT_EQ(store.set<std::vector<int>>("data.numbers", *retrieved), akasha::Status::ok);
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
	
	// Phase 5: Final verification - exactly 25 elements with correct values
	{
		akasha::Store store;
		ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
		
		auto retrieved = store.get<std::vector<int>>("data.numbers");
		ASSERT_TRUE(retrieved.has_value());
		// Critical check: size must be exactly 25, not 20 or any garbage value
		ASSERT_EQ(retrieved->size(), size_t(25));
		
		for (int i = 0; i < 25; ++i) {
			ASSERT_EQ(retrieved->at(i), i);
		}
		
		ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	}
}

TEST(vector_int_resize_file_size) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Insert large vector and compact to get optimal baseline size
	std::vector<int> large(10000);
	for (int i = 0; i < 10000; ++i) large[i] = i;
	ASSERT_EQ(store.set<std::vector<int>>("data.numbers", large), akasha::Status::ok);
	ASSERT_EQ(store.compact("data"), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_after_large = fs::file_size(temp.path());

	// Replace with small vector (fragmentation: file keeps old size)
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	std::vector<int> small = {0, 1, 2};
	ASSERT_EQ(store.set<std::vector<int>>("data.numbers", small), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_before_compact = fs::file_size(temp.path());

	// Compact to reclaim space
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	ASSERT_EQ(store.compact("data"), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_after_compact = fs::file_size(temp.path());

	// File size unchanged after overwrite (fragmentation), reduced after compact
	ASSERT_EQ(size_before_compact, size_after_large);
	ASSERT_TRUE(size_after_compact < size_after_large);

	// Data integrity: verify the small vector survived compact
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	auto retrieved = store.get<std::vector<int>>("data.numbers");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(retrieved->size(), size_t(3));
	ASSERT_EQ(retrieved->at(0), 0);
	ASSERT_EQ(retrieved->at(2), 2);
}

TEST(vector_string_resize_file_size) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Insert large vector of strings and compact (1000 strings × ~50 bytes each)
	std::vector<std::string> large;
	for (int i = 0; i < 1000; ++i) {
		large.push_back("string_" + std::to_string(i) + "_with_some_padding_data_aaa");
	}
	ASSERT_EQ(store.set<std::vector<std::string>>("data.words", large), akasha::Status::ok);
	ASSERT_EQ(store.compact("data"), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_after_large = fs::file_size(temp.path());

	// Replace with small vector (fragmentation: file keeps old size)
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	std::vector<std::string> small = {"a", "b", "c"};
	ASSERT_EQ(store.set<std::vector<std::string>>("data.words", small), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_before_compact = fs::file_size(temp.path());

	// Compact to reclaim space
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	ASSERT_EQ(store.compact("data"), akasha::Status::ok);
	ASSERT_EQ(store.unload("data"), akasha::Status::ok);
	const std::size_t size_after_compact = fs::file_size(temp.path());

	// File size typically unchanged or similar after overwrite (fragmentation)
	// The key guarantee: compact() reduces the file size
	ASSERT_TRUE(size_after_compact <= size_before_compact);
	ASSERT_TRUE(size_after_compact < size_after_large);

	// Data integrity: verify small vector survived compact
	ASSERT_EQ(store.load("data", temp.path()), akasha::Status::ok);
	auto retrieved = store.get<std::vector<std::string>>("data.words");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(retrieved->size(), size_t(3));
	ASSERT_EQ(retrieved->at(0), "a");
	ASSERT_EQ(retrieved->at(2), "c");
}

