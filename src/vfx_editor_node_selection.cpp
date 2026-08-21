#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

using namespace godot;

// ============================================================================
// RAYCAST SELECT (returns hit index only; does NOT modify selection state)
// ============================================================================
int VFXEditorNode::raycast_select(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (mesh.is_null()) return -1;
    Transform3D inv = get_global_transform().affine_inverse();
    Vector3 ro = inv.xform(ray_origin);
    Vector3 rd = inv.basis.xform(ray_dir).normalized();

    if (edit_mode == MODE_FACE) {
        Vector3 hit;
        int face_idx;
        if (mesh->raycast_select_face(ro, rd, hit, face_idx)) {
            return face_idx;
        }
    } else if (edit_mode == MODE_EDGE) {
        int edge_idx;
        if (mesh->raycast_select_edge(ro, rd, edge_idx)) {
            return edge_idx;
        }
    } else if (edit_mode == MODE_VERTEX) {
        int vert_idx;
        if (mesh->raycast_select_vertex(ro, rd, vert_idx)) {
            return vert_idx;
        }
    }
    return -1;
}

// ============================================================================
// SCREEN-SPACE SELECTION
// ============================================================================
int VFXEditorNode::screen_select_vertex(const Vector2& screen_pos) {
    if (!camera || mesh.is_null()) return -1;
    Transform3D world = get_global_transform();
    float tol_sq = select_pixel_tolerance * select_pixel_tolerance;
    int best = -1;
    float best_z = 1e20f;

    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        Vector3 w = world.xform(v->position);
        Vector2 sp = camera->unproject_position(w);
        float d = sp.distance_squared_to(screen_pos);
        if (d < tol_sq) {
            float z = camera->get_global_transform().xform_inv(w).z;
            if (z < best_z) {
                best_z = z;
                best = (int)v->id;
            }
        }
    }
    return best;
}

int VFXEditorNode::screen_select_edge(const Vector2& screen_pos) {
    if (!camera || mesh.is_null()) return -1;
    Transform3D world = get_global_transform();
    float tol_sq = select_pixel_tolerance * select_pixel_tolerance;
    int best = -1;
    float best_z = 1e20f;

    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = world.xform(e->next->vertex->position);
        Vector3 b = world.xform(e->vertex->position);
        Vector2 sa = camera->unproject_position(a);
        Vector2 sb = camera->unproject_position(b);
        float d = _point_segment_dist_sq_2d(screen_pos, sa, sb);
        if (d < tol_sq) {
            float z = (camera->get_global_transform().xform_inv(a).z +
                       camera->get_global_transform().xform_inv(b).z) * 0.5f;
            if (z < best_z) {
                best_z = z;
                best = (int)e->id;
            }
        }
    }
    return best;
}

int VFXEditorNode::screen_select_face(const Vector2& screen_pos) {
    if (!camera || mesh.is_null()) return -1;
    Transform3D world = get_global_transform();
    int best = -1;
    float best_z = 1e20f;

    for (auto* f : mesh->get_faces()) {
        if (f->deleted || !f->halfedge) continue;
        std::vector<vfx::HEVertex*> verts;
        mesh->get_face_vertices(f->id, verts);
        if (verts.size() < 3) continue;

        PackedVector2Array poly;
        float avg_z = 0.0f;
        for (auto* v : verts) {
            Vector3 w = world.xform(v->position);
            poly.append(camera->unproject_position(w));
            avg_z += camera->get_global_transform().xform_inv(w).z;
        }
        avg_z /= verts.size();

        if (_point_in_polygon_2d(screen_pos, poly)) {
            if (avg_z < best_z) {
                best_z = avg_z;
                best = (int)f->id;
            }
        }
    }
    return best;
}

// ============================================================================
// GIZMO UPDATE FOR SELECTION (multi-select aware)
// ============================================================================
void VFXEditorNode::_update_gizmo_for_selection() {
    Vector3 center;
    bool has = false;

    if (edit_mode == MODE_VERTEX && !selected_vertices.empty()) {
        for (int v : selected_vertices) {
            center += mesh->get_vertex_position(v);
        }
        center /= selected_vertices.size();
        has = true;
    } else if (edit_mode == MODE_EDGE && !selected_edges.empty()) {
        for (int e : selected_edges) {
            center += mesh->get_edge_midpoint(e);
        }
        center /= selected_edges.size();
        has = true;
    } else if (edit_mode == MODE_FACE && !selected_faces.empty()) {
        for (int f : selected_faces) {
            center += mesh->get_face_center(f);
        }
        center /= selected_faces.size();
        has = true;
    }

    // Fallback to single selection for compatibility
    if (!has) {
        if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
            center = mesh->get_vertex_position(selected_vertex);
            has = true;
        } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
            center = mesh->get_edge_midpoint(selected_edge);
            has = true;
        } else if (edit_mode == MODE_FACE && selected_face >= 0) {
            center = mesh->get_face_center(selected_face);
            has = true;
        }
    }

    if (has) {
        gizmo_transform.set_origin(center);
        gizmo_transform.basis = Basis();
        if (gizmo_node) {
            gizmo_node->set_transform(_get_visual_gizmo_transform());
            gizmo_node->set_visible(true);
            _build_gizmo_mesh();
        }
    } else {
        if (gizmo_node) gizmo_node->set_visible(false);
    }
    _update_gizmo_visibility();
}

// ============================================================================
// SCREEN-SPACE GIZMO RAYCAST
// ============================================================================
int VFXEditorNode::screen_raycast_gizmo(const Vector2& screen_pos) {
    if (!camera || !gizmo_node) return GIZMO_NONE;
    Vector3 ray_origin = camera->get_global_transform().origin;
    Vector3 ray_dir = camera->project_ray_normal(screen_pos);
    return raycast_gizmo(ray_origin, ray_dir);
}

// ============================================================================
// 2D HELPERS
// ============================================================================
float VFXEditorNode::_point_segment_dist_sq_2d(const Vector2& p, const Vector2& a, const Vector2& b) {
    Vector2 ab = b - a;
    float len_sq = ab.length_squared();
    if (len_sq < 0.0001f) return p.distance_squared_to(a);
    float t = MAX(0.0f, MIN(1.0f, (p - a).dot(ab) / len_sq));
    Vector2 closest = a + ab * t;
    return p.distance_squared_to(closest);
}

bool VFXEditorNode::_point_in_polygon_2d(const Vector2& p, const PackedVector2Array& poly) {
    bool inside = false;
    int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const Vector2& pi = poly[i];
        const Vector2& pj = poly[j];
        if (((pi.y > p.y) != (pj.y > p.y)) &&
            (p.x < (pj.x - pi.x) * (p.y - pi.y) / (pj.y - pi.y + 1e-10f) + pi.x)) {
            inside = !inside;
        }
    }
    return inside;
}
