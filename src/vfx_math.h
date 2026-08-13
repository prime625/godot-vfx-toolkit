#ifndef VFX_MATH_H
#define VFX_MATH_H

#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/color.hpp>
#include <cmath>

using namespace godot;

namespace vfx {

inline float clampf(float v, float mn, float mx) {
    return v < mn ? mn : (v > mx ? mx : v);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Vector3 lerp_vec3(const Vector3& a, const Vector3& b, float t) {
    return a.lerp(b, t);
}

inline Quaternion slerp_quat(const Quaternion& a, const Quaternion& b, float t) {
    return a.slerp(b, t);
}

inline Vector3 project_on_plane(const Vector3& v, const Vector3& normal) {
    return v - normal * v.dot(normal);
}

inline Basis look_at_safe(const Vector3& direction, const Vector3& up = Vector3(0, 1, 0)) {
    Vector3 z = direction.normalized();
    Vector3 x = up.cross(z).normalized();
    if (x.length_squared() < 0.0001f) {
        x = Vector3(1, 0, 0).cross(z).normalized();
        if (x.length_squared() < 0.0001f) {
            x = Vector3(0, 0, 1).cross(z).normalized();
        }
    }
    Vector3 y = z.cross(x);
    return Basis(x, y, z);
}

// 4x4 matrix for skinning (stored as Transform3D + scale in Godot)
struct SkinMatrix {
    Transform3D transform;

    Vector3 transform_point(const Vector3& p) const {
        return transform.xform(p);
    }

    Vector3 transform_vector(const Vector3& v) const {
        Basis b = transform.get_basis();
        return b.xform(v);
    }
};

// AABB for spatial queries
struct AABB {
    Vector3 min, max;

    AABB() : min(Vector3(1e30f, 1e30f, 1e30f)), max(Vector3(-1e30f, -1e30f, -1e30f)) {}

    void expand(const Vector3& p) {
        min.x = fmin(min.x, p.x); min.y = fmin(min.y, p.y); min.z = fmin(min.z, p.z);
        max.x = fmax(max.x, p.x); max.y = fmax(max.y, p.y); max.z = fmax(max.z, p.z);
    }

    bool contains(const Vector3& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
    }

    Vector3 center() const { return (min + max) * 0.5f; }
    float radius() const { return (max - min).length() * 0.5f; }
};

} // namespace vfx

#endif
