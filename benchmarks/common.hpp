#pragma once

// ============================================================================
// Benchmark Common Utilities
// ============================================================================

#include "akasha.hpp"

#include <filesystem>
#include <atomic>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

/**
 * @brief Temporary file RAII wrapper for benchmarks
 */
class TempFile {
private:
    std::string m_path;

    // Helper to generate a unique filename
    static std::string generate_unique_path(const std::string& suffix) {
        static std::atomic<int> counter{0};
        return "/tmp/akasha_benchmark_" + std::to_string(getpid())
               + "_" + std::to_string(counter++) + suffix;
    }

public:
    explicit TempFile(const std::string& suffix = ".db") {
        // Create temp file in /tmp with unique name
        m_path = generate_unique_path(suffix);
        
        // Clean up if exists
        if (fs::exists(m_path)) {
            fs::remove(m_path);
        }
    }

    // Constructor that copies another TempFile
    explicit TempFile(const TempFile& other) {
        m_path = generate_unique_path(".db");
        
        // Clean up if exists
        if (fs::exists(m_path)) {
            fs::remove(m_path);
        }
        
        // Copy the file from other to this
        if (fs::exists(other.m_path)) {
            fs::copy_file(other.m_path, m_path, fs::copy_options::overwrite_existing);
        }
    }

    ~TempFile() {
        if (fs::exists(m_path)) {
            fs::remove(m_path);
        }
    }

    const std::string& path() const { return m_path; }

    // Allow copy constructor (to copy from another TempFile)
    // but delete copy assignment (to prevent accidental overwrites)
    TempFile& operator=(const TempFile&) = delete;

    // Allow move operations
    TempFile(TempFile&& other) noexcept : m_path(std::move(other.m_path)) {
        other.m_path.clear();  // Prevent destructor from deleting the file twice
    }

    TempFile& operator=(TempFile&& other) noexcept {
        if (this != &other) {
            // Clean up existing file
            if (fs::exists(m_path)) {
                fs::remove(m_path);
            }
            m_path = std::move(other.m_path);
            other.m_path.clear();
        }
        return *this;
    }
};

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