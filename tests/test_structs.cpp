// ============================================================================
// Tests: Serializable<T>, generic numeric types, nested composition (F3.4+)
// ============================================================================
#include "test_framework.hpp"
#include "test_common.hpp"

// ---- User types ----

struct Point {
	double x, y, z;
	bool operator==(const Point& o) const { return x == o.x && y == o.y && z == o.z; }
};

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

struct Camera {
	Point position;
	Point look_at;
	double fov;
	bool operator==(const Camera& o) const {
		return position == o.position && look_at == o.look_at && fov == o.fov;
	}
};

template<>
struct akasha::Serializable<Camera> {
	static void serialize(const Camera& c, akasha::BatchWriter& bw) {
		(void)bw.set<Point>("position", c.position);
		(void)bw.set<Point>("look_at", c.look_at);
		(void)bw.set<double>("fov", c.fov);
	}
	static std::optional<Camera> deserialize(const akasha::BatchReader& br) {
		auto pos = br.get<Point>("position");
		auto look = br.get<Point>("look_at");
		auto fov = br.get<double>("fov");
		if (!pos || !look || !fov) return std::nullopt;
		return Camera{*pos, *look, *fov};
	}
};

struct Scene {
	std::string name;
	bool active;
	int64_t version;
	Camera camera;
	std::vector<double> ambient;
	std::vector<std::string> tags;
	bool operator==(const Scene& o) const {
		return name == o.name && active == o.active && version == o.version &&
		       camera == o.camera && ambient == o.ambient && tags == o.tags;
	}
};

template<>
struct akasha::Serializable<Scene> {
	static void serialize(const Scene& s, akasha::BatchWriter& bw) {
		(void)bw.set<std::string>("name", s.name);
		(void)bw.set<bool>("active", s.active);
		(void)bw.set<int64_t>("version", s.version);
		(void)bw.set<Camera>("camera", s.camera);
		(void)bw.set<std::vector<double>>("ambient", s.ambient);
		(void)bw.set<std::vector<std::string>>("tags", s.tags);
	}
	static std::optional<Scene> deserialize(const akasha::BatchReader& br) {
		auto name = br.get<std::string>("name");
		auto active = br.get<bool>("active");
		auto version = br.get<int64_t>("version");
		auto camera = br.get<Camera>("camera");
		auto ambient = br.get<std::vector<double>>("ambient");
		auto tags = br.get<std::vector<std::string>>("tags");
		if (!name || !active || !version || !camera || !ambient || !tags) return std::nullopt;
		return Scene{*name, *active, *version, *camera, *ambient, *tags};
	}
};

// ---- Basic Serializable ----

TEST(structs_point_roundtrip) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	Point original{1.5, 2.7, 3.14};
	ASSERT_EQ(store.set<Point>("db/point", original), akasha::Status::ok);

	auto retrieved = store.get<Point>("db/point");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(*retrieved, original);
}

TEST(structs_get_nonexistent) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	auto p = store.get<Point>("db/nonexistent");
	ASSERT_FALSE(p.has_value());
}

TEST(structs_partial_data_returns_nullopt) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	ASSERT_EQ(store.set<double>("db/partial/x", 1.0), akasha::Status::ok);
	ASSERT_EQ(store.set<double>("db/partial/y", 2.0), akasha::Status::ok);

	auto p = store.get<Point>("db/partial");
	ASSERT_FALSE(p.has_value());
}

// ---- Point collection: store N, retrieve, overwrite, copy subtree, persist ----

