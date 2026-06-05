// ============================================================================
// Tests: akasha::vector<T> — persistent vector backed by Store
// ============================================================================
#include "test_framework.hpp"
#include "test_common.hpp"

#include "akasha/structs/vector.hpp"
#include <numeric>

// ── Construction & basic size ─────────────────────────────────────────────────

TEST(akasha_vector_empty_on_create) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    ASSERT_SIZE(vec, std::size_t(0));
    ASSERT_TRUE(vec.empty());
}

TEST(akasha_vector_push_back_and_size) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);
    ASSERT_SIZE(vec, std::size_t(3));
    ASSERT_FALSE(vec.empty());
}

// ── Element access (const / non-const) ───────────────────────────────────────

TEST(akasha_vector_read_elements) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(100);
    vec.push_back(200);
    vec.push_back(300);

    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::int64_t(100));
    ASSERT_EQ(cvec[1], std::int64_t(200));
    ASSERT_EQ(cvec[2], std::int64_t(300));
}

TEST(akasha_vector_write_via_proxy) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);
    vec.push_back(2);
    vec[0] = std::int64_t(42);
    vec[1] = std::int64_t(99);

    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::int64_t(42));
    ASSERT_EQ(cvec[1], std::int64_t(99));
}

TEST(akasha_vector_at_bounds_check) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);

    bool threw = false;
    try { (void)vec.at(5); } catch (const std::out_of_range&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(akasha_vector_front_back) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    const auto& cvec = vec;
    ASSERT_EQ(cvec.front(), std::int64_t(10));
    ASSERT_EQ(cvec.back(),  std::int64_t(30));
}

TEST(akasha_vector_front_back_empty_throws) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");

    bool threw_front = false;
    bool threw_back  = false;
    try { (void)vec.front(); } catch (const std::out_of_range&) { threw_front = true; }
    try { (void)vec.back();  } catch (const std::out_of_range&) { threw_back  = true; }
    ASSERT_TRUE(threw_front);
    ASSERT_TRUE(threw_back);
}

// ── Modifiers ────────────────────────────────────────────────────────────────

TEST(akasha_vector_clear) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.clear();

    ASSERT_SIZE(vec, std::size_t(0));
    ASSERT_TRUE(vec.empty());
}

TEST(akasha_vector_resize_grow) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);
    vec.push_back(2);
    vec.resize(5, std::int64_t(99));

    ASSERT_SIZE(vec, std::size_t(5));
    const auto& cvec = vec;
    ASSERT_EQ(cvec[2], std::int64_t(99));
    ASSERT_EQ(cvec[4], std::int64_t(99));
}

TEST(akasha_vector_resize_shrink) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    for (int i = 0; i < 5; ++i) vec.push_back(std::int64_t(i));
    vec.resize(2);

    ASSERT_SIZE(vec, std::size_t(2));
    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::int64_t(0));
    ASSERT_EQ(cvec[1], std::int64_t(1));
}

// ── Strings ──────────────────────────────────────────────────────────────────

TEST(akasha_vector_strings) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::string> vec(store, "db/v");
    vec.push_back("hello");
    vec.push_back("world");

    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::string("hello"));
    ASSERT_EQ(cvec[1], std::string("world"));
}

// ── Persistence across Store instances ───────────────────────────────────────

TEST(akasha_vector_persistence) {
    TempFile temp;
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
        akasha::vector<std::int64_t> vec(store, "db/v");
        vec.push_back(7);
        vec.push_back(14);
        vec.push_back(21);
    }
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::none), akasha::Status::ok);
        akasha::vector<std::int64_t> vec(store, "db/v");
        ASSERT_SIZE(vec, std::size_t(3));
        const auto& cvec = vec;
        ASSERT_EQ(cvec[0], std::int64_t(7));
        ASSERT_EQ(cvec[1], std::int64_t(14));
        ASSERT_EQ(cvec[2], std::int64_t(21));
    }
}

// ── Attach to existing std::vector<T> data ───────────────────────────────────

TEST(akasha_vector_attach_to_stl_vector) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    std::vector<std::int64_t> stl = {10, 20, 30};
    ASSERT_EQ(store.set<std::vector<std::int64_t>>("db/v", stl), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    ASSERT_SIZE(vec, std::size_t(3));
    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::int64_t(10));
    ASSERT_EQ(cvec[1], std::int64_t(20));
    ASSERT_EQ(cvec[2], std::int64_t(30));
}

// ── store.set / store.get interop ────────────────────────────────────────────

