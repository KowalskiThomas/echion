# This file is part of "echion" which is released under MIT.
#
# Copyright (c) 2023 Gabriele N. Tornetta <phoenix1987@gmail.com>.

from types import ModuleType


import os
import sys
from pathlib import Path

from setuptools import Extension
from setuptools import find_packages
from setuptools import setup
from setuptools.command.build_py import build_py as _build_py
import importlib.util

def load_helper(relpath: str) -> ModuleType:
    p = Path.cwd() / relpath
    spec = importlib.util.spec_from_file_location("_build_helper", p)
    if spec is None:
        raise ValueError(f"Failed to load spec from {p}")
    
    mod = importlib.util.module_from_spec(spec)
    if mod is None:
        raise ValueError(f"Failed to load module from {p}")

    if spec.loader is None:
        raise ValueError(f"Failed to load loader from {p}")

    spec.loader.exec_module(mod)
    return mod

class build_py(_build_py):
    def run(self):
        helper = load_helper("amalgamate.py")
        src_pkg_dir = Path("")  # adjust if not using src/ layout
        src_pkg_dir.mkdir(parents=True, exist_ok=True)
        helper.generate_file(src_pkg_dir / "echion.cpp")
        super().run()


PLATFORM = sys.platform.lower()

DISABLE_NATIVE = os.environ.get("UNWIND_NATIVE_DISABLE")

LDADD = {
    "linux": ["-l:libunwind.a", "-l:liblzma.a"] if not DISABLE_NATIVE else [],
}

# add option to colorize compiler output
COLORS = [
    "-fdiagnostics-color=always" if PLATFORM == "linux" else "-fcolor-diagnostics"
]

if PLATFORM == "darwin":
    CFLAGS = ["-mmacosx-version-min=10.15"]
else:
    CFLAGS = []

CFLAGS += ["-Wno-unused-function", "-Wno-unused-parameter", "-Wno-reorder", "-Wextra"]

if DISABLE_NATIVE:
    CFLAGS += ["-DUNWIND_NATIVE_DISABLE"]

echionmodule = Extension(
    "echion.core",
    sources=[
        "echion.cpp",
    ],
    include_dirs=["."],
    define_macros=[(f"PL_{PLATFORM.upper()}", None)],
    extra_compile_args=["-std=c++17", "-Wall", "-Wextra"] + CFLAGS + COLORS,
    extra_link_args=LDADD.get(PLATFORM, []),
    libraries=["unwind", "lzma"] if PLATFORM == "linux" and not DISABLE_NATIVE else [],
)

setup(
    name="echion",
    author="Gabriele N. Tornetta",
    description="In-process Python sampling profiler",
    long_description=Path("README.md")
    .read_text()
    .replace(
        'src="art/', 'src="https://raw.githubusercontent.com/P403n1x87/echion/main/art/'
    ),
    ext_modules=[echionmodule],
    entry_points={
        "console_scripts": ["echion=echion.__main__:main"],
    },
    packages=find_packages(exclude=["tests"]),
    cmdclass={"build_py": build_py},
)
