#include "vfx_curve.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

void VFXCurve::_bind_methods() {
    ClassDB::bind_method(D_METHOD("clear"), &VFXCurve::clear);
    ClassDB::bind_method(D_METHOD("add_point", "pos", "handle_in", "handle_out"), &VFXCurve::add_point, DEFVAL(Vector3()), DEFVAL(Vector3()));
    ClassDB::bind_method(D_METHOD("set_point", "idx", "pos"), &VFXCurve::set_point);
    ClassDB::bind_method(D_METHOD("set_handle_in", "idx", "h"), &VFXCurve::set_handle_in);
    ClassDB::bind_method(D_METHOD("set_handle_out", "idx", "h"), &VFXCurve::set_handle_out);
    ClassDB::bind_method(D_METHOD("get_point", "idx"), &VFXCurve::get_point);
    ClassDB::bind_method(D_METHOD("get_handle_in", "idx"), &VFXCurve::get_handle_in);
    ClassDB::bind_method(D_METHOD("get_handle_out", "idx"), &VFXCurve::get_handle_out);
    ClassDB::bind_method(D_METHOD("get_point_count"), &VFXCurve::get_point_count);
    ClassDB::bind_method(D_METHOD("set_closed", "closed"), &VFXCurve::set_closed);
    ClassDB::bind_method(D_METHOD("is_closed"), &VFXCurve::is_closed);
    ClassDB::bind_method(D_METHOD("set_point_radius", "idx", "radius"), &VFXCurve::set_point_radius);
    ClassDB::bind_method(D_METHOD("get_point_radius", "idx"), &VFXCurve::get_point_radius);
    ClassDB::bind_method(D_METHOD("set_point_tilt", "idx", "tilt"), &VFXCurve::set_point_tilt);
    ClassDB::bind_method(D_METHOD("get_point_tilt", "idx"), &VFXCurve::get_point_tilt);

    ClassDB::bind_method(D_METHOD("evaluate", "t"), &VFXCurve::evaluate);
    ClassDB::bind_method(D_METHOD("evaluate_tangent", "t"), &VFXCurve::evaluate_tangent);
    ClassDB::bind_method(D_METHOD("tessellate", "segments_per_span"), &VFXCurve::tessellate, DEFVAL(16));

    ClassDB::bind_method(D_METHOD("to_tube_mesh", "radius", "segments", "rings", "cap_start", "cap_end"), &VFXCurve::to_tube_mesh, DEFVAL(0.1f), DEFVAL(16), DEFVAL(8), DEFVAL(true), DEFVAL(true));
    ClassDB::bind_method(D_METHOD("to_ribbon_mesh", "width", "segments"), &VFXCurve::to_ribbon_mesh, DEFVAL(0.2f), DEFVAL(16));
    ClassDB::bind_method(D_METHOD("to_tube_mesh_advanced", "radius", "segments_per_span", "radial_segments", "cap_start", "cap_end", "max_vertices"), &VFXCurve::to_tube_mesh_advanced, DEFVAL(0.1f), DEFVAL(8), DEFVAL(8), DEFVAL(true), DEFVAL(true), DEFVAL(65536));
}

VFXCurve::VFXCurve() {}
VFXCurve::~VFXCurve() {}

void VFXCurve::clear() { points.clear(); closed = false; }

int VFXCurve::add_point(const Vector3& pos, const Vector3& handle_in, const Vector3& handle_out) {
    // A hard control-point ceiling protects touch workflows from unbounded allocations.
    if (points.size() >= 512) return -1;
    Point p;
    p.position = pos;
    p.handle_in = handle_in.is_zero_approx() ? (pos - Vector3(1,0,0)) : handle_in;
    p.handle_out = handle_out.is_zero_approx() ? (pos + Vector3(1,0,0)) : handle_out;
    points.push_back(p);
    return points.size() - 1;
}