TEST(akasha_vector_set_get_same_dataset) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> src(store, "db/src");
    src.push_back(1);
    src.push_back(2);
    src.push_back(3);

    // Source and destination in the same dataset — exercises the snapshot path
    ASSERT_EQ(store.set<akasha::vector<std::int64_t>>("db/dst", src), akasha::Status::ok);

    auto dst = store.get<akasha::vector<std::int64_t>>("db/dst");
    ASSERT_TRUE(dst.has_value());
    ASSERT_SIZE(*dst, std::size_t(3));
    const auto& cdst = *dst;
    ASSERT_EQ(cdst[0], std::int64_t(1));
    ASSERT_EQ(cdst[1], std::int64_t(2));
    ASSERT_EQ(cdst[2], std::int64_t(3));
}

TEST(akasha_vector_set_get_different_datasets) {
    TempFile temp_a("test_vec_a.mmap");
    TempFile temp_b("test_vec_b.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("a", temp_a.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);
    ASSERT_EQ(store.load("b", temp_b.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> src(store, "a/src");
    src.push_back(10);
    src.push_back(20);

    // Source in "a", destination in "b" — exercises the direct (no-snapshot) path
    ASSERT_EQ(store.set<akasha::vector<std::int64_t>>("b/dst", src), akasha::Status::ok);

    auto dst = store.get<akasha::vector<std::int64_t>>("b/dst");
    ASSERT_TRUE(dst.has_value());
    ASSERT_SIZE(*dst, std::size_t(2));
    const auto& cdst = *dst;
    ASSERT_EQ(cdst[0], std::int64_t(10));
    ASSERT_EQ(cdst[1], std::int64_t(20));
}

TEST(akasha_vector_set_empty) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> src(store, "db/src");
    // empty vector
    ASSERT_EQ(store.set<akasha::vector<std::int64_t>>("db/dst", src), akasha::Status::ok);

    auto dst = store.get<akasha::vector<std::int64_t>>("db/dst");
    ASSERT_TRUE(dst.has_value());
    ASSERT_SIZE(*dst, std::size_t(0));
    ASSERT_TRUE(dst->empty());
}

// ── Proxy-to-proxy assignment ─────────────────────────────────────────────────

TEST(akasha_vector_proxy_to_proxy) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> a(store, "db/a");
    akasha::vector<std::int64_t> b(store, "db/b");
    a.push_back(55);
    b.push_back(0);

    b[0] = a[0];  // proxy-to-proxy: reads from a[0], writes to b[0]

    const auto& cb = b;
    ASSERT_EQ(cb[0], std::int64_t(55));
}

// ── Iterators ─────────────────────────────────────────────────────────────────

TEST(akasha_vector_range_for_const) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    const auto& cvec = vec;
    std::int64_t sum = 0;
    for (auto val : cvec) sum += val;
    ASSERT_EQ(sum, std::int64_t(6));
}

TEST(akasha_vector_range_for_write) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    for (auto ref : vec) ref = ref + std::int64_t(10);

    const auto& cvec = vec;
    ASSERT_EQ(cvec[0], std::int64_t(11));
    ASSERT_EQ(cvec[1], std::int64_t(12));
    ASSERT_EQ(cvec[2], std::int64_t(13));
}

TEST(akasha_vector_iterator_arithmetic) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    for (int i = 0; i < 5; ++i) vec.push_back(std::int64_t(i * 10));

    auto it = vec.begin() + 2;
    ASSERT_EQ(static_cast<std::int64_t>(*it), std::int64_t(20));

    auto dist = vec.end() - vec.begin();
    ASSERT_EQ(dist, std::ptrdiff_t(5));
}

TEST(akasha_vector_cbegin_cend) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(7);
    vec.push_back(14);

    std::int64_t first = *vec.cbegin();
    ASSERT_EQ(first, std::int64_t(7));
    ASSERT_EQ(vec.cend() - vec.cbegin(), std::ptrdiff_t(2));
}

TEST(akasha_vector_iterator_empty) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    ASSERT_TRUE(vec.begin() == vec.end());
    ASSERT_TRUE(vec.cbegin() == vec.cend());
}

TEST(akasha_vector_std_copy_to_stl) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> vec(store, "db/v");
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    std::vector<std::int64_t> out;
    std::copy(vec.cbegin(), vec.cend(), std::back_inserter(out));

    ASSERT_SIZE(out, std::size_t(3));
    ASSERT_EQ(out[0], std::int64_t(10));
    ASSERT_EQ(out[1], std::int64_t(20));
    ASSERT_EQ(out[2], std::int64_t(30));
}

