// ============================================================================
// Tests: akasha::map<K, V> — persistent map backed by Store
// ============================================================================
#include "test_framework.hpp"
#include "test_common.hpp"

#include "akasha/structs/map.hpp"
#include <map>

// ── Construction & basic size ─────────────────────────────────────────────────

TEST(akasha_map_empty_on_create) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    ASSERT_SIZE(m, std::size_t(0));
    ASSERT_TRUE(m.empty());

    (void)store.unload("db");
}

// ── insert + size ────────────────────────────────────────────────────────────

TEST(akasha_map_insert_and_size) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("a", std::int64_t(1));
    m.insert("b", std::int64_t(2));
    m.insert("c", std::int64_t(3));

    ASSERT_SIZE(m, std::size_t(3));
    ASSERT_FALSE(m.empty());

    (void)store.unload("db");
}

// ── contains ─────────────────────────────────────────────────────────────────

TEST(akasha_map_contains) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("x", std::int64_t(99));

    ASSERT_TRUE(m.contains("x"));
    ASSERT_FALSE(m.contains("y"));

    (void)store.unload("db");
}

// ── at() and operator[] const ─────────────────────────────────────────────────

TEST(akasha_map_at_and_read) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("pi", std::int64_t(314));
    m.insert("e",  std::int64_t(271));

    ASSERT_EQ(m.at("pi"), std::int64_t(314));
    ASSERT_EQ(m.at("e"),  std::int64_t(271));

    const auto& cm = m;
    ASSERT_EQ(cm["pi"], std::int64_t(314));

    (void)store.unload("db");
}

// ── at() throws for missing key ───────────────────────────────────────────────

TEST(akasha_map_at_not_found_throws) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");

    bool thrown = false;
    try { (void)m.at("missing"); }
    catch (const std::out_of_range&) { thrown = true; }
    ASSERT_TRUE(thrown);

    (void)store.unload("db");
}

// ── mutable operator[] — existing key ────────────────────────────────────────

TEST(akasha_map_proxy_write) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("v", std::int64_t(10));

    m["v"] = std::int64_t(42);
    ASSERT_EQ(m.at("v"), std::int64_t(42));

    (void)store.unload("db");
}

// ── mutable operator[] — new key auto-inserts default ─────────────────────────

TEST(akasha_map_proxy_new_key) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m["newkey"] = std::int64_t(7);

    ASSERT_SIZE(m, std::size_t(1));
    ASSERT_TRUE(m.contains("newkey"));
    ASSERT_EQ(m.at("newkey"), std::int64_t(7));

    (void)store.unload("db");
}

// ── erase ────────────────────────────────────────────────────────────────────

TEST(akasha_map_erase) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("a", std::int64_t(1));
    m.insert("b", std::int64_t(2));
    m.insert("c", std::int64_t(3));

    ASSERT_TRUE(m.erase("b"));
    ASSERT_SIZE(m, std::size_t(2));
    ASSERT_FALSE(m.contains("b"));
    ASSERT_FALSE(m.erase("b"));  // already gone

    (void)store.unload("db");
}

// ── clear ─────────────────────────────────────────────────────────────────────

TEST(akasha_map_clear) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("a", std::int64_t(1));
    m.insert("b", std::int64_t(2));
    m.clear();

    ASSERT_SIZE(m, std::size_t(0));
    ASSERT_TRUE(m.empty());
    ASSERT_FALSE(m.contains("a"));

    (void)store.unload("db");
}

// ── integer keys ─────────────────────────────────────────────────────────────

TEST(akasha_map_integer_keys) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::int64_t, double> m(store, "db/m");
    m.insert(std::int64_t(1), 1.0);
    m.insert(std::int64_t(2), 4.0);
    m.insert(std::int64_t(3), 9.0);

    ASSERT_SIZE(m, std::size_t(3));
    ASSERT_EQ(m.at(std::int64_t(2)), 4.0);
    ASSERT_FALSE(m.contains(std::int64_t(99)));

    (void)store.unload("db");
}

// ── string values ─────────────────────────────────────────────────────────────

TEST(akasha_map_string_values) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::string> m(store, "db/m");
    m.insert("lang", std::string("C++23"));
    m.insert("lib",  std::string("akasha"));

    ASSERT_EQ(m.at("lang"), std::string("C++23"));
    ASSERT_EQ(m.at("lib"),  std::string("akasha"));

    (void)store.unload("db");
}

// ── insert replaces existing value ────────────────────────────────────────────

