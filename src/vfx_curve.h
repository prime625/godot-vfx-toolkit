#ifndef VFX_CURVE_H
#define VFX_CURVE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <vector>
#include "vfx_mesh.h"

using namespace godot;

// Blender-style 3D Bezier curve with extrude/bevel to mesh
class VFXCurve : public RefCounted {
    GDCLASS(VFXCurve, RefCounted)

public:
    struct Point {
        Vector3 position;
        Vector3 handle_in;
        Vector3 handle_out;
        float radius = 1.0f; // Per-point multiplier; keeps the base radius global.
        float tilt = 0.0f;   // Per-point twist in radians.
    };

private:
    std::vector<Point> points;
    bool closed = false;

    int _get_span_count() const;
    int _get_next_point(int idx) const;

    static Vector3 _eval_bezier(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
    static Vector3 _eval_tangent(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

protected:
    static void _bind_methods();

public:
    VFXCurve();
    ~VFXCurve();

    void clear();
    int add_point(const Vector3& pos, const Vector3& handle_in = Vector3(), const Vector3& handle_out = Vector3());
    void set_point(int idx, const Vector3& pos);
    void set_handle_in(int idx, const Vector3& h);
    void set_handle_out(int idx, const Vector3& h);
    Vector3 get_point(int idx) const;
    Vector3 get_handle_in(int idx) const;
    Vector3 get_handle_out(int idx) const;
    int get_point_count() const;

    void set_closed(bool p_closed);
    bool is_closed() const;
    void set_point_radius(int idx, float radius);
    float get_point_radius(int idx) const;
    void set_point_tilt(int idx, float tilt);
    float get_point_tilt(int idx) const;

    Vector3 evaluate(float t) const; // t in [0,1] across entire curve
    Vector3 evaluate_tangent(float t) const;
    PackedVector3Array tessellate(int segments_per_span = 16) const;

    // Mesh generation
    Ref<VFXMesh> to_tube_mesh(float radius = 0.1f, int segments = 16, int rings = 8, bool cap_start = true, bool cap_end = true) const;
    Ref<VFXMesh> to_ribbon_mesh(float width = 0.2f, int segments = 16) const;
    // Mobile-safe builder: hard limits prevent accidental multi-million vertex meshes.
    Ref<VFXMesh> to_tube_mesh_advanced(float radius = 0.1f, int segments_per_span = 8,
            int radial_segments = 8, bool cap_start = true, bool cap_end = true,
            int max_vertices = 65536) const;

    void from_catmull_rom(const PackedVector3Array& path, float tension = 0.5f);
};

#endif