TEST(container_vector_construction_forms) {
    TempFile temp("test_container_ctor_vec.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // store.get en path inexistente → nullopt
    ASSERT_FALSE(store.get<akasha::vector<std::int64_t>>("db/v").has_value());

    // Constructor directo en path nuevo → vacío; no lanza ni devuelve nullopt
    akasha::vector<std::int64_t> v(store, "db/v");
    ASSERT_TRUE(v.empty());

    // Constructor directo en path existente → lee los datos sin borrarlos
    v.push_back(std::int64_t(1));
    v.push_back(std::int64_t(2));
    akasha::vector<std::int64_t> v2(store, "db/v");
    ASSERT_SIZE(v2, std::size_t(2));
    ASSERT_EQ(v2[0], std::int64_t(1));
    ASSERT_EQ(v2[1], std::int64_t(2));

    // store.get en path existente → optional con los datos correctos
    auto opt = store.get<akasha::vector<std::int64_t>>("db/v");
    ASSERT_TRUE(opt.has_value());
    ASSERT_SIZE(*opt, std::size_t(2));

    // Interop: store.set<std::vector> → store.get<akasha::vector>
    (void)store.set<std::vector<std::int64_t>>("db/v2", {7, 8, 9});
    auto opt2 = store.get<akasha::vector<std::int64_t>>("db/v2");
    ASSERT_TRUE(opt2.has_value());
    ASSERT_SIZE(*opt2, std::size_t(3));

    // Interop inverso: akasha::vector → store.get<std::vector>
    auto opt3 = store.get<std::vector<std::int64_t>>("db/v");
    ASSERT_TRUE(opt3.has_value());
    ASSERT_SIZE(*opt3, std::size_t(2));
}

TEST(vector_push_pop_front_back) {
    TempFile temp("test_vector_deque.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    akasha::vector<std::int64_t> v(store, "db/v");
    v.push_back(std::int64_t(2));
    v.push_front(std::int64_t(1));   // [1, 2]
    v.push_back(std::int64_t(3));    // [1, 2, 3]
    v.push_front(std::int64_t(0));   // [0, 1, 2, 3]

    ASSERT_SIZE(v, std::size_t(4));
    ASSERT_EQ(v[0], std::int64_t(0));
    ASSERT_EQ(v[1], std::int64_t(1));
    ASSERT_EQ(v[2], std::int64_t(2));
    ASSERT_EQ(v[3], std::int64_t(3));

    v.pop_front();  // [1, 2, 3]
    ASSERT_SIZE(v, std::size_t(3));
    ASSERT_EQ(v[0], std::int64_t(1));
    ASSERT_EQ(v[2], std::int64_t(3));

    v.pop_back();   // [1, 2]
    ASSERT_SIZE(v, std::size_t(2));
    ASSERT_EQ(v[0], std::int64_t(1));
    ASSERT_EQ(v[1], std::int64_t(2));

    v.pop_back();   // [1]
    v.pop_front();  // []
    ASSERT_TRUE(v.empty());
}

TEST(akasha_vector_std_api) {
    TempFile temp("test_vector_std_api.mmap");
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

    // ── default ctor + move assignment ───────────────────────────────────────
    akasha::vector<std::int64_t> v;
    v = akasha::vector<std::int64_t>(store, "db/v");
    ASSERT_TRUE(v.empty());

    // ── assign(count, value) ──────────────────────────────────────────────────
    v.assign(3, std::int64_t(7));
    ASSERT_SIZE(v, std::size_t(3));
    ASSERT_EQ(v[0], std::int64_t(7));
    ASSERT_EQ(v[2], std::int64_t(7));

    // ── assign(first, last) ───────────────────────────────────────────────────
    std::vector<std::int64_t> src = {10, 20, 30, 40};
    v.assign(src.begin(), src.end());
    ASSERT_SIZE(v, std::size_t(4));
    ASSERT_EQ(v[1], std::int64_t(20));
    ASSERT_EQ(v[3], std::int64_t(40));

    // ── assign(initializer_list) ──────────────────────────────────────────────
    v.assign({std::int64_t(1), std::int64_t(2), std::int64_t(3)});
    ASSERT_SIZE(v, std::size_t(3));
    ASSERT_EQ(v[0], std::int64_t(1));

    // ── insert(pos, value) ────────────────────────────────────────────────────
    // [1, 2, 3] → insert 99 at index 1 → [1, 99, 2, 3]
    auto it = v.insert(v.begin() + 1, std::int64_t(99));
    ASSERT_SIZE(v, std::size_t(4));
    ASSERT_EQ(static_cast<std::int64_t>(*it), std::int64_t(99));
    ASSERT_EQ(v[0], std::int64_t(1));
    ASSERT_EQ(v[1], std::int64_t(99));
    ASSERT_EQ(v[2], std::int64_t(2));
    ASSERT_EQ(v[3], std::int64_t(3));

    // ── insert(pos, count, value) ─────────────────────────────────────────────
    // [1, 99, 2, 3] → insert 2×55 at begin → [55, 55, 1, 99, 2, 3]
    v.insert(v.begin(), std::size_t(2), std::int64_t(55));
    ASSERT_SIZE(v, std::size_t(6));
    ASSERT_EQ(v[0], std::int64_t(55));
    ASSERT_EQ(v[1], std::int64_t(55));
    ASSERT_EQ(v[2], std::int64_t(1));

    // ── erase(pos) ────────────────────────────────────────────────────────────
    // [55, 55, 1, 99, 2, 3] → erase index 0 → [55, 1, 99, 2, 3]
    auto after = v.erase(v.begin());
    ASSERT_SIZE(v, std::size_t(5));
    ASSERT_EQ(static_cast<std::int64_t>(*after), std::int64_t(55));
    ASSERT_EQ(v[0], std::int64_t(55));
    ASSERT_EQ(v[1], std::int64_t(1));

    // ── erase(first, last) ────────────────────────────────────────────────────
    // [55, 1, 99, 2, 3] → erase [1..3) → [55, 2, 3]
    v.erase(v.begin() + 1, v.begin() + 3);
    ASSERT_SIZE(v, std::size_t(3));
    ASSERT_EQ(v[0], std::int64_t(55));
    ASSERT_EQ(v[1], std::int64_t(2));
    ASSERT_EQ(v[2], std::int64_t(3));

    // ── emplace_back ──────────────────────────────────────────────────────────
    v.emplace_back(std::int64_t(42));
    ASSERT_SIZE(v, std::size_t(4));
    ASSERT_EQ(v[3], std::int64_t(42));

    // ── rbegin / rend ─────────────────────────────────────────────────────────
    // [55, 2, 3, 42] — reversed: [42, 3, 2, 55]
    std::vector<std::int64_t> rev;
    for (auto rit = v.rbegin(); rit != v.rend(); ++rit)
        rev.push_back(static_cast<std::int64_t>(*rit));
    ASSERT_SIZE(rev, std::size_t(4));
    ASSERT_EQ(rev[0], std::int64_t(42));
    ASSERT_EQ(rev[3], std::int64_t(55));

    // ── swap ──────────────────────────────────────────────────────────────────
    akasha::vector<std::int64_t> w(store, "db/w");
    w.push_back(std::int64_t(100));
    v.swap(w);
    ASSERT_SIZE(v, std::size_t(1));
    ASSERT_EQ(v[0], std::int64_t(100));
    ASSERT_SIZE(w, std::size_t(4));

    // ── operator== / operator!= ───────────────────────────────────────────────
    akasha::vector<std::int64_t> v2(store, "db/v2");
    v2.push_back(std::int64_t(100));
    ASSERT_TRUE(v == v2);
    v2.push_back(std::int64_t(200));
    ASSERT_TRUE(v != v2);

    // ── STL read-only algorithms (min/max) via random-access iterator ─────────
    akasha::vector<std::int64_t> s(store, "db/s");
    s.assign({std::int64_t(5), std::int64_t(1), std::int64_t(4), std::int64_t(2), std::int64_t(3)});
    auto min_it = std::min_element(s.cbegin(), s.cend());
    auto max_it = std::max_element(s.cbegin(), s.cend());
    ASSERT_EQ(*min_it, std::int64_t(1));
    ASSERT_EQ(*max_it, std::int64_t(5));
    auto sum_val = std::accumulate(s.cbegin(), s.cend(), std::int64_t(0));
    ASSERT_EQ(sum_val, std::int64_t(15));

    // ── max_size ──────────────────────────────────────────────────────────────
    ASSERT_TRUE(v.max_size() > std::size_t(0));

    // ── operator-> (WriteProxy): field-level mutation de struct ──────────────
    akasha::vector<Point> pts(store, "db/pts");
    pts.push_back(Point{1.0, 2.0, 3.0});
    pts[0]->x = 99.0;          // lee Point, modifica .x, persiste al destruirse
    const auto& cpts = pts;
    ASSERT_EQ(cpts[0].x, 99.0);
    ASSERT_EQ(cpts[0].y, 2.0);
    ASSERT_EQ(cpts[0].z, 3.0);
}
