#include "vfx_editor_utils.h"
#include "vfx_math.h"
#include <godot_cpp/variant/vector3.hpp>
#include <cmath>

using namespace godot;

namespace vfx_editor {

void append_cylinder(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& from, const Vector3& to, float radius, int segs, const Color& col) {
    Vector3 dir = to - from;
    float len = dir.length();
    if (len < 0.0001f) return;
    dir /= len;

    Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = dir.cross(up).normalized();
    up = right.cross(dir).normalized();

    int base = verts.size();
    for (int i = 0; i <= segs; i++) {
        float ang = (float)i / (float)segs * 3.14159265f * 2.0f;
        Vector3 off = right * cosf(ang) * radius + up * sinf(ang) * radius;
        verts.push_back(from + off);
        cols.push_back(col);
        verts.push_back(to + off);
        cols.push_back(col);
    }
    for (int i = 0; i < segs; i++) {
        int b = base + i * 2;
        idx.push_back(b); idx.push_back(b + 2); idx.push_back(b + 1);
        idx.push_back(b + 1); idx.push_back(b + 2); idx.push_back(b + 3);
    }
}

void append_triangle(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& a, const Vector3& b, const Vector3& c, const Color& col) {
    int base = verts.size();
    verts.push_back(a); verts.push_back(b); verts.push_back(c);
    cols.push_back(col); cols.push_back(col); cols.push_back(col);
    idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
}

void append_ring(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& center, const Vector3& normal, float radius, int segs, float tube_radius, const Color& col) {
    Vector3 up = fabs(normal.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = normal.cross(up).normalized();
    up = right.cross(normal).normalized();

    Vector3 prev_pos;
    bool has_prev = false;

    for (int i = 0; i <= segs; i++) {
        float ang = (float)i / (float)segs * 3.14159265f * 2.0f;
        Vector3 pos = center + right * cosf(ang) * radius + up * sinf(ang) * radius;
        if (has_prev) {
            append_cylinder(verts, cols, idx, prev_pos, pos, tube_radius, 4, col);
        }
        prev_pos = pos;
        has_prev = true;
    }
}

void append_box(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& center, float size, const Color& col) {
    float h = size * 0.5f;
    int base = verts.size();

    verts.push_back(center + Vector3(-h, -h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3( h, -h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3( h,  h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h,  h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h, -h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3( h, -h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3( h,  h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h,  h,  h)); cols.push_back(col);

    const int faces[12][3] = {
        {0,1,2}, {0,2,3}, {4,6,5}, {4,7,6},
        {0,5,1}, {0,4,5}, {2,7,3}, {2,6,7},
        {0,3,7}, {0,7,4}, {1,6,2}, {1,5,6}
    };
    for (int i = 0; i < 12; i++) {
        idx.push_back(base + faces[i][0]);
        idx.push_back(base + faces[i][1]);
        idx.push_back(base + faces[i][2]);
    }
}

bool ray_vs_segment(const Vector3& ro, const Vector3& rd,
    const Vector3& a, const Vector3& b, float radius, float& out_t) {
    Vector3 u = rd.normalized();
    Vector3 v = b - a;
    Vector3 w0 = ro - a;

    float uv = u.dot(v);
    float vv = v.length_squared();
    float uw0 = u.dot(w0);
    float vw0 = v.dot(w0);

    if (vv < 0.0001f) {
        Vector3 oc = ro - a;
        float b_ = u.dot(oc);
        float c = oc.dot(oc) - radius * radius;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) return false;
        float t = -b_ - sqrt(disc);
        if (t < 0.0f) t = -b_ + sqrt(disc);
        if (t < 0.0f) return false;
        out_t = t;
        return true;
    }

    float det = uv * uv - vv;
    float t, s;

    if (fabs(det) < 0.0001f) {
        s = vfx::clampf(vw0 / vv, 0.0f, 1.0f);
        Vector3 closest_seg = a + v * s;
        t = u.dot(closest_seg - ro);
    } else {
        t = (uw0 * vv - uv * vw0) / det;
        s = (uv * uw0 - vw0) / det;
    }

    if (s >= 0.0f && s <= 1.0f && t >= 0.0f) {
        Vector3 closest_seg = a + v * s;
        Vector3 closest_ray = ro + u * t;
        if ((closest_seg - closest_ray).length_squared() < radius * radius) {
            out_t = t;
            return true;
        }
    }

    if (s >= 0.0f && s <= 1.0f && t < 0.0f) {
        Vector3 closest_seg = a + v * s;
        if ((closest_seg - ro).length_squared() < radius * radius) {
            out_t = 0.0f;
            return true;
        }
    }

    auto sphere_test = [&](const Vector3& center) -> bool {
        Vector3 oc = ro - center;
        float b_ = u.dot(oc);
        float c = oc.dot(oc) - radius * radius;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) return false;
        float tt = -b_ - sqrt(disc);
        if (tt < 0.0f) tt = -b_ + sqrt(disc);
        if (tt < 0.0f) return false;
        out_t = tt;
        return true;
    };

    if (s < 0.0f) {
        if (sphere_test(a)) return true;
    }
    if (s > 1.0f) {
        if (sphere_test(b)) return true;
    }

    return false;
}

bool ray_vs_plane(const Vector3& ro, const Vector3& rd,
    const Plane& p, Vector3& out_hit) {
    float denom = p.normal.dot(rd);
    if (fabs(denom) < 0.0001f) return false;
    float t = -(p.normal.dot(ro) + p.d) / denom;
    if (t < 0.0f) return false;
    out_hit = ro + rd * t;
    return true;
}

} // namespace vfx_editor
