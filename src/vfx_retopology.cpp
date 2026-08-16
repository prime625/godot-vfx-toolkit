#include "vfx_retopology.h"
#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <unordered_set>

using namespace godot;

void VFXRetopology::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXRetopology::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXRetopology::get_mesh);

    ClassDB::bind_method(D_METHOD("set_feature_angle", "degrees"), &VFXRetopology::set_feature_angle);
    ClassDB::bind_method(D_METHOD("get_feature_angle"), &VFXRetopology::get_feature_angle);
    ClassDB::bind_method(D_METHOD("set_target_edge_length", "len"), &VFXRetopology::set_target_edge_length);
    ClassDB::bind_method(D_METHOD("get_target_edge_length"), &VFXRetopology::get_target_edge_length);
    ClassDB::bind_method(D_METHOD("set_preserve_boundary", "enable"), &VFXRetopology::set_preserve_boundary);
    ClassDB::bind_method(D_METHOD("get_preserve_boundary"), &VFXRetopology::get_preserve_boundary);
    ClassDB::bind_method(D_METHOD("set_preserve_features", "enable"), &VFXRetopology::set_preserve_features);
    ClassDB::bind_method(D_METHOD("get_preserve_features"), &VFXRetopology::get_preserve_features);
    ClassDB::bind_method(D_METHOD("set_max_iterations", "iters"), &VFXRetopology::set_max_iterations);
    ClassDB::bind_method(D_METHOD("get_max_iterations"), &VFXRetopology::get_max_iterations);

    ClassDB::bind_method(D_METHOD("retopologize", "target_faces"), &VFXRetopology::retopologize);
    ClassDB::bind_method(D_METHOD("remesh_uniform", "edge_length", "iterations"), &VFXRetopology::remesh_uniform);
    ClassDB::bind_method(D_METHOD("simplify_quad_dominant", "target_faces"), &VFXRetopology::simplify_quad_dominant);
    ClassDB::bind_method(D_METHOD("decimate_preserve_topology", "target_faces"), &VFXRetopology::decimate_preserve_topology);
    ClassDB::bind_method(D_METHOD("relax_vertices", "iterations"), &VFXRetopology::relax_vertices);

    ClassDB::bind_method(D_METHOD("get_edge_loops"), &VFXRetopology::get_edge_loops);
    ClassDB::bind_method(D_METHOD("get_feature_edges"), &VFXRetopology::get_feature_edges);
    ClassDB::bind_method(D_METHOD("get_progress"), &VFXRetopology::get_progress);
    ClassDB::bind_method(D_METHOD("get_status"), &VFXRetopology::get_status);
}

VFXRetopology::VFXRetopology() {}
VFXRetopology::~VFXRetopology() {}

void VFXRetopology::set_mesh(const Ref<VFXMesh>& p_mesh) { mesh = p_mesh; }
Ref<VFXMesh> VFXRetopology::get_mesh() const { return mesh; }
void VFXRetopology::set_feature_angle(float d) { feature_angle_threshold = d; }
float VFXRetopology::get_feature_angle() const { return feature_angle_threshold; }
void VFXRetopology::set_target_edge_length(float l) { target_edge_length = l; }
float VFXRetopology::get_target_edge_length() const { return target_edge_length; }
void VFXRetopology::set_preserve_boundary(bool e) { preserve_boundary = e; }
bool VFXRetopology::get_preserve_boundary() const { return preserve_boundary; }
void VFXRetopology::set_preserve_features(bool e) { preserve_features = e; }
bool VFXRetopology::get_preserve_features() const { return preserve_features; }
void VFXRetopology::set_max_iterations(int i) { max_iterations = i; }
int VFXRetopology::get_max_iterations() const { return max_iterations; }
float VFXRetopology::get_progress() const { return progress; }
String VFXRetopology::get_status() const { return status; }

void VFXRetopology::_set_progress(float p, const String& msg) {
    progress = p;
    status = msg;
}


