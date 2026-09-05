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



## License

MIT
