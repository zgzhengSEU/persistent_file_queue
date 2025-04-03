import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class PersistentFileQueueRecipe(ConanFile):
    name = "persistent_file_queue"
    version = "1.0"
    # package_type = "library"

    # Optional metadata
    license = "MIT"
    author = "SEUGarfield zzg.seu@gmail.com"
    url = ""
    description = "A persistent file queue library"
    topics = ("")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "tests/*", "cmake/*"

    def requirements(self):
        self.requires("spdlog/1.15.1")
        self.test_requires("gtest/1.16.0")
    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        # this check is not needed if using CTest instead of gtest
        # in that case just call to cmake.test() and it will be skipped
        # if tools.build:skip_test=True
        if not self.conf.get("tools.build:skip_test", default=False):
            test_folder = os.path.join("tests")
            if self.settings.os == "Windows":
                test_folder = os.path.join("tests", str(self.settings.build_type))
            self.run(os.path.join(test_folder, "test_persistent_file_queue"))

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["persistent_file_queue"]