void VFXCurve::set_point(int idx, const Vector3& pos) {
    if (idx >= 0 && idx < (int)points.size()) {
        // Handles are stored in curve-local coordinates; translating a knot should
        // preserve its tangent shape instead of leaving handles behind in space.
        Vector3 delta = pos - points[idx].position;
        points[idx].position = pos;
        points[idx].handle_in += delta;
        points[idx].handle_out += delta;
    }
}
void VFXCurve::set_handle_in(int idx, const Vector3& h) {
    if (idx >= 0 && idx < (int)points.size()) points[idx].handle_in = h;
}
void VFXCurve::set_handle_out(int idx, const Vector3& h) {
    if (idx >= 0 && idx < (int)points.size()) points[idx].handle_out = h;
}
Vector3 VFXCurve::get_point(int idx) const {
    if (idx >= 0 && idx < (int)points.size()) return points[idx].position;
    return Vector3();
}
Vector3 VFXCurve::get_handle_in(int idx) const {
    if (idx >= 0 && idx < (int)points.size()) return points[idx].handle_in;
    return Vector3();
}
Vector3 VFXCurve::get_handle_out(int idx) const {
    if (idx >= 0 && idx < (int)points.size()) return points[idx].handle_out;
    return Vector3();
}
int VFXCurve::get_point_count() const { return (int)points.size(); }

int VFXCurve::_get_span_count() const {
    if (points.size() < 2) return 0;
    return closed ? (int)points.size() : (int)points.size() - 1;
}

int VFXCurve::_get_next_point(int idx) const {
    return (idx + 1) % (int)points.size();
}

void VFXCurve::set_closed(bool p_closed) { closed = p_closed; }
bool VFXCurve::is_closed() const { return closed; }
void VFXCurve::set_point_radius(int idx, float radius) {
    if (idx >= 0 && idx < (int)points.size()) points[idx].radius = vfx::clampf(radius, 0.0f, 1000.0f);
}
float VFXCurve::get_point_radius(int idx) const {
    return (idx >= 0 && idx < (int)points.size()) ? points[idx].radius : 1.0f;
}
void VFXCurve::set_point_tilt(int idx, float tilt) {
    if (idx >= 0 && idx < (int)points.size()) points[idx].tilt = vfx::clampf(tilt, -1000.0f, 1000.0f);
}
float VFXCurve::get_point_tilt(int idx) const {
    return (idx >= 0 && idx < (int)points.size()) ? points[idx].tilt : 0.0f;
}

Vector3 VFXCurve::_eval_bezier(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    float omt = 1.0f - t;
    float omt2 = omt * omt;
    float omt3 = omt2 * omt;
    float t2 = t * t;
    float t3 = t2 * t;
    return p0 * omt3 + p1 * (3.0f * omt2 * t) + p2 * (3.0f * omt * t2) + p3 * t3;
}

Vector3 VFXCurve::_eval_tangent(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    float omt = 1.0f - t;
    return (p1 - p0) * (3.0f * omt * omt) + (p2 - p1) * (6.0f * omt * t) + (p3 - p2) * (3.0f * t * t);
}

Vector3 VFXCurve::evaluate(float t) const {
    if (points.empty()) return Vector3();
    if (points.size() == 1) return points[0].position;
    const int spans = _get_span_count();
    t = vfx::clampf(t, 0.0f, 1.0f);
    float span_f = t * spans;
    int span = MIN((int)floor(span_f), spans - 1);
    float local_t = (t >= 1.0f) ? 1.0f : span_f - span;
    int next = _get_next_point(span);
    return _eval_bezier(points[span].position, points[span].handle_out,
            points[next].handle_in, points[next].position, local_t);
}

Vector3 VFXCurve::evaluate_tangent(float t) const {
    if (points.size() < 2) return Vector3(0, 0, 1);
    const int spans = _get_span_count();
    t = vfx::clampf(t, 0.0f, 1.0f);
    float span_f = t * spans;
    int span = MIN((int)floor(span_f), spans - 1);
    float local_t = (t >= 1.0f) ? 1.0f : span_f - span;
    int next = _get_next_point(span);
    Vector3 tangent = _eval_tangent(points[span].position, points[span].handle_out,
            points[next].handle_in, points[next].position, local_t);
    if (tangent.length_squared() < 1e-10f)
        tangent = points[next].position - points[span].position;
    return tangent.normalized();
}

