#include "vfx_primitives.h"
#include <godot_cpp/core/class_db.hpp>
#include <cmath>
#include <map>
#include <vector>

using namespace godot;

// ============================================================================
// Helpers
// ============================================================================

static float _dict_f(const Dictionary& d, const char* key, float def) {
    if (d.has(key)) return (float)(double)d[key];
    return def;
}

static int _dict_i(const Dictionary& d, const char* key, int def) {
    if (d.has(key)) return (int)d[key];
    return def;
}

static bool _dict_b(const Dictionary& d, const char* key, bool def) {
    if (d.has(key)) return (bool)d[key];
    return def;
}

static int _clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float _clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Godot front faces are wound clockwise when seen from outside, i.e. the
// right-hand-rule cross product of (b-a)x(c-a) points INWARD. This helper
// orders each triangle so the winding always matches the outward normal,
// which makes every builder immune to axis-orientation mistakes.
static void _tri_out(VFXMesh* m, int a, int b, int c, const Vector3& outward) {
    Vector3 pa = m->get_vertex_position(a);
    Vector3 pb = m->get_vertex_position(b);
    Vector3 pc = m->get_vertex_position(c);
    Vector3 n = (pb - pa).cross(pc - pa);
    if (n.dot(outward) >= 0.0f) {
        m->add_triangle(a, c, b);
    } else {
        m->add_triangle(a, b, c);
    }
}

static void _quad_out(VFXMesh* m, int a, int b, int c, int d, const Vector3& outward) {
    _tri_out(m, a, b, c, outward);
    _tri_out(m, a, c, d, outward);
}

// ============================================================================
// Registration
// ============================================================================

void VFXPrimitives::_bind_methods() {
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create", "primitive_type", "params"), &VFXPrimitives::create);
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_named", "primitive_name", "params"), &VFXPrimitives::create_named);
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("get_primitive_name", "primitive_type"), &VFXPrimitives::get_primitive_name);
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("get_primitive_type_count"), &VFXPrimitives::get_primitive_type_count);
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_plane", "size", "subdivisions"), &VFXPrimitives::create_plane, DEFVAL(1.0f), DEFVAL(1));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_cube", "size", "subdivisions"), &VFXPrimitives::create_cube, DEFVAL(1.0f), DEFVAL(1));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_uv_sphere", "radius", "segments", "rings"), &VFXPrimitives::create_uv_sphere, DEFVAL(0.5f), DEFVAL(32), DEFVAL(16));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_ico_sphere", "radius", "subdivisions"), &VFXPrimitives::create_ico_sphere, DEFVAL(0.5f), DEFVAL(1));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_cylinder", "radius_top", "radius_bottom", "depth", "sides", "cap_top", "cap_bottom"), &VFXPrimitives::create_cylinder, DEFVAL(0.5f), DEFVAL(0.5f), DEFVAL(1.0f), DEFVAL(32), DEFVAL(true), DEFVAL(true));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_cone", "radius1", "radius2", "depth", "sides"), &VFXPrimitives::create_cone, DEFVAL(0.5f), DEFVAL(0.0f), DEFVAL(1.0f), DEFVAL(32));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_torus", "major_radius", "minor_radius", "major_segments", "minor_segments"), &VFXPrimitives::create_torus, DEFVAL(0.5f), DEFVAL(0.25f), DEFVAL(32), DEFVAL(12));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("create_circle", "radius", "sides"), &VFXPrimitives::create_circle, DEFVAL(0.5f), DEFVAL(32));
    ClassDB::bind_static_method("VFXPrimitives", D_METHOD("get_default_params", "primitive_type"), &VFXPrimitives::get_default_params);

    BIND_ENUM_CONSTANT(PRIM_PLANE);
    BIND_ENUM_CONSTANT(PRIM_CUBE);
    BIND_ENUM_CONSTANT(PRIM_UV_SPHERE);
    BIND_ENUM_CONSTANT(PRIM_ICO_SPHERE);
    BIND_ENUM_CONSTANT(PRIM_CYLINDER);
    BIND_ENUM_CONSTANT(PRIM_CONE);
    BIND_ENUM_CONSTANT(PRIM_TORUS);
    BIND_ENUM_CONSTANT(PRIM_CIRCLE);
}

