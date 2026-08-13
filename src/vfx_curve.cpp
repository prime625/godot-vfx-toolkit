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

    ClassDB::bind_method(D_METHOD("evaluate", "t"), &VFXCurve::evaluate);
    ClassDB::bind_method(D_METHOD("evaluate_tangent", "t"), &VFXCurve::evaluate_tangent);
    ClassDB::bind_method(D_METHOD("tessellate", "segments_per_span"), &VFXCurve::tessellate, DEFVAL(16));

    ClassDB::bind_method(D_METHOD("to_tube_mesh", "radius", "segments", "rings", "cap_start", "cap_end"), &VFXCurve::to_tube_mesh, DEFVAL(0.1f), DEFVAL(16), DEFVAL(8), DEFVAL(true), DEFVAL(true));
    ClassDB::bind_method(D_METHOD("to_ribbon_mesh", "width", "segments"), &VFXCurve::to_ribbon_mesh, DEFVAL(0.2f), DEFVAL(16));
}

VFXCurve::VFXCurve() {}
VFXCurve::~VFXCurve() {}

void VFXCurve::clear() { points.clear(); }

int VFXCurve::add_point(const Vector3& pos, const Vector3& handle_in, const Vector3& handle_out) {
    Point p;
    p.position = pos;
    p.handle_in = handle_in.is_zero_approx() ? (pos - Vector3(1,0,0)) : handle_in;
    p.handle_out = handle_out.is_zero_approx() ? (pos + Vector3(1,0,0)) : handle_out;
    points.push_back(p);
    return points.size() - 1;
}

void VFXCurve::set_point(int idx, const Vector3& pos) {
    if (idx >= 0 && idx < (int)points.size()) points[idx].position = pos;
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
int VFXCurve::get_point_count() const { return points.size(); }

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
    t = vfx::clampf(t, 0.0f, 1.0f);
    float span_f = t * (points.size() - 1);
    int span = (int)floor(span_f);
    span = vfx::clampf(span, 0, (int)points.size() - 2);
    float local_t = span_f - span;
    return _eval_bezier(points[span].position, points[span].handle_out, points[span+1].handle_in, points[span+1].position, local_t);
}

Vector3 VFXCurve::evaluate_tangent(float t) const {
    if (points.size() < 2) return Vector3(0,0,1);
    t = vfx::clampf(t, 0.0f, 1.0f);
    float span_f = t * (points.size() - 1);
    int span = (int)floor(span_f);
    span = vfx::clampf(span, 0, (int)points.size() - 2);
    float local_t = span_f - span;
    return _eval_tangent(points[span].position, points[span].handle_out, points[span+1].handle_in, points[span+1].position, local_t).normalized();
}

PackedVector3Array VFXCurve::tessellate(int segments_per_span) const {
    PackedVector3Array arr;
    if (points.size() < 2) return arr;
    for (size_t i = 0; i + 1 < points.size(); i++) {
        for (int s = 0; s < segments_per_span; s++) {
            float t = (float)s / segments_per_span;
            arr.push_back(_eval_bezier(points[i].position, points[i].handle_out, points[i+1].handle_in, points[i+1].position, t));
        }
    }
    arr.push_back(points.back().position);
    return arr;
}

Ref<VFXMesh> VFXCurve::to_tube_mesh(float radius, int segments, int rings, bool cap_start, bool cap_end) const {
    PackedVector3Array pts;
    PackedVector3Array h_in, h_out;
    for (auto& p : points) {
        pts.push_back(p.position);
        h_in.push_back(p.handle_in);
        h_out.push_back(p.handle_out);
    }
    return VFXMesh::create_from_curve(pts, h_in, h_out, radius, segments, rings, cap_start, cap_end);
}

Ref<VFXMesh> VFXCurve::to_ribbon_mesh(float width, int segments) const {
    Ref<VFXMesh> m;
    m.instantiate();
    if (points.size() < 2) return m;

    std::vector<Vector3> samples;
    std::vector<Vector3> tangents;
    for (size_t i = 0; i + 1 < points.size(); i++) {
        for (int s = 0; s < segments; s++) {
            float t = (float)s / segments;
            samples.push_back(_eval_bezier(points[i].position, points[i].handle_out, points[i+1].handle_in, points[i+1].position, t));
            tangents.push_back(_eval_tangent(points[i].position, points[i].handle_out, points[i+1].handle_in, points[i+1].position, t).normalized());
        }
    }
    samples.push_back(points.back().position);
    tangents.push_back((points.back().position - points[points.size()-2].position).normalized());

    Vector3 ref = (fabs(tangents[0].dot(Vector3(0,1,0))) < 0.99f) ? Vector3(0,1,0) : Vector3(1,0,0);
    Vector3 x = ref.cross(tangents[0]).normalized();

    std::vector<int> left_verts, right_verts;
    for (size_t i = 0; i < samples.size(); i++) {
        if (i > 0) {
            Vector3 axis = tangents[i-1].cross(tangents[i]);
            if (axis.length_squared() > 0.0001f) {
                float angle = atan2(axis.length(), tangents[i-1].dot(tangents[i]));
                x = x.rotated(axis.normalized(), angle);
            }
        }
        Vector3 y = tangents[i].cross(x).normalized();
        left_verts.push_back(m->add_vertex(samples[i] - x * width * 0.5f, Vector2(0, (float)i / samples.size())));
        right_verts.push_back(m->add_vertex(samples[i] + x * width * 0.5f, Vector2(1, (float)i / samples.size())));
    }

    for (size_t i = 0; i + 1 < samples.size(); i++) {
        m->add_quad(left_verts[i], right_verts[i], right_verts[i+1], left_verts[i+1]);
    }

    m->recalculate_normals();
    m->link_twins();
    return m;
}

void VFXCurve::from_catmull_rom(const PackedVector3Array& path, float tension) {
    clear();
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