PackedVector3Array VFXCurve::tessellate(int segments_per_span) const {
    PackedVector3Array arr;
    const int spans = _get_span_count();
    if (spans == 0) return arr;
    // Bound allocation for mobile devices. This is a preview/path API, not a render-quality setting.
    segments_per_span = CLAMP(segments_per_span, 1, 128);
    const int sample_count = spans * segments_per_span + (closed ? 0 : 1);
    arr.resize(sample_count);
    int out = 0;
    for (int i = 0; i < spans; i++) {
        int next = _get_next_point(i);
        for (int s = 0; s < segments_per_span; s++) {
            float t = (float)s / (float)segments_per_span;
            arr.set(out++, _eval_bezier(points[i].position, points[i].handle_out,
                    points[next].handle_in, points[next].position, t));
        }
    }
    if (!closed) arr.set(out, points.back().position);
    return arr;
}

Ref<VFXMesh> VFXCurve::to_tube_mesh(float radius, int segments, int rings, bool cap_start, bool cap_end) const {
    return to_tube_mesh_advanced(radius, segments, rings, cap_start, cap_end, 65536);
}

Ref<VFXMesh> VFXCurve::to_tube_mesh_advanced(float radius, int segments_per_span,
        int radial_segments, bool cap_start, bool cap_end, int max_vertices) const {
    Ref<VFXMesh> mesh;
    mesh.instantiate();
    const int spans = _get_span_count();
    if (spans == 0 || radius <= 0.0f) return mesh;

    // Conservative mobile limits. Avoid integer overflow and runaway allocations.
    segments_per_span = CLAMP(segments_per_span, 1, 128);
    radial_segments = CLAMP(radial_segments, 3, 32);
    max_vertices = CLAMP(max_vertices, 64, 262144);
    int samples_per_span = segments_per_span;
    int sample_count = spans * samples_per_span + (closed ? 0 : 1);
    int cap_vertices = closed ? 0 : ((cap_start ? 1 : 0) + (cap_end ? 1 : 0));
    while (sample_count * radial_segments + cap_vertices > max_vertices && samples_per_span > 1) {
        samples_per_span--;
        sample_count = spans * samples_per_span + (closed ? 0 : 1);
    }
    if (sample_count * radial_segments + cap_vertices > max_vertices) return mesh;

    std::vector<Vector3> centers; centers.reserve(sample_count);
    std::vector<Vector3> tangents; tangents.reserve(sample_count);
    std::vector<float> radii; radii.reserve(sample_count);
    std::vector<float> tilts; tilts.reserve(sample_count);
    for (int i = 0; i < spans; ++i) {
        const int next = _get_next_point(i);
        for (int j = 0; j < samples_per_span; ++j) {
            const float t = (float)j / (float)samples_per_span;
            centers.push_back(_eval_bezier(points[i].position, points[i].handle_out,
                    points[next].handle_in, points[next].position, t));
            Vector3 tangent = _eval_tangent(points[i].position, points[i].handle_out,
                    points[next].handle_in, points[next].position, t);
            if (tangent.length_squared() < 1e-10f) tangent = points[next].position - points[i].position;
            tangents.push_back(tangent.normalized());
            radii.push_back(radius * (points[i].radius + (points[next].radius - points[i].radius) * t));
            tilts.push_back((points[i].tilt + (points[next].tilt - points[i].tilt) * t));
        }
    }
    if (!closed) {
        centers.push_back(points.back().position);
        Vector3 tangent = points.back().position - points[points.size() - 2].position;
        if (tangent.length_squared() < 1e-10f) tangent = Vector3(0, 0, 1);
        tangents.push_back(tangent.normalized());
        radii.push_back(radius * points.back().radius);
        tilts.push_back(points.back().tilt);
    }

    // Parallel-transport frames: avoids sudden twisting and is cheap (linear in samples).
    std::vector<Vector3> frame_x; frame_x.reserve(sample_count);
    std::vector<Vector3> frame_y; frame_y.reserve(sample_count);
    Vector3 ref = fabs(tangents[0].dot(Vector3(0, 1, 0))) < 0.95f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 x = ref.cross(tangents[0]).normalized();
    if (x.length_squared() < 1e-10f) x = Vector3(1, 0, 0);
    for (int i = 0; i < sample_count; ++i) {
        if (i > 0) {
            Vector3 prev_t = tangents[i - 1];
            Vector3 curr_t = tangents[i];
            Vector3 axis = prev_t.cross(curr_t);
            float axis_len = axis.length();
            if (axis_len > 1e-6f) {
                float angle = atan2f(axis_len, CLAMP(prev_t.dot(curr_t), -1.0f, 1.0f));
                x = x.rotated(axis / axis_len, angle);
            } else if (prev_t.dot(curr_t) < -0.999f) {
                x = x.rotated(frame_y.back(), Math_PI);
            }
        }
        x = (x - tangents[i] * x.dot(tangents[i])).normalized();
        if (x.length_squared() < 1e-10f) x = ref.cross(tangents[i]).normalized();
        Vector3 y = tangents[i].cross(x).normalized();
        if (fabsf(tilts[i]) > 1e-6f) {
            x = x.rotated(tangents[i], tilts[i]);
            y = tangents[i].cross(x).normalized();
        }
        frame_x.push_back(x);
        frame_y.push_back(y);
    }

    std::vector<int> ring_verts((size_t)sample_count * radial_segments);
    for (int i = 0; i < sample_count; ++i) {
        const float u = sample_count > 1 ? (float)i / (float)(sample_count - 1) : 0.0f;
        for (int r = 0; r < radial_segments; ++r) {
            const float a = (float)r / (float)radial_segments * Math_TAU;
            Vector3 offset = (frame_x[i] * cosf(a) + frame_y[i] * sinf(a)) * radii[i];
            ring_verts[(size_t)i * radial_segments + r] = mesh->add_vertex(
                    centers[i] + offset, Vector2(u, (float)r / (float)radial_segments));
        }
    }
    const int ring_count = closed ? sample_count : sample_count - 1;
    for (int i = 0; i < ring_count; ++i) {
        const int next_i = (i + 1) % sample_count;
        for (int r = 0; r < radial_segments; ++r) {
            const int rn = (r + 1) % radial_segments;
            const int a = ring_verts[(size_t)i * radial_segments + r];
            const int b = ring_verts[(size_t)i * radial_segments + rn];
            const int c = ring_verts[(size_t)next_i * radial_segments + rn];
            const int d = ring_verts[(size_t)next_i * radial_segments + r];
            mesh->add_quad(a, b, c, d);
        }
    }
    if (!closed && cap_start) {
        int center = mesh->add_vertex(centers.front(), Vector2(0.5f, 0.5f));
        for (int r = 0; r < radial_segments; ++r)
            mesh->add_triangle(center, ring_verts[r], ring_verts[(r + 1) % radial_segments]);
    }
    if (!closed && cap_end) {
        int center = mesh->add_vertex(centers.back(), Vector2(0.5f, 0.5f));
        int base = (sample_count - 1) * radial_segments;
        for (int r = 0; r < radial_segments; ++r)
            mesh->add_triangle(center, ring_verts[base + (r + 1) % radial_segments], ring_verts[base + r]);
    }
    mesh->recalculate_normals();
    mesh->link_twins();
    return mesh;
}