VFXPrimitives::VFXPrimitives() {}
VFXPrimitives::~VFXPrimitives() {}

String VFXPrimitives::get_primitive_name(int primitive_type) {
    switch (primitive_type) {
        case PRIM_PLANE:     return "Plane";
        case PRIM_CUBE:      return "Cube";
        case PRIM_UV_SPHERE: return "Sphere";
        case PRIM_ICO_SPHERE:return "IcoSphere";
        case PRIM_CYLINDER:  return "Cylinder";
        case PRIM_CONE:      return "Cone";
        case PRIM_TORUS:     return "Torus";
        case PRIM_CIRCLE:    return "Circle";
        default:             return "Primitive";
    }
}

int VFXPrimitives::get_primitive_type_count() { return 8; }

Dictionary VFXPrimitives::get_default_params(int primitive_type) {
    Dictionary p;
    switch (primitive_type) {
        case PRIM_PLANE:
            p["size"] = 1.0; p["subdivisions"] = 1;
            break;
        case PRIM_CUBE:
            p["size"] = 1.0; p["subdivisions"] = 1;
            break;
        case PRIM_UV_SPHERE:
            p["radius"] = 0.5; p["segments"] = 32; p["rings"] = 16;
            break;
        case PRIM_ICO_SPHERE:
            p["radius"] = 0.5; p["subdivisions"] = 1;
            break;
        case PRIM_CYLINDER:
            p["radius_top"] = 0.5; p["radius_bottom"] = 0.5; p["depth"] = 1.0;
            p["sides"] = 32; p["cap_top"] = true; p["cap_bottom"] = true;
            break;
        case PRIM_CONE:
            p["radius1"] = 0.5; p["radius2"] = 0.0; p["depth"] = 1.0; p["sides"] = 32;
            break;
        case PRIM_TORUS:
            p["major_radius"] = 0.5; p["minor_radius"] = 0.25;
            p["major_segments"] = 32; p["minor_segments"] = 12;
            break;
        case PRIM_CIRCLE:
            p["radius"] = 0.5; p["sides"] = 32;
            break;
        default:
            break;
    }
    return p;
}

Ref<VFXMesh> VFXPrimitives::create(int primitive_type, const Dictionary& params) {
    switch (primitive_type) {
        case PRIM_PLANE:
            return create_plane(_dict_f(params, "size", 1.0f), _dict_i(params, "subdivisions", 1));
        case PRIM_CUBE:
            return create_cube(_dict_f(params, "size", 1.0f), _dict_i(params, "subdivisions", 1));
        case PRIM_UV_SPHERE:
            return create_uv_sphere(_dict_f(params, "radius", 0.5f),
                                    _dict_i(params, "segments", 32),
                                    _dict_i(params, "rings", 16));
        case PRIM_ICO_SPHERE:
            return create_ico_sphere(_dict_f(params, "radius", 0.5f),
                                     _dict_i(params, "subdivisions", 1));
        case PRIM_CYLINDER:
            return create_cylinder(_dict_f(params, "radius_top", 0.5f),
                                   _dict_f(params, "radius_bottom", 0.5f),
                                   _dict_f(params, "depth", 1.0f),
                                   _dict_i(params, "sides", _dict_i(params, "vertices", 32)),
                                   _dict_b(params, "cap_top", true),
                                   _dict_b(params, "cap_bottom", true));
        case PRIM_CONE:
            return create_cone(_dict_f(params, "radius1", 0.5f),
                               _dict_f(params, "radius2", 0.0f),
                               _dict_f(params, "depth", 1.0f),
                               _dict_i(params, "sides", _dict_i(params, "vertices", 32)));
        case PRIM_TORUS:
            return create_torus(_dict_f(params, "major_radius", 0.5f),
                                _dict_f(params, "minor_radius", 0.25f),
                                _dict_i(params, "major_segments", 32),
                                _dict_i(params, "minor_segments", 12));
        case PRIM_CIRCLE:
            return create_circle(_dict_f(params, "radius", 0.5f),
                                 _dict_i(params, "sides", 32));
        default:
            return Ref<VFXMesh>();
    }
}