TEST(akasha_map_insert_replace) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("k", std::int64_t(1));
    m.insert("k", std::int64_t(42));  // replace

    ASSERT_SIZE(m, std::size_t(1));   // still one entry
    ASSERT_EQ(m.at("k"), std::int64_t(42));

    (void)store.unload("db");
}

// ── persistence across reload ─────────────────────────────────────────────────

TEST(akasha_map_persistence) {
    TempFile tmp("test_akasha_map.mmap");
    {
        akasha::Store store;
        (void)store.load("db", tmp.path(), akasha::FileOptions::create_if_missing);
        akasha::map<std::string, std::int64_t> m(store, "db/m");
        m.insert("key1", std::int64_t(100));
        m.insert("key2", std::int64_t(200));
        (void)store.unload("db");
    }
    {
        akasha::Store store;
        (void)store.load("db", tmp.path());
        akasha::map<std::string, std::int64_t> m(store, "db/m");
        ASSERT_SIZE(m, std::size_t(2));
        ASSERT_EQ(m.at("key1"), std::int64_t(100));
        ASSERT_EQ(m.at("key2"), std::int64_t(200));
        (void)store.unload("db");
    }
}

// ── range-for iteration (const) ───────────────────────────────────────────────

TEST(akasha_map_range_for_const) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("a", std::int64_t(10));
    m.insert("b", std::int64_t(20));
    m.insert("c", std::int64_t(30));

    std::int64_t sum = 0;
    std::size_t count = 0;
    for (auto [k, v] : m) {
        sum += v;
        ++count;
    }
    ASSERT_EQ(count, std::size_t(3));
    ASSERT_EQ(sum, std::int64_t(60));

    (void)store.unload("db");
}

// ── empty map iteration ───────────────────────────────────────────────────────

TEST(akasha_map_iterate_empty) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    std::size_t count = 0;
    for (auto [k, v] : m) { ++count; (void)k; (void)v; }
    ASSERT_EQ(count, std::size_t(0));

    (void)store.unload("db");
}

// ── interop: write std::map, read akasha::map ─────────────────────────────────

TEST(akasha_map_interop_read_std_map) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    std::map<std::string, std::int64_t> stl { {"alpha", 1}, {"beta", 2}, {"gamma", 3} };
    (void)store.set<std::map<std::string, std::int64_t>>("db/m", stl);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    ASSERT_SIZE(m, std::size_t(3));
    ASSERT_EQ(m.at("alpha"), std::int64_t(1));
    ASSERT_EQ(m.at("beta"),  std::int64_t(2));
    ASSERT_EQ(m.at("gamma"), std::int64_t(3));

    (void)store.unload("db");
}

// ── interop: write akasha::map, read via store.get<std::map> ──────────────────

TEST(akasha_map_interop_write_then_get_std) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> m(store, "db/m");
    m.insert("x", std::int64_t(10));
    m.insert("y", std::int64_t(20));

    auto result = store.get<std::map<std::string, std::int64_t>>("db/m");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ((*result)["x"], std::int64_t(10));
    ASSERT_EQ((*result)["y"], std::int64_t(20));

    (void)store.unload("db");
}

// ── store.set<akasha::map> — mismo dataset ────────────────────────────────────

TEST(akasha_map_set_get_same_dataset) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::map<std::string, std::int64_t> src(store, "db/src");
    src.insert("p", std::int64_t(100));
    src.insert("q", std::int64_t(200));

    (void)store.set<akasha::map<std::string, std::int64_t>>("db/dst", src);

    auto dst_opt = store.get<akasha::map<std::string, std::int64_t>>("db/dst");
    ASSERT_TRUE(dst_opt.has_value());
    ASSERT_EQ(dst_opt->size(), std::size_t(2));
    ASSERT_EQ(dst_opt->at("p"), std::int64_t(100));
    ASSERT_EQ(dst_opt->at("q"), std::int64_t(200));

    (void)store.unload("db");
}

// ── store.set<akasha::map> — diferentes datasets ──────────────────────────────

