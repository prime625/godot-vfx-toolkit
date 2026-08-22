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
    # Enable NEON for ARM
    if env["arch"] in ["arm64", "arm32"]:
        env.Append(CCFLAGS=["-mfpu=neon"])
    # Link math library
    env.Append(LIBS=["m"])

elif env["platform"] == "ios":
    env.Append(CPPDEFINES=["IOS_ENABLED"])
    # iOS arm64 has NEON implicitly; no extra flags needed.
    # Ensure C++17 is respected (godot-cpp already sets this, but enforce)
    env.Append(CXXFLAGS=["-std=c++17"])
    # Static analysis / safety flags useful for mobile
    env.Append(CCFLAGS=["-fvisibility=hidden"])

# C++17 required for our data structures
env.Append(CXXFLAGS=["-std=c++17"])

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
