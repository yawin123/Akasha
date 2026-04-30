#include "akasha.hpp"
#include <iostream>

/**
 * Demonstrates using akasha::Serializable<T> to store and retrieve
 * user-defined types as structured data.
 *
 * By specializing Serializable<T>, any type is transparently supported
 * with store.set<T>() and store.get<T>().
 */

// --- User-defined types ---

struct Point {
	double x, y, z;
};

struct Color {
	int64_t r, g, b;
	std::string name;
};

// --- Serializable specializations ---

template<>
struct akasha::Serializable<Point> {
	static void serialize(const Point& p, akasha::BatchWriter& bw) {
		(void)bw.set<double>("x", p.x);
		(void)bw.set<double>("y", p.y);
		(void)bw.set<double>("z", p.z);
	}
	static std::optional<Point> deserialize(const akasha::BatchReader& br) {
		auto x = br.get<double>("x");
		auto y = br.get<double>("y");
		auto z = br.get<double>("z");
		if (!x || !y || !z) return std::nullopt;
		return Point{*x, *y, *z};
	}
};

template<>
struct akasha::Serializable<Color> {
	static void serialize(const Color& c, akasha::BatchWriter& bw) {
		(void)bw.set<int64_t>("r", c.r);
		(void)bw.set<int64_t>("g", c.g);
		(void)bw.set<int64_t>("b", c.b);
		(void)bw.set<std::string>("name", c.name);
	}
	static std::optional<Color> deserialize(const akasha::BatchReader& br) {
		auto r = br.get<int64_t>("r");
		auto g = br.get<int64_t>("g");
		auto b = br.get<int64_t>("b");
		auto name = br.get<std::string>("name");
		if (!r || !g || !b || !name) return std::nullopt;
		return Color{*r, *g, *b, *name};
	}
};

// --- Main ---

int main() {
	akasha::Store store;
	
	std::cout << "=== Serializable<T> Demo ===\n\n";
	
	std::string file_path = "/tmp/akasha_serializable.db";
	auto status = store.load("db", file_path, akasha::FileOptions::create_if_missing);
	if (status != akasha::Status::ok) {
		std::cerr << "Failed to load dataset\n";
		return 1;
	}
	
	// --- Point ---
	std::cout << "1. Storing a Point{1.5, 2.7, 3.14}:\n";
	Point p{1.5, 2.7, 3.14};
	if (store.set<Point>("db/origin", p) != akasha::Status::ok) {
		std::cerr << "Failed to store Point\n";
		return 1;
	}
	
	auto retrieved_p = store.get<Point>("db/origin");
	if (retrieved_p) {
		std::cout << "   Retrieved: (" << retrieved_p->x << ", "
		          << retrieved_p->y << ", " << retrieved_p->z << ")\n\n";
	}
	
	// Individual fields are still accessible as scalars
	std::cout << "2. Accessing individual fields as scalars:\n";
	auto x = store.get<double>("db/origin/x");
	if (x) std::cout << "   origin/x = " << *x << "\n\n";
	
	// --- Color ---
	std::cout << "3. Storing a Color{255, 128, 0, \"orange\"}:\n";
	Color c{255, 128, 0, "orange"};
	if (store.set<Color>("db/theme/accent", c) != akasha::Status::ok) {
		std::cerr << "Failed to store Color\n";
		return 1;
	}

	auto retrieved_c = store.get<Color>("db/theme/accent");
	if (retrieved_c) {
		std::cout << "   Retrieved: rgb(" << retrieved_c->r << ", "
	          << retrieved_c->g << ", " << retrieved_c->b << ") \""
	          << retrieved_c->name << "\"\n\n";
	}

	// --- Persistence ---
	std::cout << "4. Persistence test (unload + reload):\n";
	if (store.unload("db") != akasha::Status::ok) {
		std::cerr << "Failed to unload\n";
	}
	if (store.load("db", file_path) != akasha::Status::ok) {
		std::cerr << "Failed to reload\n";
		return 1;
	}

	auto persisted = store.get<Point>("db/origin");
	if (persisted) {
		std::cout << "   After reload: (" << persisted->x << ", "
		          << persisted->y << ", " << persisted->z << ")\n\n";
	}
	
	// --- DatasetView interop ---
	std::cout << "5. Reading through DatasetView:\n";
	auto view = store.get<akasha::Store::DatasetView>("db/theme");
	if (view) {
		auto color = view->get<Color>("accent");
		if (color) {
			std::cout << "   theme/accent: rgb(" << color->r << ", "
			          << color->g << ", " << color->b << ") \""
			          << color->name << "\"\n\n";
		}
	}
	
	std::cout << "=== Done ===\n";
	
	// Cleanup
	if (store.unload("db") != akasha::Status::ok) {
		std::cerr << "Warning: Failed to unload\n";
	}
	if (std::remove(file_path.c_str()) != 0) {
		std::cerr << "Warning: Failed to remove temp file\n";
	}
	return 0;
}