TEST(akasha_map_set_get_different_datasets) {
    TempFile tmp1("test_akasha_map1.mmap");
    TempFile tmp2("test_akasha_map2.mmap");
    akasha::Store store;
    (void)store.load("src", tmp1.path(), akasha::FileOptions::create_if_missing);
    (void)store.load("dst", tmp2.path(), akasha::FileOptions::create_if_missing);

    akasha::map<std::string, std::int64_t> src(store, "src/m");
    src.insert("a", std::int64_t(1));
    src.insert("b", std::int64_t(2));

    (void)store.set<akasha::map<std::string, std::int64_t>>("dst/m", src);

    auto dst_opt = store.get<akasha::map<std::string, std::int64_t>>("dst/m");
    ASSERT_TRUE(dst_opt.has_value());
    ASSERT_EQ(dst_opt->size(), std::size_t(2));
    ASSERT_EQ(dst_opt->at("a"), std::int64_t(1));

    (void)store.unload("src");
    (void)store.unload("dst");
}

// ── store.get en path sin __children__ devuelve nullopt ───────────────────────

TEST(akasha_map_get_nonmap_path_returns_nullopt) {
    TempFile tmp("test_akasha_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", tmp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    (void)store.set<std::int64_t>("db/notamap", std::int64_t(42));

    auto result = store.get<akasha::map<std::string, std::int64_t>>("db/notamap");
    ASSERT_FALSE(result.has_value());

    (void)store.unload("db");
}

TEST(container_map_construction_forms) {
    TempFile temp("test_container_ctor_map.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // store.get en path inexistente → nullopt
    {
        auto absent = store.get<akasha::map<std::string, std::int64_t>>("db/m");
        ASSERT_FALSE(absent.has_value());
    }

    // Constructor directo en path nuevo → vacío; no lanza ni devuelve nullopt
    akasha::map<std::string, std::int64_t> m(store, "db/m");
    ASSERT_TRUE(m.empty());

    // Constructor directo en path existente → lee los datos sin borrarlos
    m.insert("a", std::int64_t(1));
    m.insert("b", std::int64_t(2));
    akasha::map<std::string, std::int64_t> m2(store, "db/m");
    ASSERT_SIZE(m2, std::size_t(2));
    ASSERT_EQ(m2.at("a"), std::int64_t(1));
    ASSERT_EQ(m2.at("b"), std::int64_t(2));

    // store.get en path existente → optional con los datos correctos
    auto opt = store.get<akasha::map<std::string, std::int64_t>>("db/m");
    ASSERT_TRUE(opt.has_value());
    ASSERT_SIZE(*opt, std::size_t(2));

    // Interop: store.set<std::map> → store.get<akasha::map>
    (void)store.set<std::map<std::string, std::int64_t>>("db/m2", {{"x", 7}, {"y", 8}});
    auto opt2 = store.get<akasha::map<std::string, std::int64_t>>("db/m2");
    ASSERT_TRUE(opt2.has_value());
    ASSERT_SIZE(*opt2, std::size_t(2));

    // Interop inverso: akasha::map → store.get<std::map>
    auto opt3 = store.get<std::map<std::string, std::int64_t>>("db/m");
    ASSERT_TRUE(opt3.has_value());
    ASSERT_SIZE(*opt3, std::size_t(2));
}

TEST(akasha_map_std_api) {
    TempFile temp("test_map_std_api.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // ── default ctor + move assignment ───────────────────────────────────────
    akasha::map<std::string, std::int64_t> m;
    m = akasha::map<std::string, std::int64_t>(store, "db/m");
    ASSERT_TRUE(m.empty());

    // ── insert(pair) → pair<iterator,bool> ───────────────────────────────────
    auto [it1, ok1] = m.insert(std::pair<std::string, std::int64_t>{"alpha", std::int64_t(1)});
    ASSERT_TRUE(ok1);
    ASSERT_EQ(it1->first, std::string("alpha"));
    ASSERT_EQ(static_cast<std::int64_t>(it1->second), std::int64_t(1));

    // insertar clave existente → ok==false, devuelve iter al existente
    auto [it2, ok2] = m.insert(std::pair<std::string, std::int64_t>{"alpha", std::int64_t(99)});
    ASSERT_FALSE(ok2);
    ASSERT_EQ(static_cast<std::int64_t>(it2->second), std::int64_t(1));   // valor sin cambios

    // ── emplace ───────────────────────────────────────────────────────────────
    auto [it3, ok3] = m.emplace("beta", std::int64_t(2));
    ASSERT_TRUE(ok3);
    ASSERT_EQ(it3->first, std::string("beta"));

    // ── try_emplace ───────────────────────────────────────────────────────────
    auto [it4, ok4] = m.try_emplace("gamma", std::int64_t(3));
    ASSERT_TRUE(ok4);
    ASSERT_EQ(static_cast<std::int64_t>(it4->second), std::int64_t(3));

    auto [it5, ok5] = m.try_emplace("gamma", std::int64_t(999));  // ya existe
    ASSERT_FALSE(ok5);
    ASSERT_EQ(static_cast<std::int64_t>(it5->second), std::int64_t(3));  // sin cambios

    // ── insert_or_assign ──────────────────────────────────────────────────────
    auto [it6, ok6] = m.insert_or_assign("delta", std::int64_t(4));
    ASSERT_TRUE(ok6);   // inserción nueva
    ASSERT_EQ(static_cast<std::int64_t>(it6->second), std::int64_t(4));

    auto [it7, ok7] = m.insert_or_assign("delta", std::int64_t(44));
    ASSERT_FALSE(ok7);  // asignación sobre existente
    ASSERT_EQ(static_cast<std::int64_t>(it7->second), std::int64_t(44));
    ASSERT_EQ(m.at("delta"), std::int64_t(44));

    // ── count ─────────────────────────────────────────────────────────────────
    ASSERT_EQ(m.count("alpha"), std::size_t(1));
    ASSERT_EQ(m.count("noexiste"), std::size_t(0));

    // ── find (const y no-const) ───────────────────────────────────────────────
    auto fit = m.find("beta");
    ASSERT_TRUE(fit != m.end());
    ASSERT_EQ(fit->first, std::string("beta"));

    const auto& cm = m;
    auto cfit = cm.find("beta");
    ASSERT_TRUE(cfit != cm.end());
    ASSERT_EQ((*cfit).second, std::int64_t(2));

    ASSERT_FALSE(m.contains("noexiste"));  // find para clave inexistente

    // ── write-back a través del iterador (StoreRef<V> como second) ───────────
    auto mit = m.find("alpha");
    mit->second = std::int64_t(100);          // scalar: StoreRef::operator=
    ASSERT_EQ(m.at("alpha"), std::int64_t(100));

    // ── erase(iterator) → iterator siguiente ─────────────────────────────────
    std::size_t sz_before = m.size();
    auto eit = m.find("gamma");
    m.erase(eit);
    ASSERT_EQ(m.size(), sz_before - 1);
    ASSERT_FALSE(m.contains("gamma"));

    // ── erase(key) → size_type eliminados ────────────────────────────────────
    std::size_t n = m.erase("delta");
    ASSERT_EQ(n, std::size_t(1));
    ASSERT_FALSE(m.contains("delta"));
    ASSERT_EQ(m.erase("noexiste"), std::size_t(0));

    // ── rbegin / rend ─────────────────────────────────────────────────────────
    // quedan: alpha, beta  (inserción: alpha=1ª, beta=2ª)
    std::vector<std::string> rev_keys;
    for (auto rit = m.rbegin(); rit != m.rend(); ++rit)
        rev_keys.push_back((*rit).first);
    ASSERT_SIZE(rev_keys, std::size_t(2));
    ASSERT_EQ(rev_keys[0], std::string("beta"));   // último insertado primero
    ASSERT_EQ(rev_keys[1], std::string("alpha"));

    // ── swap ──────────────────────────────────────────────────────────────────
    akasha::map<std::string, std::int64_t> m2(store, "db/m2");
    m2.insert(std::pair<std::string, std::int64_t>{"x", std::int64_t(9)});
    m.swap(m2);
    ASSERT_SIZE(m, std::size_t(1));
    ASSERT_TRUE(m.contains("x"));
    ASSERT_SIZE(m2, std::size_t(2));
    ASSERT_TRUE(m2.contains("alpha"));

    // ── operator== / operator!= ───────────────────────────────────────────────
    akasha::map<std::string, std::int64_t> m3(store, "db/m3");
    m3.insert(std::pair<std::string, std::int64_t>{"x", std::int64_t(9)});
    ASSERT_TRUE(m == m3);
    m3.insert(std::pair<std::string, std::int64_t>{"y", std::int64_t(0)});
    ASSERT_TRUE(m != m3);

    // ── max_size ──────────────────────────────────────────────────────────────
    ASSERT_TRUE(m.max_size() > std::size_t(0));

    // ── operator-> (WriteProxy): field-level mutation de struct ──────────────
    akasha::map<std::string, Point> mpts(store, "db/mpts");
    mpts.insert("origin", Point{0.0, 0.0, 0.0});
    mpts["origin"]->z = 42.0;  // lee Point, modifica .z, persiste al destruirse
    ASSERT_EQ(mpts.at("origin").z, 42.0);
    ASSERT_EQ(mpts.at("origin").x, 0.0);
}