TEST(structs_point_collection) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	Point vertices[] = {
		{0.0, 0.0, 0.0},
		{1.0, 0.0, 0.0},
		{0.5, 1.0, 0.0},
		{0.5, 0.5, 1.0},
	};

	// Store 4 vertices
	for (int i = 0; i < 4; ++i) {
		ASSERT_EQ(store.set<Point>("db/mesh/v/" + std::to_string(i), vertices[i]), akasha::Status::ok);
	}

	// Retrieve all
	for (int i = 0; i < 4; ++i) {
		auto p = store.get<Point>("db/mesh/v/" + std::to_string(i));
		ASSERT_TRUE(p.has_value());
		ASSERT_EQ(*p, vertices[i]);
	}

	// Overwrite one, check others unchanged
	Point updated{9.9, 8.8, 7.7};
	ASSERT_EQ(store.set<Point>("db/mesh/v/1", updated), akasha::Status::ok);

	auto v0 = store.get<Point>("db/mesh/v/0");
	ASSERT_TRUE(v0.has_value());
	ASSERT_EQ(*v0, vertices[0]);

	auto v1 = store.get<Point>("db/mesh/v/1");
	ASSERT_TRUE(v1.has_value());
	ASSERT_EQ(*v1, updated);

	// Copy entire subtree via DatasetView
	auto mesh_view = store.get<akasha::Store::DatasetView>("db/mesh");
	ASSERT_TRUE(mesh_view.has_value());
	ASSERT_EQ(store.set<akasha::Store::DatasetView>("db/backup", *mesh_view), akasha::Status::ok);

	auto copied0 = store.get<Point>("db/backup/v/0");
	ASSERT_TRUE(copied0.has_value());
	ASSERT_EQ(*copied0, vertices[0]);

	auto copied1 = store.get<Point>("db/backup/v/1");
	ASSERT_TRUE(copied1.has_value());
	ASSERT_EQ(*copied1, updated);

	// Persist: unload/reload, verify all survive
	ASSERT_EQ(store.unload("db"), akasha::Status::ok);
	ASSERT_EQ(store.load("db", temp.path()), akasha::Status::ok);

	for (int i = 0; i < 4; ++i) {
		auto p = store.get<Point>("db/mesh/v/" + std::to_string(i));
		ASSERT_TRUE(p.has_value());
		ASSERT_EQ(*p, (i == 1 ? updated : vertices[i]));
	}
}

// ---- Complex nested struct: Scene → Camera → Point + vectors + scalars ----

TEST(structs_scene_roundtrip) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	Scene original{
		"Main Scene", true, 42,
		Camera{{0.0, 5.0, -10.0}, {0.0, 0.0, 0.0}, 60.0},
		{0.2, 0.3, 0.5},
		{"outdoor", "winter", "night"}
	};
	ASSERT_EQ(store.set<Scene>("db/scene", original), akasha::Status::ok);

	auto retrieved = store.get<Scene>("db/scene");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(*retrieved, original);
}

TEST(structs_scene_deep_field_access) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	Scene scene{
		"Test", false, 7,
		Camera{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, 90.0},
		{1.0, 0.0, 0.0},
		{"indoor"}
	};
	ASSERT_EQ(store.set<Scene>("db/s", scene), akasha::Status::ok);

	// Top-level fields
	auto name = store.get<std::string>("db/s/name");
	ASSERT_TRUE(name.has_value());
	ASSERT_EQ(*name, "Test");

	auto active = store.get<bool>("db/s/active");
	ASSERT_TRUE(active.has_value());
	ASSERT_EQ(*active, false);

	// Nested Camera
	auto cam = store.get<Camera>("db/s/camera");
	ASSERT_TRUE(cam.has_value());
	ASSERT_EQ(cam->position, scene.camera.position);

	// Three levels deep: Scene → Camera → Point → field
	auto cam_pos_x = store.get<double>("db/s/camera/position/x");
	ASSERT_TRUE(cam_pos_x.has_value());
	ASSERT_NEAR(*cam_pos_x, 1.0, 0.001);

	auto cam_fov = store.get<double>("db/s/camera/fov");
	ASSERT_TRUE(cam_fov.has_value());
	ASSERT_NEAR(*cam_fov, 90.0, 0.001);
}

TEST(structs_scene_persistence) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	Scene original{
		"Persistent", true, 100,
		Camera{{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, 45.0},
		{0.1, 0.2, 0.3},
		{"tag1", "tag2"}
	};
	ASSERT_EQ(store.set<Scene>("db/scene", original), akasha::Status::ok);

	ASSERT_EQ(store.unload("db"), akasha::Status::ok);
	ASSERT_EQ(store.load("db", temp.path()), akasha::Status::ok);

	auto retrieved = store.get<Scene>("db/scene");
	ASSERT_TRUE(retrieved.has_value());
	ASSERT_EQ(*retrieved, original);
}