Ref<VFXMesh> VFXPrimitives::create_named(const String& primitive_name, const Dictionary& params) {
    String n = primitive_name.to_lower();
    if (n == "plane")   return create(PRIM_PLANE, params);
    if (n == "cube" || n == "box") return create(PRIM_CUBE, params);
    if (n == "sphere" || n == "uv_sphere" || n == "uvsphere") return create(PRIM_UV_SPHERE, params);
    if (n == "ico_sphere" || n == "icosphere" || n == "ico")  return create(PRIM_ICO_SPHERE, params);
    if (n == "cylinder") return create(PRIM_CYLINDER, params);
    if (n == "cone")     return create(PRIM_CONE, params);
    if (n == "torus")    return create(PRIM_TORUS, params);
    if (n == "circle")   return create(PRIM_CIRCLE, params);
    return Ref<VFXMesh>();
}

// ============================================================================
// PLANE  (XZ, facing +Y — like Godot's PlaneMesh)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_plane(float size, int subdivisions) {
    size = MAX(size, 0.0001f);
    subdivisions = _clampi(subdivisions, 1, 512);

    Ref<VFXMesh> m;
    m.instantiate();

    const int n = subdivisions;
    const float h = size * 0.5f;
    const Vector3 outward(0, 1, 0);

    std::vector<int> grid((n + 1) * (n + 1));
    for (int j = 0; j <= n; j++) {
        for (int i = 0; i <= n; i++) {
            Vector3 pos(-h + size * (float)i / n, 0.0f, -h + size * (float)j / n);
            Vector2 uv((float)i / n, (float)j / n);
            grid[j * (n + 1) + i] = m->add_vertex(pos, uv);
            m->set_vertex_normal(grid[j * (n + 1) + i], outward);
        }
    }
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            int a = grid[j * (n + 1) + i];
            int b = grid[j * (n + 1) + i + 1];
            int c = grid[(j + 1) * (n + 1) + i + 1];
            int d = grid[(j + 1) * (n + 1) + i];
            _quad_out(m.ptr(), a, b, c, d, outward);
        }
    }
    m->link_twins();
    return m;
}

// ============================================================================
// CUBE  (Blender-style: per-face subdivisions, flat-shaded via duplicated
// face vertices, per-face 0..1 UVs)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_cube(float size, int subdivisions) {
    size = MAX(size, 0.0001f);
    subdivisions = _clampi(subdivisions, 1, 512);

    Ref<VFXMesh> m;
    m.instantiate();

    const int n = subdivisions;
    const float h = size * 0.5f;

    // Face frames: base corner + spanning axes (each length `size`).
    struct FaceFrame { Vector3 b0, a, b, n; };
    const FaceFrame frames[6] = {
        { Vector3(-h, -h,  h), Vector3( size, 0, 0), Vector3(0,  size, 0), Vector3( 0, 0,  1) }, // +Z
        { Vector3( h, -h, -h), Vector3(-size, 0, 0), Vector3(0,  size, 0), Vector3( 0, 0, -1) }, // -Z
        { Vector3( h, -h, -h), Vector3(0, 0,  size), Vector3(0,  size, 0), Vector3( 1, 0,  0) }, // +X
        { Vector3(-h, -h,  h), Vector3(0, 0, -size), Vector3(0,  size, 0), Vector3(-1, 0,  0) }, // -X
        { Vector3(-h,  h, -h), Vector3( size, 0, 0), Vector3(0, 0,  size), Vector3( 0, 1,  0) }, // +Y
        { Vector3(-h, -h,  h), Vector3( size, 0, 0), Vector3(0, 0, -size), Vector3( 0, -1, 0) }, // -Y
    };

    for (int f = 0; f < 6; f++) {
        const FaceFrame& fr = frames[f];
        std::vector<int> grid((n + 1) * (n + 1));
        for (int j = 0; j <= n; j++) {
            for (int i = 0; i <= n; i++) {
                Vector3 pos = fr.b0 + fr.a * ((float)i / n) + fr.b * ((float)j / n);
                Vector2 uv((float)i / n, (float)j / n);
                int id = m->add_vertex(pos, uv);
                m->set_vertex_normal(id, fr.n);
                grid[j * (n + 1) + i] = id;
            }
        }
        for (int j = 0; j < n; j++) {
            for (int i = 0; i < n; i++) {
                int a = grid[j * (n + 1) + i];
                int b = grid[j * (n + 1) + i + 1];
                int c = grid[(j + 1) * (n + 1) + i + 1];
                int d = grid[(j + 1) * (n + 1) + i];
                _quad_out(m.ptr(), a, b, c, d, fr.n);
            }
        }
    }
    m->link_twins();
    return m;
}