bool VFXRetopology::retopologize(int target_faces) {
    if (mesh.is_null()) return false;
    if (target_faces < 4) target_faces = 4;

    int start_faces = mesh->get_face_count();
    if (start_faces <= target_faces) {
        _set_progress(1.0f, "Target already reached");
        return true;
    }

    _set_progress(0.0f, "Analyzing mesh...");
    _classify_edges();
    _detect_edge_loops();
    _compute_curvature();
    _mark_protected();

    _set_progress(0.15f, "Simplifying with topology preservation...");
    _simplify_pass(target_faces);

    _set_progress(0.70f, "Optimizing quad flow...");
    for (int i = 0; i < max_iterations; i++) {
        _remesh_pass();
        _set_progress(0.70f + 0.25f * ((float)i / max_iterations), "Remeshing pass...");
    }

    _set_progress(0.95f, "Relaxing vertices...");
    _relax_vertices(3);

    mesh->recalculate_normals();
    mesh->link_twins();

    _set_progress(1.0f, "Done — " + String::num_int64(start_faces) + " -> " + String::num_int64(mesh->get_face_count()) + " faces");
    return true;
}

bool VFXRetopology::remesh_uniform(float edge_length, int iterations) {
    if (mesh.is_null()) return false;
    target_edge_length = edge_length;

    _classify_edges();
    _mark_protected();

    for (int iter = 0; iter < iterations; iter++) {
        _set_progress((float)iter / iterations, "Uniform remesh...");

        // Split long edges
        std::vector<int> to_split;
        int ec = mesh->get_edge_count();
        for (int i = 0; i < ec; i++) {
            if (_edge_length(i) > 1.5f * target_edge_length) {
                if (!mesh->is_edge_boundary(i) || !preserve_boundary) {
                    to_split.push_back(i);
                }
            }
        }
        for (int eid : to_split) mesh->split_edge(eid);

        // Collapse short edges
        std::vector<std::pair<float, int>> to_collapse;
        ec = mesh->get_edge_count();
        for (int i = 0; i < ec; i++) {
            float len = _edge_length(i);
            if (len < 0.5f * target_edge_length && _can_collapse_edge(i)) {
                to_collapse.push_back({len, i});
            }
        }
        std::sort(to_collapse.begin(), to_collapse.end());
        for (auto& p : to_collapse) {
            int v0, v1;
            if (mesh->get_edge_vertices(p.second, v0, v1)) {
                // Collapse toward the vertex with higher curvature (preserves features)
                float c0 = (v0 < (int)vertex_curvature.size()) ? vertex_curvature[v0] : 0.0f;
                float c1 = (v1 < (int)vertex_curvature.size()) ? vertex_curvature[v1] : 0.0f;
                mesh->collapse_edge(p.second, (c0 > c1) ? v0 : v1);
            }
        }

        // Flip for valence
        ec = mesh->get_edge_count();
        for (int i = 0; i < ec; i++) {
            if (_can_flip_edge(i)) mesh->flip_edge(i);
        }

        _relax_vertices(1);
    }

    mesh->recalculate_normals();
    mesh->link_twins();
    _set_progress(1.0f, "Done");
    return true;
}

bool VFXRetopology::simplify_quad_dominant(int target_faces) {
    if (mesh.is_null()) return false;
    _classify_edges();
    _detect_edge_loops();
    _mark_protected();
    _simplify_pass(target_faces);
    mesh->recalculate_normals();
    mesh->link_twins();
    return true;
}

bool VFXRetopology::decimate_preserve_topology(int target_faces) {
    if (mesh.is_null()) return false;
    _classify_edges();
    _mark_protected();

    int current = mesh->get_face_count();
    int needed = current - target_faces;
    int done = 0;

    auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)> queue(cmp);

    int ec = mesh->get_edge_count();
    for (int i = 0; i < ec; i++) {
        if (_can_collapse_edge(i)) {
            float cost = _edge_length(i);
            if (i < (int)edge_types.size()) {
                if (edge_types[i] == vfx::EDGE_FEATURE) cost *= 10.0f;
                if (edge_types[i] == vfx::EDGE_BOUNDARY) cost *= 5.0f;
            }
            queue.push({cost, i});
        }
    }

    while (done < needed && !queue.empty()) {
        auto [cost, eid] = queue.top();
        queue.pop();

        if (_can_collapse_edge(eid)) {
            int v0, v1;
            if (mesh->get_edge_vertices(eid, v0, v1)) {
                float c0 = (v0 < (int)vertex_curvature.size()) ? vertex_curvature[v0] : 0.0f;
                float c1 = (v1 < (int)vertex_curvature.size()) ? vertex_curvature[v1] : 0.0f;
                if (mesh->collapse_edge(eid, (c0 > c1) ? v0 : v1)) {
                    done++;
                    current--;
                }
            }
        }
        if (done % 10 == 0) {
            _set_progress((float)done / needed, "Decimating...");
        }
    }

    mesh->recalculate_normals();
    mesh->link_twins();
    _set_progress(1.0f, "Done — removed " + String::num_int64(done) + " faces");
    return true;
}

