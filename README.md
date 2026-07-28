# Godot VFX Toolkit (Android)

Custom 3D character pipeline for Godot 4.3+ on Android. 
Built as a C++ GDExtension with Godot handling the UI.

## Architecture

```
┌─────────────────────────────────────┐
│         Godot UI (GDScript)         │
│  Viewport · Timeline · UV Editor    │
└──────────────┬──────────────────────┘
               │ GDExtension API
┌──────────────▼──────────────────────┐
│      C++ Core (libvfx_toolkit)      │
│  CustomMesh · MixamoSkeleton        │
│  WeightPaint · Animator · glTF I/O  │
└─────────────────────────────────────┘
```

## Project Structure

```
src/
  register_types.cpp/h       # GDExtension entry point
  vfx_mesh.h/cpp             # Custom half-edge mesh (NOT Godot ArrayMesh)
  vfx_skeleton.h/cpp         # Mixamo-standard bone hierarchy + solvers
  vfx_skin.h/cpp             # Weight painting + GPU skinning data
  vfx_animator.h/cpp         # Keyframe curves + sampling
  vfx_gltf_exporter.h/cpp    # glTF 2.0 / glb export (VAT support)
  vfx_math.h/cpp             # Linear algebra helpers
  vfx_editor_node.h/cpp      # Godot Node3D wrapper for the viewport
```

## Build (Android)

### Prerequisites
- Python 3.11+
- SCons: `pip install scons`
- Android NDK r26d

### Clone & Setup
```bash
git clone --recursive https://github.com/YOURNAME/godot_vfx_toolkit.git
cd godot_vfx_toolkit
```

If you forgot `--recursive`:
```bash
git submodule update --init --recursive
```

### Build ARM64
```bash
scons platform=android arch=arm64 target=template_debug -j$(nproc)
scons platform=android arch=arm64 target=template_release -j$(nproc)
```

### Build ARM32
```bash
scons platform=android arch=arm32 target=template_debug -j$(nproc)
```

## CI/CD

GitHub Actions builds both `arm64` and `arm32` on every push to `main`.
Artifacts are packaged as `godot_vfx_toolkit_android.zip`.

## License

MIT