// ============================================================================
// UV SPHERE  (pole-to-pole rings; seam/pole vertices duplicated for clean UVs)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_uv_sphere(float radius, int segments, int rings) {
    radius = MAX(radius, 0.0001f);
    segments = _clampi(segments, 3, 512);
    rings = _clampi(rings, 2, 512);

    Ref<VFXMesh> m;
    m.instantiate();

    std::vector<int> grid((rings + 1) * (segments + 1));
    for (int j = 0; j <= rings; j++) {
        float phi = Math_PI * (float)j / rings; // 0 = top pole
        float y = radius * cosf(phi);
        float rr = radius * sinf(phi);
        for (int i = 0; i <= segments; i++) {
            float th = Math_TAU * (float)i / segments;
            Vector3 pos(rr * cosf(th), y, rr * sinf(th));
            Vector2 uv((float)i / segments, (float)j / rings);
            int id = m->add_vertex(pos, uv);
            Vector3 nrm = pos.normalized();
            if (nrm.length_squared() < 0.5f) nrm = (j == 0) ? Vector3(0, 1, 0) : Vector3(0, -1, 0);
            m->set_vertex_normal(id, nrm);
            grid[j * (segments + 1) + i] = id;
        }
    }
    for (int j = 0; j < rings; j++) {
        for (int i = 0; i < segments; i++) {
            int a = grid[j * (segments + 1) + i];
            int b = grid[j * (segments + 1) + i + 1];
            int c = grid[(j + 1) * (segments + 1) + i + 1];
            int d = grid[(j + 1) * (segments + 1) + i];
            Vector3 center = (m->get_vertex_position(a) + m->get_vertex_position(c)) * 0.5f;
            Vector3 outward = center.normalized();
            _quad_out(m.ptr(), a, b, c, d, outward);
        }
    }
    m->link_twins();
    return m;
}

// ============================================================================
// ICO SPHERE  (icosahedron subdivided `subdivisions` times)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_ico_sphere(float radius, int subdivisions) {
    radius = MAX(radius, 0.0001f);
    subdivisions = _clampi(subdivisions, 0, 5);

    Ref<VFXMesh> m;
    m.instantiate();

    const float t = (1.0f + sqrtf(5.0f)) * 0.5f;
    std::vector<Vector3> pts = {
        Vector3(-1,  t, 0), Vector3( 1,  t, 0), Vector3(-1, -t, 0), Vector3(1, -t, 0),
        Vector3( 0, -1, t), Vector3( 0,  1, t), Vector3( 0, -1,-t), Vector3(0,  1,-t),
        Vector3(  t, 0,-1), Vector3( t,  0, 1), Vector3(-t,  0,-1), Vector3(-t, 0, 1),
    };
    std::vector<Vector3> normals(pts.size());
    for (size_t i = 0; i < pts.size(); i++) normals[i] = pts[i].normalized();

    struct Tri { int a, b, c; };
    std::vector<Tri> tris = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1},
    };

    for (int sub = 0; sub < subdivisions; sub++) {
        std::map<std::pair<int, int>, int> mid_cache;
        std::vector<Vector3> new_pts;
        std::vector<Vector3> new_nrm;
        std::vector<Tri> new_tris;

        auto get_mid = [&](int a, int b) -> int {
            std::pair<int, int> key(a < b ? a : b, a < b ? b : a);
            auto it = mid_cache.find(key);
            if (it != mid_cache.end()) return it->second;
            Vector3 mid = (pts[a] + pts[b]) * 0.5f;
            new_pts.push_back(mid);
            new_nrm.push_back(mid.normalized());
            int id = (int)pts.size() + (int)new_pts.size() - 1;
            mid_cache[key] = id;
            return id;
        };

        for (const Tri& tr : tris) {
            int ab = get_mid(tr.a, tr.b);
            int bc = get_mid(tr.b, tr.c);
            int ca = get_mid(tr.c, tr.a);
            new_tris.push_back({tr.a, ab, ca});
            new_tris.push_back({tr.b, bc, ab});
            new_tris.push_back({tr.c, ca, bc});
            new_tris.push_back({ab, bc, ca});
        }

        pts.insert(pts.end(), new_pts.begin(), new_pts.end());
        normals.insert(normals.end(), new_nrm.begin(), new_nrm.end());
        tris = new_tris;
    }

    std::vector<int> ids(pts.size());
    for (size_t i = 0; i < pts.size(); i++) {
        Vector3 p = pts[i].normalized() * radius;
        // Cheap spherical UV projection (approximate; ico spheres have no
        // natural UV layout — same as Blender, which ships none by default).
        Vector2 uv(0.5f + atan2f(p.z, p.x) / Math_TAU, 0.5f - asinf(_clampf(p.y / radius, -1.0f, 1.0f)) / Math_PI);
        ids[i] = m->add_vertex(p, uv);
        m->set_vertex_normal(ids[i], normals[i]);
    }
    for (const Tri& tr : tris) {
        Vector3 outward = (pts[tr.a] + pts[tr.b] + pts[tr.c]).normalized();
        _tri_out(m.ptr(), ids[tr.a], ids[tr.b], ids[tr.c], outward);
    }
    m->link_twins();
    return m;
}

