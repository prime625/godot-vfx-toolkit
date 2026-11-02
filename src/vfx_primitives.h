#ifndef VFX_PRIMITIVES_H
#define VFX_PRIMITIVES_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include "vfx_mesh.h"

using namespace godot;

// ============================================================================
// Blender-style parametric primitive builders.
//
// Every builder returns a ready-to-edit VFXMesh (twins linked, normals and
// UVs set, bounds valid) so the result works immediately with the existing
// modeling tools (extrude, inset, bevel, UV editor, ...).
//
// Parameter dictionaries use Blender-like keys, e.g.:
//   Cube:       {"size": 1.0, "subdivisions": 1}
//   UV Sphere:  {"radius": 0.5, "segments": 32, "rings": 16}
//   Cylinder:   {"radius_top": 0.5, "radius_bottom": 0.5, "depth": 1.0,
//                "sides": 32, "cap_top": true, "cap_bottom": true}
//   Torus:      {"major_radius": 0.5, "minor_radius": 0.25,
//                "major_segments": 32, "minor_segments": 12}
// ============================================================================

class VFXPrimitives : public RefCounted {
    GDCLASS(VFXPrimitives, RefCounted)

public:
    enum PrimitiveType {
        PRIM_PLANE = 0,
        PRIM_CUBE = 1,
        PRIM_UV_SPHERE = 2,
        PRIM_ICO_SPHERE = 3,
        PRIM_CYLINDER = 4,
        PRIM_CONE = 5,
        PRIM_TORUS = 6,
        PRIM_CIRCLE = 7,
    };

protected:
    static void _bind_methods();

public:
    VFXPrimitives();
    ~VFXPrimitives();

    // Generic entry point used by scene/editor integration.
    static Ref<VFXMesh> create(int primitive_type, const Dictionary& params);
    static Ref<VFXMesh> create_named(const String& primitive_name, const Dictionary& params);

    static String get_primitive_name(int primitive_type);
    static int get_primitive_type_count();

    // === INDIVIDUAL BUILDERS ===
    // All parameters mirror Blender's "Add > Mesh" operators.

    static Ref<VFXMesh> create_plane(float size = 1.0f, int subdivisions = 1);
    static Ref<VFXMesh> create_cube(float size = 1.0f, int subdivisions = 1);

    static Ref<VFXMesh> create_uv_sphere(float radius = 0.5f, int segments = 32, int rings = 16);
    static Ref<VFXMesh> create_ico_sphere(float radius = 0.5f, int subdivisions = 1);

    static Ref<VFXMesh> create_cylinder(float radius_top = 0.5f, float radius_bottom = 0.5f,
                                        float depth = 1.0f, int sides = 32,
                                        bool cap_top = true, bool cap_bottom = true);
    static Ref<VFXMesh> create_cone(float radius1 = 0.5f, float radius2 = 0.0f,
                                    float depth = 1.0f, int sides = 32);

    static Ref<VFXMesh> create_torus(float major_radius = 0.5f, float minor_radius = 0.25f,
                                     int major_segments = 32, int minor_segments = 12);

    static Ref<VFXMesh> create_circle(float radius = 0.5f, int sides = 32);

    // === BLENDER-STYLE "LAST OPERATION" SUPPORT ===
    // Default parameter set for the redo panel (F9 in Blender).
    static Dictionary get_default_params(int primitive_type);
};

VARIANT_ENUM_CAST(VFXPrimitives::PrimitiveType);

#endif
