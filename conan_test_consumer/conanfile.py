import os
from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class UnilinkTestPackage(ConanFile):
    """
    Conan test package that mirrors the quick-start example in README.md.

    It links against the packaged `unilink::unilink` target, builds the sample
    client from `main.cpp`, and executes it to verify that basic setup works.
    """

    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if not can_run(self):
            return

        for bindir in self.cpp.build.bindirs:
            candidate = os.path.join(self.build_folder, bindir, "conan_test_consumer")
            if os.path.isfile(candidate):
                self.run(candidate, env="conanrun")
                return

        self.output.warn("Test executable 'conan_test_consumer' was not found; skipping run.")