void VFXRetopology::relax_vertices(int iterations) {
    _relax_vertices(iterations);
    mesh->recalculate_normals();
}


void VFXRetopology::_classify_edges() {
    int ec = mesh->get_edge_count();
    edge_types.resize(ec, vfx::EDGE_NORMAL);

    float cos_thresh = cosf(feature_angle_threshold * 3.14159265f / 180.0f);

    for (int i = 0; i < ec; i++) {
        if (mesh->is_edge_boundary(i)) {
            edge_types[i] = vfx::EDGE_BOUNDARY;
            continue;
        }

        float dihedral = _dihedral_angle(i);
        if (dihedral < cos_thresh) {
            edge_types[i] = vfx::EDGE_FEATURE;
        }
    }
}

void VFXRetopology::_detect_edge_loops() {
    edge_loops.clear();
    int ec = mesh->get_edge_count();
    std::vector<bool> visited(ec, false);

    for (int i = 0; i < ec; i++) {
        if (visited[i]) continue;
        if (edge_types[i] != vfx::EDGE_FEATURE && edge_types[i] != vfx::EDGE_BOUNDARY) continue;

        vfx::EdgeLoop loop;
        loop.is_feature = (edge_types[i] == vfx::EDGE_FEATURE);
        loop.is_boundary = (edge_types[i] == vfx::EDGE_BOUNDARY);

        // Greedy walk: follow edges sharing vertices
        int current = i;
        int start_v = -1, prev_v = -1;

        while (!visited[current]) {
            visited[current] = true;
            loop.edge_ids.push_back(current);

            int v0, v1;
            if (!mesh->get_edge_vertices(current, v0, v1)) break;

            if (start_v < 0) start_v = v0;
            int next_v = (prev_v == v0) ? v1 : v0;
            prev_v = next_v;

            // Find next edge of same type sharing next_v
            bool found = false;
            for (int j = 0; j < ec; j++) {
                if (!visited[j] && edge_types[j] == edge_types[i]) {
                    int a, b;
                    if (mesh->get_edge_vertices(j, a, b)) {
                        if (a == next_v || b == next_v) {
                            current = j;
                            found = true;
                            break;
                        }
                    }
                }
            }
            if (!found) break;
        }

        if (loop.edge_ids.size() > 2) {
            loop.importance = loop.is_boundary ? 2.0f : 1.5f;
            edge_loops.push_back(loop);
        }
    }
}

void VFXRetopology::_compute_curvature() {
    int vc = mesh->get_vertex_count();
    vertex_curvature.resize(vc, 0.0f);

    for (int i = 0; i < vc; i++) {
        std::vector<int> neighbors;
        mesh->get_vertex_neighbors(i, neighbors);
        if (neighbors.size() < 3) continue;

        Vector3 center = mesh->get_vertex_position(i);
        float total_angle = 0.0f;

        for (size_t j = 0; j < neighbors.size(); j++) {
            int next = neighbors[(j + 1) % neighbors.size()];
            Vector3 a = mesh->get_vertex_position(neighbors[j]) - center;
            Vector3 b = mesh->get_vertex_position(next) - center;
            float angle = atan2f(a.cross(b).length(), a.dot(b));
            total_angle += angle;
        }

        float expected = (neighbors.size() - 2) * 3.14159265f;
        vertex_curvature[i] = fabsf(expected - total_angle);
    }
}

