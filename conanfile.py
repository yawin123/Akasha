from conan import ConanFile

class AkashaConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("boost/1.90.0")
        self.requires("flatbuffers/25.9.23")