// ============================================================================
// CYLINDER / CONE  (frustum; caps are flat fans with their own ring vertices)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_cylinder(float radius_top, float radius_bottom,
                                            float depth, int sides,
                                            bool cap_top, bool cap_bottom) {
    radius_top = MAX(radius_top, 0.0f);
    radius_bottom = MAX(radius_bottom, 0.0f);
    depth = MAX(depth, 0.0001f);
    sides = _clampi(sides, 3, 512);
    if (radius_top <= 0.0f && radius_bottom <= 0.0f) radius_bottom = 0.5f;

    Ref<VFXMesh> m;
    m.instantiate();

    const float yb = -depth * 0.5f;
    const float yt =  depth * 0.5f;
    const float slope = (radius_bottom - radius_top) / depth; // for frustum normals

    // --- Side wall: two rings, seam-duplicated for clean UVs ---
    std::vector<int> ring_b(sides + 1), ring_t(sides + 1);
    for (int i = 0; i <= sides; i++) {
        float th = Math_TAU * (float)i / sides;
        float c = cosf(th), s = sinf(th);
        Vector2 uv((float)i / sides, 0.0f);
        ring_b[i] = m->add_vertex(Vector3(radius_bottom * c, yb, radius_bottom * s), uv);
        Vector3 nb(c, slope, s);
        m->set_vertex_normal(ring_b[i], nb.normalized());

        uv.y = 1.0f;
        ring_t[i] = m->add_vertex(Vector3(radius_top * c, yt, radius_top * s), uv);
        Vector3 nt(c, slope, s);
        m->set_vertex_normal(ring_t[i], nt.normalized());
    }
    for (int i = 0; i < sides; i++) {
        Vector3 outward = (m->get_vertex_normal(ring_b[i]) + m->get_vertex_normal(ring_t[i])) * 0.5f;
        _quad_out(m.ptr(), ring_b[i], ring_b[i + 1], ring_t[i + 1], ring_t[i], outward);
    }

    // --- Caps: separate center + ring (flat shading, own UVs) ---
    auto add_cap = [&](float radius, float y, bool top) {
        if (radius <= 0.0f) return;
        Vector3 n(0, top ? 1.0f : -1.0f, 0);
        int center = m->add_vertex(Vector3(0, y, 0), Vector2(0.5f, 0.5f));
        m->set_vertex_normal(center, n);
        std::vector<int> ring(sides + 1);
        for (int i = 0; i <= sides; i++) {
            float th = Math_TAU * (float)i / sides;
            Vector3 p(radius * cosf(th), y, radius * sinf(th));
            Vector2 uv(0.5f + 0.5f * cosf(th), 0.5f + 0.5f * sinf(th));
            ring[i] = m->add_vertex(p, uv);
            m->set_vertex_normal(ring[i], n);
        }
        for (int i = 0; i < sides; i++) {
            if (top) _tri_out(m.ptr(), center, ring[i], ring[i + 1], n);
            else     _tri_out(m.ptr(), center, ring[i], ring[i + 1], n);
        }
    };
    if (cap_bottom && radius_bottom > 0.0f) add_cap(radius_bottom, yb, false);
    if (cap_top && radius_top > 0.0f)       add_cap(radius_top, yt, true);

    m->link_twins();
    return m;
}

