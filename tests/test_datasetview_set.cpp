#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: DatasetView::set<T>() — Writing through views
//
// Note: The root dataset view (store.get<DatasetView>("dataset_id")) is always
// available when a dataset is loaded, even if empty. Subpath views are only
// available once data exists under that subpath.
// ============================================================================

TEST(datasetview_set_int64) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("config", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Get the dataset root view — always available when dataset is loaded
	auto view = store.get<akasha::Store::DatasetView>("config");
	ASSERT_TRUE(view.has_value());

	// Write through the view using a nested relative path
	auto status = view->set<int64_t>("settings/timeout", 30);
	ASSERT_EQ(status, akasha::Status::ok);

	// Verify: read directly from store using absolute path
	auto timeout = store.get<int64_t>("config/settings/timeout");
	ASSERT_TRUE(timeout.has_value());
	ASSERT_EQ(timeout.value(), 30);

	// Verify: read through the view using relative path
	auto timeout_via_view = view->get<int64_t>("settings/timeout");
	ASSERT_TRUE(timeout_via_view.has_value());
	ASSERT_EQ(timeout_via_view.value(), 30);
}

TEST(datasetview_set_string) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("app", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Root view allows writing to any nested path
	auto view = store.get<akasha::Store::DatasetView>("app");
	ASSERT_TRUE(view.has_value());

	// Write string through view using nested path
	auto status = view->set<std::string>("info/name", "MyApp");
	ASSERT_EQ(status, akasha::Status::ok);

	// Verify
	auto name = store.get<std::string>("app/info/name");
	ASSERT_TRUE(name.has_value());
	ASSERT_EQ(name.value(), "MyApp");
}

TEST(datasetview_set_bool) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	auto view = store.get<akasha::Store::DatasetView>("data");
	ASSERT_TRUE(view.has_value());

	// Write bool through view using nested path
	auto status = view->set<bool>("flags/enabled", true);
	ASSERT_EQ(status, akasha::Status::ok);

	// Verify
	auto enabled = store.get<bool>("data/flags/enabled");
	ASSERT_TRUE(enabled.has_value());
	ASSERT_EQ(enabled.value(), true);
}

TEST(datasetview_set_double) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("sensors", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	auto view = store.get<akasha::Store::DatasetView>("sensors");
	ASSERT_TRUE(view.has_value());

	// Write double through view using nested path
	auto status = view->set<double>("readings/temperature", 23.5);
	ASSERT_EQ(status, akasha::Status::ok);

	// Verify
	auto temp_val = store.get<double>("sensors/readings/temperature");
	ASSERT_TRUE(temp_val.has_value());
	ASSERT_NEAR(temp_val.value(), 23.5, 0.01);
}

TEST(datasetview_set_multiple_fields) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("user", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Root view for the dataset
	auto view = store.get<akasha::Store::DatasetView>("user");
	ASSERT_TRUE(view.has_value());

	// Write multiple fields using nested paths from root view
	ASSERT_EQ(view->set<std::string>("profile/name", "Alice"), akasha::Status::ok);
	ASSERT_EQ(view->set<int64_t>("profile/age", 30), akasha::Status::ok);
	ASSERT_EQ(view->set<bool>("profile/admin", true), akasha::Status::ok);

	// Verify all through store
	ASSERT_EQ(store.get<std::string>("user/profile/name").value(), "Alice");
	ASSERT_EQ(store.get<int64_t>("user/profile/age").value(), 30);
	ASSERT_EQ(store.get<bool>("user/profile/admin").value(), true);
}

TEST(datasetview_set_nested_view) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Get root view (always available once dataset is loaded)
	auto root_view = store.get<akasha::Store::DatasetView>("data");
	ASSERT_TRUE(root_view.has_value());

	// Create some data under "level1" through the root view
	ASSERT_EQ(root_view->set<int64_t>("level1/seed", 0), akasha::Status::ok);

	// Now a subpath view of "level1" exists because data is under it
	auto nested_view = root_view->get<akasha::Store::DatasetView>("level1");
	ASSERT_TRUE(nested_view.has_value());

	// Write through nested view (relative path "value" → absolute "data/level1/value")
	ASSERT_EQ(nested_view->set<int64_t>("value", 42), akasha::Status::ok);

	// Verify through store absolute path
	auto val = store.get<int64_t>("data/level1/value");
	ASSERT_TRUE(val.has_value());
	ASSERT_EQ(val.value(), 42);
}

TEST(datasetview_set_vector) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Root view allows writing vectors to nested paths
	auto view = store.get<akasha::Store::DatasetView>("db");
	ASSERT_TRUE(view.has_value());

	// Write vector through view using nested path
	std::vector<int64_t> numbers = {1, 2, 3, 4, 5};
	auto status = view->set<std::vector<int64_t>>("arrays/ids", numbers);
	ASSERT_EQ(status, akasha::Status::ok);

	// Verify
	auto ids = store.get<std::vector<int64_t>>("db/arrays/ids");
	ASSERT_TRUE(ids.has_value());
	ASSERT_EQ(ids.value().size(), 5);
	ASSERT_EQ(ids.value()[0], 1);
	ASSERT_EQ(ids.value()[4], 5);
}

TEST(datasetview_set_nonexistent_subpath) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("test", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// Subpath view returns nullopt when no data exists under that path
	auto subview = store.get<akasha::Store::DatasetView>("test/nonexistent");
	ASSERT_FALSE(subview.has_value());

	// Non-loaded dataset root returns nullopt
	akasha::Store store2;
	auto view2 = store2.get<akasha::Store::DatasetView>("notloaded");
	ASSERT_FALSE(view2.has_value());
}