Ref<VFXMesh> VFXCurve::to_ribbon_mesh(float width, int segments) const {
    Ref<VFXMesh> mesh;
    mesh.instantiate();
    const int spans = _get_span_count();
    if (spans == 0 || width <= 0.0f) return mesh;
    segments = CLAMP(segments, 1, 256);
    // A ribbon uses two vertices per sample. Reduce tessellation before refusing
    // generation so long curves still produce a useful mobile-quality result.
    segments = MIN(segments, MAX(1, 32768 / spans));
    const int sample_count = spans * segments + (closed ? 0 : 1);

    std::vector<Vector3> centers; centers.reserve(sample_count);
    std::vector<Vector3> tangents; tangents.reserve(sample_count);
    std::vector<float> widths; widths.reserve(sample_count);
    std::vector<float> tilts; tilts.reserve(sample_count);
    for (int i = 0; i < spans; ++i) {
        const int next = _get_next_point(i);
        for (int j = 0; j < segments; ++j) {
            const float t = (float)j / (float)segments;
            centers.push_back(_eval_bezier(points[i].position, points[i].handle_out,
                    points[next].handle_in, points[next].position, t));
            Vector3 tangent = _eval_tangent(points[i].position, points[i].handle_out,
                    points[next].handle_in, points[next].position, t);
            if (tangent.length_squared() < 1e-10f) tangent = points[next].position - points[i].position;
            tangents.push_back(tangent.normalized());
            widths.push_back(width * (points[i].radius + (points[next].radius - points[i].radius) * t));
            tilts.push_back(points[i].tilt + (points[next].tilt - points[i].tilt) * t);
        }
    }
    if (!closed) {
        centers.push_back(points.back().position);
        Vector3 tangent = points.back().position - points[points.size() - 2].position;
        if (tangent.length_squared() < 1e-10f) tangent = Vector3(0, 0, 1);
        tangents.push_back(tangent.normalized());
        widths.push_back(width * points.back().radius);
        tilts.push_back(points.back().tilt);
    }

    Vector3 ref = fabs(tangents[0].dot(Vector3(0, 1, 0))) < 0.95f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 side = ref.cross(tangents[0]).normalized();
    if (side.length_squared() < 1e-10f) side = Vector3(1, 0, 0);
    std::vector<int> left(sample_count), right(sample_count);
    for (int i = 0; i < sample_count; ++i) {
        if (i > 0) {
            Vector3 axis = tangents[i - 1].cross(tangents[i]);
            float axis_len = axis.length();
            if (axis_len > 1e-6f) {
                float angle = atan2f(axis_len, CLAMP(tangents[i - 1].dot(tangents[i]), -1.0f, 1.0f));
                side = side.rotated(axis / axis_len, angle);
            }
        }
        side = (side - tangents[i] * side.dot(tangents[i])).normalized();
        if (side.length_squared() < 1e-10f) side = ref.cross(tangents[i]).normalized();
        if (fabsf(tilts[i]) > 1e-6f)
            side = side.rotated(tangents[i], tilts[i]);
        const float half_width = widths[i] * 0.5f;
        float u = sample_count > 1 ? (float)i / (float)(sample_count - 1) : 0.0f;
        left[i] = mesh->add_vertex(centers[i] - side * half_width, Vector2(0.0f, u));
        right[i] = mesh->add_vertex(centers[i] + side * half_width, Vector2(1.0f, u));
    }
    const int segment_count = closed ? sample_count : sample_count - 1;
    for (int i = 0; i < segment_count; ++i) {
        const int n = (i + 1) % sample_count;
        mesh->add_quad(left[i], right[i], right[n], left[n]);
    }
    mesh->recalculate_normals();
    mesh->link_twins();
    return mesh;
}

void VFXCurve::from_catmull_rom(const PackedVector3Array& path, float tension) {
    clear();
    closed = false;
    if (path.size() < 2) return;
    for (int i = 0; i < path.size(); i++) {
        Vector3 p = path[i];
        Vector3 in_vec, out_vec;
        if (i == 0) {
            out_vec = (path[1] - path[0]) * tension;
        } else if (i == path.size() - 1) {
            in_vec = (path[i] - path[i-1]) * tension;
        } else {
            in_vec = (path[i] - path[i-1]) * tension;
            out_vec = (path[i+1] - path[i]) * tension;
        }
        add_point(p, p - in_vec, p + out_vec);
    }
}
