#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# Add our source files
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

# Platform-specific flags
if env["platform"] == "android":
    env.Append(CPPDEFINES=["ANDROID_ENABLED"])
    # NEON: arm64 has it by default. arm32 uses softfp.
    if env["arch"] == "arm32":
        env.Append(CCFLAGS=["-mfloat-abi=softfp", "-mfpu=neon"])
    # Android NDK links math automatically, no need for -lm

# C++17 required
env.Append(CXXFLAGS=["-std=c++17"])

# Use ccache if available (massively speeds up rebuilds)
env["CC"] = "ccache " + env["CC"] if os.system("which ccache > /dev/null 2>&1") == 0 else env["CC"]
env["CXX"] = "ccache " + env["CXX"] if os.system("which ccache > /dev/null 2>&1") == 0 else env["CXX"]

# Create the library
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "demo/addons/vfx_toolkit/libvfx_toolkit.{}.{}.framework/libvfx_toolkit.{}.{}".format(
            env["platform"], env["arch"], env["platform"], env["arch"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "demo/addons/vfx_toolkit/libvfx_toolkit{}{}".format(
            env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)
