// STL container cross-serialization tests

#include "test_framework.hpp"
#include "test_common.hpp"
#include <array>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <algorithm>

TEST(stl_containers_chain) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // array → vector
    std::array<int64_t, 5> as_array = {10, 20, 30, 40, 50};
    auto array_set_status = store.set<std::array<int64_t, 5>>("data/seq", as_array);
    ASSERT_EQ(array_set_status, akasha::Status::ok);
    auto as_vector = store.get<std::vector<int64_t>>("data/seq");
    ASSERT_TRUE(as_vector.has_value());
    ASSERT_SIZE(*as_vector, size_t(5));

    // vector → list
    ASSERT_EQ(store.set<std::vector<int64_t>>("data/seq", *as_vector), akasha::Status::ok);
    auto as_list = store.get<std::list<int64_t>>("data/seq");
    ASSERT_TRUE(as_list.has_value());
    ASSERT_SIZE(*as_list, size_t(5));

    // list → set
    ASSERT_EQ(store.set<std::list<int64_t>>("data/seq", *as_list), akasha::Status::ok);
    auto as_set = store.get<std::set<int64_t>>("data/seq");
    ASSERT_TRUE(as_set.has_value());
    ASSERT_SIZE(*as_set, size_t(5));

    // set → unordered_set
    ASSERT_EQ(store.set<std::set<int64_t>>("data/seq", *as_set), akasha::Status::ok);
    auto as_unordered_set = store.get<std::unordered_set<int64_t>>("data/seq");
    ASSERT_TRUE(as_unordered_set.has_value());
    ASSERT_SIZE(*as_unordered_set, size_t(5));

    // unordered_set → array (cierre del ciclo, verifica que el count coincide con N)
    ASSERT_EQ(store.set<std::unordered_set<int64_t>>("data/seq", *as_unordered_set), akasha::Status::ok);
    auto final_array = store.get<std::array<int64_t, 5>>("data/seq");
    ASSERT_TRUE(final_array.has_value());

    for (const auto& elem : as_array) {
        ASSERT_TRUE(std::find(final_array->begin(), final_array->end(), elem) != final_array->end());
    }
}

TEST(stl_map_chain) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("data", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // map → unordered_map
    std::map<std::string, int64_t> original = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto map_set_status = store.set<std::map<std::string, int64_t>>("data/m", original);
    ASSERT_EQ(map_set_status, akasha::Status::ok);

    auto as_unordered = store.get<std::unordered_map<std::string, int64_t>>("data/m");
    ASSERT_TRUE(as_unordered.has_value());
    ASSERT_SIZE(*as_unordered, size_t(3));

    // unordered_map → map (cierre del ciclo)
    auto unordered_set_status = store.set<std::unordered_map<std::string, int64_t>>("data/m", *as_unordered);
    ASSERT_EQ(unordered_set_status, akasha::Status::ok);

    auto final_map = store.get<std::map<std::string, int64_t>>("data/m");
    ASSERT_TRUE(final_map.has_value());
    ASSERT_SIZE(*final_map, size_t(3));
    
    for (const auto& [k, v] : original) {
        ASSERT_TRUE(final_map->count(k) == 1);
        ASSERT_EQ(final_map->at(k), v);
    }
}