void VFXRetopology::_mark_protected() {
    protected_edges.clear();
    protected_vertices.clear();

    for (const auto& loop : edge_loops) {
        if (loop.importance > 1.0f) {
            for (int eid : loop.edge_ids) protected_edges.insert(eid);
        }
    }

    for (int i = 0; i < (int)vertex_curvature.size(); i++) {
        if (vertex_curvature[i] > 1.0f) protected_vertices.insert(i);
    }
}


bool VFXRetopology::_can_collapse_edge(int edge_id) const {
    if (edge_id < 0 || edge_id >= mesh->get_edge_count()) return false;
    if (protected_edges.find(edge_id) != protected_edges.end()) return false;
    if (mesh->is_edge_boundary(edge_id) && preserve_boundary) return false;

    // Don't collapse if either vertex is protected
    int v0, v1;
    if (!mesh->get_edge_vertices(edge_id, v0, v1)) return false;
    if (protected_vertices.find(v0) != protected_vertices.end()) return false;
    if (protected_vertices.find(v1) != protected_vertices.end()) return false;

    // Valence check: don't create vertices with valence < 3
    int val0 = _vertex_valence(v0);
    int val1 = _vertex_valence(v1);
    if (val0 <= 3 || val1 <= 3) return false;

    return true;
}

bool VFXRetopology::_can_flip_edge(int edge_id) const {
    if (edge_id < 0 || edge_id >= mesh->get_edge_count()) return false;
    if (mesh->is_edge_boundary(edge_id)) return false;
    if (protected_edges.find(edge_id) != protected_edges.end()) return false;
    return true;
}

void VFXRetopology::_remesh_pass() {
    // Collapse short edges
    std::vector<std::pair<float, int>> short_edges;
    int ec = mesh->get_edge_count();
    for (int i = 0; i < ec; i++) {
        float len = _edge_length(i);
        if (len < 0.7f * target_edge_length && _can_collapse_edge(i)) {
            short_edges.push_back({len, i});
        }
    }
    std::sort(short_edges.begin(), short_edges.end());
    for (auto& p : short_edges) {
        int v0, v1;
        if (mesh->get_edge_vertices(p.second, v0, v1)) {
            float c0 = (v0 < (int)vertex_curvature.size()) ? vertex_curvature[v0] : 0.0f;
            float c1 = (v1 < (int)vertex_curvature.size()) ? vertex_curvature[v1] : 0.0f;
            mesh->collapse_edge(p.second, (c0 > c1) ? v0 : v1);
        }
    }

    // Split long edges
    std::vector<std::pair<float, int>> long_edges;
    ec = mesh->get_edge_count();
    for (int i = 0; i < ec; i++) {
        float len = _edge_length(i);
        if (len > 1.4f * target_edge_length) {
            long_edges.push_back({len, i});
        }
    }
    for (auto& p : long_edges) {
        mesh->split_edge(p.second);
    }

    // Flip for valence
    ec = mesh->get_edge_count();
    for (int i = 0; i < ec; i++) {
        if (_can_flip_edge(i)) mesh->flip_edge(i);
    }
}

void VFXRetopology::_simplify_pass(int target_faces) {
    int current = mesh->get_face_count();
    int needed = current - target_faces;
    int done = 0;

    while (done < needed && current > target_faces) {
        float best_cost = 1e30f;
        int best_edge = -1;
        int best_keep = -1;

        int ec = mesh->get_edge_count();
        for (int i = 0; i < ec; i++) {
            if (!_can_collapse_edge(i)) continue;

            int v0, v1;
            if (!mesh->get_edge_vertices(i, v0, v1)) continue;

            float cost = _edge_length(i);
            float c0 = (v0 < (int)vertex_curvature.size()) ? vertex_curvature[v0] : 0.0f;
            float c1 = (v1 < (int)vertex_curvature.size()) ? vertex_curvature[v1] : 0.0f;

            // Penalize collapsing across features
            if (i < (int)edge_types.size() && edge_types[i] == vfx::EDGE_FEATURE) cost *= 10.0f;
            cost += (c0 + c1) * 0.1f;

            if (cost < best_cost) {
                best_cost = cost;
                best_edge = i;
                best_keep = (c0 > c1) ? v0 : v1;
            }
        }

        if (best_edge < 0) break;

        if (mesh->collapse_edge(best_edge, best_keep)) {
            done++;
            current--;
        } else {
            protected_edges.insert(best_edge);
        }

        if (done % 10 == 0) {
            _set_progress(0.15f + 0.55f * ((float)done / fmax(needed, 1)), "Simplifying...");
        }
    }
}

