from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.scm import Git
import os
import yaml

class AkashaConan(ConanFile):
    name = "akasha"
    version = "1.2.0"
    description = "High-performance hierarchical data store with memory-mapped file persistence"
    url = "https://github.com/yawin123/Akasha"
    license = "MIT"
    topics = {"libraries", "cpp"}
    settings = "os", "arch", "compiler", "build_type"
    
    options = {
        "build_examples": [True, False],
        "build_single_archive": [True, False],
    }
    default_options = {
        "build_examples": False,
        "build_single_archive": False,
    }
    
    exports_sources = "src/*", "include/*", "cmake/*", "CMakeLists.txt", "conandata.yml"
    
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("boost/1.90.0")
    
    def generate(self):
        conandata_file = os.path.join(self.recipe_folder, "conandata.yml")
        if os.path.exists(conandata_file):
            with open(conandata_file, 'r') as f:
                conandata = yaml.safe_load(f)
                if "sources" in conandata:
                    for name, config in conandata["sources"].items():
                        if config.get("type") == "git":
                            # Clonar en build/ en lugar de source_folder
                            git_path = os.path.join(self.folders.build, name)
                            if not os.path.exists(git_path):
                                self.output.info(f"Clonando {name}...")
                                git = Git(self)
                                git.clone(config["url"], target=git_path)
    
    def layout(self):
        # Keep generators in root of output folder (which is build/)
        self.folders.generators = ""
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        cmake = CMake(self)
        cmake.install()
    
    def package_info(self):
        self.cpp_info.libs = ["akasha"]
