import os
import sys
import subprocess
from pathlib import Path
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext

class CMakeExtension(Extension):
    def __init__(self, name):
        super().__init__(name, sources=[])

class CMakeBuild(build_ext):
    def build_extension(self, ext):
        if not isinstance(ext, CMakeExtension):
            super().build_extension(ext)
            return

        extdir = Path(self.get_ext_fullpath(ext.name)).parent.absolute()
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-DBUILD_PYTHON_BINDINGS=ON",
            "-DBUILD_EXAMPLES=OFF",
        ]

        build_args = []

        # Set build type
        cfg = "Debug" if self.debug else "Release"
        build_args += ["--config", cfg]
        cmake_args += [f"-DCMAKE_BUILD_TYPE={cfg}"]

        # Parallel build
        if hasattr(self, "parallel") and self.parallel:
            build_args += [f"-j{self.parallel}"]

        build_temp = Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)

        # Run CMake
        subprocess.run(
            ["cmake", str(Path(__file__).parent)] + cmake_args,
            cwd=build_temp,
            check=True
        )

        subprocess.run(
            ["cmake", "--build", "."] + build_args,
            cwd=build_temp,
            check=True
        )

setup(
    name="flowgen",
    version="1.0.0",
    author="FlowGen Team",
    description="High-performance network flow record generator",
    long_description=open("README.md").read() if os.path.exists("README.md") else "",
    long_description_content_type="text/markdown",
    packages=find_packages(),
    ext_modules=[CMakeExtension("flowgen._flowgen_core")],
    cmdclass={"build_ext": CMakeBuild},
    install_requires=[
        "pyyaml>=6.0",
        "numpy>=1.20",
    ],
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: C++",
    ],
)