void VFXRetopology::_relax_vertices(int iterations) {
    int vc = mesh->get_vertex_count();
    if (vc == 0) return;

    std::vector<Vector3> new_positions(vc);

    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < vc; i++) {
            if (protected_vertices.find(i) != protected_vertices.end()) continue;
            if (_is_boundary_vertex(i) && preserve_boundary) continue;

            std::vector<int> neighbors;
            mesh->get_vertex_neighbors(i, neighbors);
            if (neighbors.empty()) continue;

            Vector3 avg;
            for (int n : neighbors) avg += mesh->get_vertex_position(n);
            avg /= (float)neighbors.size();

            Vector3 original = mesh->get_vertex_position(i);
            Vector3 normal = mesh->get_vertex_normal(i);
            Vector3 delta = avg - original;
            delta -= normal * delta.dot(normal);

            new_positions[i] = original + delta * 0.5f;
        }

        for (int i = 0; i < vc; i++) {
            if (protected_vertices.find(i) == protected_vertices.end()) {
                mesh->set_vertex_position(i, new_positions[i]);
            }
        }
    }
}


float VFXRetopology::_edge_length(int edge_id) const {
    int v0, v1;
    if (!mesh->get_edge_vertices(edge_id, v0, v1)) return 0.0f;
    return mesh->get_vertex_position(v0).distance_to(mesh->get_vertex_position(v1));
}

float VFXRetopology::_dihedral_angle(int edge_id) const {
    int faces[2];
    int count = mesh->get_edge_faces(edge_id, faces);
    if (count < 2) return -1.0f;

    Vector3 n1 = _face_normal(faces[0]);
    Vector3 n2 = _face_normal(faces[1]);
    return n1.dot(n2);
}

Vector3 VFXRetopology::_face_normal(int face_id) const {
    // Extract face vertices and compute normal
    // Since we don't have direct face vertex access, approximate from mesh data
    // In practice, you'd walk the half-edge loop
    PackedInt32Array indices = mesh->get_indices();
    if (indices.size() < 3) return Vector3(0, 1, 0);

    // Use first triangle as approximation
    Vector3 p0 = mesh->get_vertex_position(indices[0]);
    Vector3 p1 = mesh->get_vertex_position(indices[1]);
    Vector3 p2 = mesh->get_vertex_position(indices[2]);
    return (p1 - p0).cross(p2 - p0).normalized();
}

bool VFXRetopology::_is_boundary_vertex(int vidx) const {
    // A vertex is boundary if any of its edges is boundary
    int ec = mesh->get_edge_count();
    for (int i = 0; i < ec; i++) {
        int v0, v1;
        if (mesh->get_edge_vertices(i, v0, v1)) {
            if ((v0 == vidx || v1 == vidx) && mesh->is_edge_boundary(i)) {
                return true;
            }
        }
    }
    return false;
}

int VFXRetopology::_vertex_valence(int vidx) const {
    std::vector<int> neighbors;
    mesh->get_vertex_neighbors(vidx, neighbors);
    return neighbors.size();
}


PackedInt32Array VFXRetopology::get_edge_loops() const {
    PackedInt32Array result;
    for (const auto& loop : edge_loops) {
        for (int eid : loop.edge_ids) result.append(eid);
        result.append(-1);
    }
    return result;
}

PackedInt32Array VFXRetopology::get_feature_edges() const {
    PackedInt32Array result;
    for (size_t i = 0; i < edge_types.size(); i++) {
        if (edge_types[i] == vfx::EDGE_FEATURE || edge_types[i] == vfx::EDGE_BOUNDARY) {
            result.append(i);
        }
    }
    return result;
}
