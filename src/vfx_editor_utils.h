#ifndef VFX_EDITOR_UTILS_H
#define VFX_EDITOR_UTILS_H

#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

using namespace godot;

namespace vfx_editor {

// ============================================================================
// GEOMETRY BUILDERS — append to packed arrays for immediate-mode mesh gen
// ============================================================================
void append_cylinder(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& from, const Vector3& to, float radius, int segs, const Color& col);

void append_triangle(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& a, const Vector3& b, const Vector3& c, const Color& col);

void append_ring(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& center, const Vector3& normal, float radius, int segs, float tube_radius, const Color& col);

void append_box(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& center, float size, const Color& col);
bool ray_vs_sphere(const Vector3& ro, const Vector3& rd,
    const Vector3& sc, float sr, Vector3& out_hit, Vector3& out_normal);

// ============================================================================
// RAY MATH
// ============================================================================
bool ray_vs_segment(const Vector3& ro, const Vector3& rd,
    const Vector3& a, const Vector3& b, float radius, float& out_t);

bool ray_vs_plane(const Vector3& ro, const Vector3& rd,
    const Plane& p, Vector3& out_hit);

} // namespace vfx_editor

#endif
