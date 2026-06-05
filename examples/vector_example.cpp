#include <iostream>
#include "akasha.hpp"

int main() {
    akasha::Store store;

    auto status = store.load("db", "/tmp/akasha_vector_example.db",
        akasha::FileOptions::create_if_missing | akasha::FileOptions::migrate_if_incompatible);
    if (status != akasha::Status::ok) {
        std::cerr << "Error loading store: " << static_cast<int>(status) << '\n';
        return 1;
    }

    // ── Construction ──────────────────────────────────────────────────────────
    // If the path doesn't exist, creates it with __count__ = 0.
    // If it already exists (e.g., from a previous run), wraps the existing data.
    akasha::vector<int64_t> vec(store, "db/numbers");
    vec.clear();  // Start fresh for this example

    std::cout << "=== akasha::vector<int64_t> ===\n";
    std::cout << "Initial size: " << vec.size() << '\n';
    std::cout << "Empty: " << (vec.empty() ? "yes" : "no") << "\n\n";

    // ── push_back ─────────────────────────────────────────────────────────────
    vec.push_back(10);
    vec.push_back(20);
    vec.push_back(30);

    std::cout << "After push_back(10, 20, 30):\n";
    std::cout << "  size = " << vec.size() << '\n';

    // ── operator[] — mutable proxy ────────────────────────────────────────────
    // vec[i] returns StoreRef<T>: reading converts to T, writing persists to store.
    std::cout << "  vec[0] = " << static_cast<int64_t>(vec[0]) << '\n';
    std::cout << "  vec[1] = " << static_cast<int64_t>(vec[1]) << '\n';
    std::cout << "  vec[2] = " << static_cast<int64_t>(vec[2]) << '\n';

    // ── Modify via proxy ──────────────────────────────────────────────────────
    vec[0] = 100;
    std::cout << "\nAfter vec[0] = 100:\n";
    std::cout << "  vec[0] = " << static_cast<int64_t>(vec[0]) << '\n';

    // ── front / back ──────────────────────────────────────────────────────────
    std::cout << "\nfront = " << static_cast<int64_t>(vec.front()) << '\n';
    std::cout << "back  = " << static_cast<int64_t>(vec.back())  << '\n';

    // ── Modify back via proxy ─────────────────────────────────────────────────
    vec.back() = 999;
    std::cout << "After vec.back() = 999: back = " << static_cast<int64_t>(vec.back()) << '\n';

    // ── resize ────────────────────────────────────────────────────────────────
    vec.resize(6, 0);
    std::cout << "\nAfter resize(6, 0): size = " << vec.size() << '\n';
    for (std::size_t i = 0; i < vec.size(); ++i) {
        std::cout << "  [" << i << "] = " << static_cast<int64_t>(vec[i]) << '\n';
    }

    vec.resize(2);
    std::cout << "\nAfter resize(2): size = " << vec.size() << '\n';
    for (std::size_t i = 0; i < vec.size(); ++i) {
        std::cout << "  [" << i << "] = " << static_cast<int64_t>(vec[i]) << '\n';
    }

    // ── Proxy-to-proxy assignment ─────────────────────────────────────────────
    // vec[0] = vec[1]: reads value from index 1, writes it to index 0.
    akasha::vector<int64_t> vec2(store, "db/numbers");
    vec2.push_back(77);
    vec2.push_back(88);

    akasha::vector<int64_t> other(store, "db/other");
    other.clear();
    other.push_back(11);
    other.push_back(22);

    // Copy value from other[0] to vec2[1] via proxies
    vec2[1] = other[0];
    std::cout << "\nAfter vec2[1] = other[0] (proxy-to-proxy):\n";
    std::cout << "  vec2[1] = " << static_cast<int64_t>(vec2[1]) << " (expected 11)\n";

    // ── Interoperability with std::vector<T> ──────────────────────────────────
    // A std::vector<T> stored via store.set uses the same format (__count__ + indexed keys).
    std::vector<int64_t> sv = {100, 200, 300};
    (void)store.set<std::vector<int64_t>>("db/compat", sv);

    akasha::vector<int64_t> from_std(store, "db/compat");
    std::cout << "\nWrapping a std::vector<T> written via store.set:\n";
    std::cout << "  size = " << from_std.size() << '\n';
    for (std::size_t i = 0; i < from_std.size(); ++i) {
        std::cout << "  [" << i << "] = " << static_cast<int64_t>(from_std[i]) << '\n';
    }

    // ── Out-of-range exception ────────────────────────────────────────────────
    std::cout << "\nOut-of-range access:\n";
    try {
        (void)vec[99];
    } catch (const std::out_of_range& e) {
        std::cout << "  Caught: " << e.what() << '\n';
    }

    // ── clear ─────────────────────────────────────────────────────────────────
    vec.clear();
    std::cout << "\nAfter clear: size = " << vec.size() << ", empty = " << (vec.empty() ? "yes" : "no") << '\n';

    (void)store.unload("db");
    return 0;
}