Ref<VFXMesh> VFXPrimitives::create_cone(float radius1, float radius2, float depth, int sides) {
    // Blender cone: radius1 = base, radius2 = top (0 = sharp tip).
    return create_cylinder(radius2, radius1, depth, sides, radius2 > 0.0f, radius1 > 0.0f);
}

// ============================================================================
// TORUS  (hole axis = +Y, lying in XZ like a flat donut)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_torus(float major_radius, float minor_radius,
                                         int major_segments, int minor_segments) {
    major_radius = MAX(major_radius, 0.0001f);
    minor_radius = MAX(minor_radius, 0.0001f);
    major_segments = _clampi(major_segments, 3, 512);
    minor_segments = _clampi(minor_segments, 3, 512);

    Ref<VFXMesh> m;
    m.instantiate();

    std::vector<int> grid((major_segments + 1) * (minor_segments + 1));
    for (int i = 0; i <= major_segments; i++) {
        float th = Math_TAU * (float)i / major_segments;
        float ct = cosf(th), st = sinf(th);
        for (int j = 0; j <= minor_segments; j++) {
            float ph = Math_TAU * (float)j / minor_segments;
            float cp = cosf(ph), sp = sinf(ph);
            float R = major_radius + minor_radius * cp;
            Vector3 pos(R * ct, minor_radius * sp, R * st);
            Vector2 uv((float)i / major_segments, (float)j / minor_segments);
            int id = m->add_vertex(pos, uv);
            m->set_vertex_normal(id, Vector3(cp * ct, sp, cp * st));
            grid[i * (minor_segments + 1) + j] = id;
        }
    }
    for (int i = 0; i < major_segments; i++) {
        for (int j = 0; j < minor_segments; j++) {
            int a = grid[i * (minor_segments + 1) + j];
            int b = grid[(i + 1) * (minor_segments + 1) + j];
            int c = grid[(i + 1) * (minor_segments + 1) + j + 1];
            int d = grid[i * (minor_segments + 1) + j + 1];
            Vector3 outward = (m->get_vertex_normal(a) + m->get_vertex_normal(c)) * 0.5f;
            _quad_out(m.ptr(), a, b, c, d, outward);
        }
    }
    m->link_twins();
    return m;
}

// ============================================================================
// CIRCLE  (filled ngon fan in XZ, facing +Y)
// ============================================================================
Ref<VFXMesh> VFXPrimitives::create_circle(float radius, int sides) {
    radius = MAX(radius, 0.0001f);
    sides = _clampi(sides, 3, 512);

    Ref<VFXMesh> m;
    m.instantiate();

    const Vector3 n(0, 1, 0);
    int center = m->add_vertex(Vector3(0, 0, 0), Vector2(0.5f, 0.5f));
    m->set_vertex_normal(center, n);

    std::vector<int> ring(sides + 1);
    for (int i = 0; i <= sides; i++) {
        float th = Math_TAU * (float)i / sides;
        Vector3 p(radius * cosf(th), 0.0f, radius * sinf(th));
        Vector2 uv(0.5f + 0.5f * cosf(th), 0.5f + 0.5f * sinf(th));
        ring[i] = m->add_vertex(p, uv);
        m->set_vertex_normal(ring[i], n);
    }
    for (int i = 0; i < sides; i++) {
        _tri_out(m.ptr(), center, ring[i], ring[i + 1], n);
    }
    m->link_twins();
    return m;
}
