#include "vfx_editor_node.h"
#include "vfx_math.h"
#include "vfx_editor_utils.h"
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ============================================================================
// SCREEN-SPACE SELECTION HELPERS
// ============================================================================
float VFXEditorNode::_point_segment_dist_sq_2d(const Vector2& p, const Vector2& a, const Vector2& b) {
    Vector2 ab = b - a;
    float len2 = ab.length_squared();
    if (len2 < 0.0001f) return p.distance_squared_to(a);
    float t = vfx::clampf((p - a).dot(ab) / len2, 0.0f, 1.0f);
    return p.distance_squared_to(a + ab * t);
}

bool VFXEditorNode::_point_in_polygon_2d(const Vector2& p, const PackedVector2Array& poly) {
    bool inside = false;
    int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x))
            inside = !inside;
    }
    return inside;
}

// ============================================================================
// SCREEN-SPACE SELECTION
// ============================================================================
int VFXEditorNode::screen_select_vertex(const Vector2& screen_pos) {
    if (!mesh.is_valid() || !camera) return -1;

    float best_depth = 1e20f;
    int best_idx = -1;
    Transform3D gt = _get_active_mesh_transform();
    float tol_sq = select_pixel_tolerance * select_pixel_tolerance;

    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;

        Vector3 world_pos = gt.xform(v->position);
        Vector3 cam_local = camera->get_global_transform().affine_inverse().xform(world_pos);

        if (cam_local.z >= 0.0f) continue;

        Vector2 screen = camera->unproject_position(world_pos);
        float dist_sq = screen.distance_squared_to(screen_pos);
        if (dist_sq > tol_sq) continue;

        float depth = -cam_local.z;
        if (depth < best_depth) {
            best_depth = depth;
            best_idx = (int)v->id;
        }
    }
    return best_idx;
}

int VFXEditorNode::screen_select_edge(const Vector2& screen_pos) {
    if (!mesh.is_valid() || !camera) return -1;

    float best_depth = 1e20f;
    int best_idx = -1;
    Transform3D gt = _get_active_mesh_transform();
    float tol_sq = select_pixel_tolerance * select_pixel_tolerance;
    float near_z = camera->get_near();  // positive value
    Transform3D cam_inv = camera->get_global_transform().affine_inverse();

    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;

        Vector3 a_world = gt.xform(e->next->vertex->position);
        Vector3 b_world = gt.xform(e->vertex->position);

        Vector3 a_local = cam_inv.xform(a_world);
        Vector3 b_local = cam_inv.xform(b_world);

        // Both behind camera — skip
        if (a_local.z >= -near_z && b_local.z >= -near_z) continue;

        // Clip to near plane if one point is behind
        Vector2 a_screen, b_screen;
        if (a_local.z >= -near_z) {
            float t = (-near_z - b_local.z) / (a_local.z - b_local.z);
            Vector3 clipped = b_local + (a_local - b_local) * t;
            a_screen = camera->unproject_position(camera->get_global_transform().xform(clipped));
            b_screen = camera->unproject_position(b_world);
        } else if (b_local.z >= -near_z) {
            float t = (-near_z - a_local.z) / (b_local.z - a_local.z);
            Vector3 clipped = a_local + (b_local - a_local) * t;
            b_screen = camera->unproject_position(camera->get_global_transform().xform(clipped));
            a_screen = camera->unproject_position(a_world);
        } else {
            a_screen = camera->unproject_position(a_world);
            b_screen = camera->unproject_position(b_world);
        }

        float dist_sq = _point_segment_dist_sq_2d(screen_pos, a_screen, b_screen);
        if (dist_sq > tol_sq) continue;

        float depth = 0.0f;
        int count = 0;
        if (a_local.z < -near_z) { depth += -a_local.z; count++; }
        if (b_local.z < -near_z) { depth += -b_local.z; count++; }
        if (count > 0) depth /= count;

        if (depth < best_depth) {
            best_depth = depth;
            best_idx = (int)e->id;
        }
    }
    return best_idx;
}

int VFXEditorNode::screen_select_face(const Vector2& screen_pos) {
    if (!mesh.is_valid() || !camera) return -1;

    float best_depth = 1e20f;
    int best_idx = -1;
    Transform3D gt = _get_active_mesh_transform();
    float tol_sq = select_pixel_tolerance * select_pixel_tolerance;

    // Pass 1: strict point-in-polygon
    for (auto* f : mesh->get_faces()) {
        if (f->deleted || !f->halfedge) continue;

        std::vector<vfx::HEVertex*> verts;
        mesh->get_face_vertices(f->id, verts);
        if (verts.size() < 3) continue;

        PackedVector2Array poly;
        float avg_depth = 0.0f;
        bool all_in_front = true;

        for (auto* v : verts) {
            Vector3 world_pos = gt.xform(v->position);
            Vector3 local = camera->get_global_transform().affine_inverse().xform(world_pos);
            if (local.z >= 0.0f) { all_in_front = false; break; }
            poly.push_back(camera->unproject_position(world_pos));
            avg_depth += -local.z;
        }

        if (!all_in_front) continue;
        if (!_point_in_polygon_2d(screen_pos, poly)) continue;

        avg_depth /= verts.size();
        if (avg_depth < best_depth) {
            best_depth = avg_depth;
            best_idx = (int)f->id;
        }
    }

    // Pass 2: fallback to face-center distance if polygon test missed
    if (best_idx < 0) {
        for (auto* f : mesh->get_faces()) {
            if (f->deleted || !f->halfedge) continue;
            Vector3 c_world = gt.xform(mesh->get_face_center(f->id));
            Vector3 local = camera->get_global_transform().affine_inverse().xform(c_world);
            if (local.z >= 0.0f) continue;
            Vector2 c_screen = camera->unproject_position(c_world);
            float dist_sq = c_screen.distance_squared_to(screen_pos);
            if (dist_sq > tol_sq) continue;
            float depth = -local.z;
            if (depth < best_depth) {
                best_depth = depth;
                best_idx = (int)f->id;
            }
        }
    }

    return best_idx;
}

// ============================================================================
// RAYCAST SELECTION (3D)
// ============================================================================
int VFXEditorNode::raycast_select(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (mesh.is_null()) return -1;
    Transform3D inv = _get_active_mesh_transform().affine_inverse();
    Vector3 ro = inv.xform(ray_origin);
    Vector3 rd = inv.basis.xform(ray_dir).normalized();

    if (edit_mode == MODE_FACE) {
        Vector3 hit;
        int face_idx;
        if (mesh->raycast_select_face(ro, rd, hit, face_idx)) return face_idx;
    } else if (edit_mode == MODE_EDGE) {
        int edge_idx;
        if (mesh->raycast_select_edge(ro, rd, edge_idx)) return edge_idx;
    } else if (edit_mode == MODE_VERTEX) {
        int vert_idx;
        if (mesh->raycast_select_vertex(ro, rd, vert_idx)) return vert_idx;
    }
    return -1;
}

// ============================================================================
// GIZMO PLACEMENT FOR SELECTION
// ============================================================================
void VFXEditorNode::_update_gizmo_for_selection() {
    if (edit_mode == MODE_OBJECT && active_scene_node.is_valid()) {
        gizmo_transform = active_scene_node->get_global_transform();
    } else if (edit_mode == MODE_OBJECT) {
        gizmo_transform = Transform3D();
    }
    else if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        Vector3 center;
        int count = 0;

        // Single selection
        if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
            center = mesh->get_vertices()[selected_vertex]->position;
            count = 1;
        } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
            int v0, v1;
            mesh->get_edge_endpoints(selected_edge, v0, v1);
            if (v0 >= 0 && v1 >= 0) {
                center = (mesh->get_vertices()[v0]->position + mesh->get_vertices()[v1]->position) * 0.5f;
                count = 2;
            }
        } else if (edit_mode == MODE_FACE && selected_face >= 0) {
            std::vector<vfx::HEVertex*> fverts;
            mesh->get_face_vertices(selected_face, fverts);
            for (auto* v : fverts) {
                if (!v->deleted) {
                    center += v->position;
                    count++;
                }
            }
            if (count > 0) center /= (float)count;
        }

        // Multi-select fallback
        if (count == 0) {
            if (edit_mode == MODE_VERTEX && !selected_vertices.empty()) {
                for (int v : selected_vertices) {
                    if (v >= 0 && v < mesh->get_vertex_count() && !mesh->get_vertices()[v]->deleted) {
                        center += mesh->get_vertices()[v]->position;
                        count++;
                    }
                }
            } else if (edit_mode == MODE_EDGE && !selected_edges.empty()) {
                for (int e : selected_edges) {
                    int v0, v1;
                    mesh->get_edge_endpoints(e, v0, v1);
                    if (v0 >= 0 && !mesh->get_vertices()[v0]->deleted) {
                        center += mesh->get_vertices()[v0]->position;
                        count++;
                    }
                    if (v1 >= 0 && !mesh->get_vertices()[v1]->deleted) {
                        center += mesh->get_vertices()[v1]->position;
                        count++;
                    }
                }
            } else if (edit_mode == MODE_FACE && !selected_faces.empty()) {
                for (int f : selected_faces) {
                    std::vector<vfx::HEVertex*> fverts;
                    mesh->get_face_vertices(f, fverts);
                    for (auto* v : fverts) {
                        if (!v->deleted) {
                            center += v->position;
                            count++;
                        }
                    }
                }
            }
            if (count > 0) center /= (float)count;
        }

        if (count > 0) {
            gizmo_transform.set_origin(center);
            if (!gizmo_local) {
                gizmo_transform.set_basis(Transform3D().get_basis());
            }
        }
    }

    if (gizmo_node) {
        gizmo_node->set_transform(_get_visual_gizmo_transform());
        _build_gizmo_mesh();
    }
}




// ============================================================================
// BONE PICKING
// ============================================================================
int VFXEditorNode::raycast_bone(const Vector3& ray_origin, const Vector3& ray_dir) const {
    if (skeleton.is_null()) return -1;
    if (!show_skeleton) return -1;
    if (skel_visual && !skel_visual->is_visible()) return -1;

    skeleton->update_transforms();

    int best = -1;
    float best_t = 1e20f;
    Vector3 rd = ray_dir.normalized();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        float t;
        float radius = (tail - head).length() * 0.35f;
        if (radius < 0.08f) radius = 0.08f;

        if (vfx_editor::ray_vs_segment(ray_origin, rd, head, tail, radius, t)) {
            if (t < best_t) {
                best_t = t;
                best = i;
            }
        }
    }
    return best;
}


// ============================================================================
// BOX / MARQUEE SELECTION
// ============================================================================

static bool _segment_intersects_rect_2d(const Vector2& a, const Vector2& b, const Rect2& rect) {
    // Endpoint inside rect
    if (rect.has_point(a) || rect.has_point(b)) return true;
    // Check intersection with each edge of the rect
    Vector2 tl = rect.position;
    Vector2 tr = rect.position + Vector2(rect.size.x, 0);
    Vector2 br = rect.position + rect.size;
    Vector2 bl = rect.position + Vector2(0, rect.size.y);

    auto ccw = [](const Vector2& A, const Vector2& B, const Vector2& C) -> float {
        return (C.y - A.y) * (B.x - A.x) - (B.y - A.y) * (C.x - A.x);
    };
    auto intersect = [&](const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p4) -> bool {
        float d1 = ccw(p3, p4, p1);
        float d2 = ccw(p3, p4, p2);
        float d3 = ccw(p1, p2, p3);
        float d4 = ccw(p1, p2, p4);
        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
            return true;
        // Collinear on segment — simplified, check bounding box overlap
        if (d1 == 0 && d2 == 0 && d3 == 0 && d4 == 0) {
            return (MAX(MIN(p1.x, p2.x), MIN(p3.x, p4.x)) <= MIN(MAX(p1.x, p2.x), MAX(p3.x, p4.x))) &&
                   (MAX(MIN(p1.y, p2.y), MIN(p3.y, p4.y)) <= MIN(MAX(p1.y, p2.y), MAX(p3.y, p4.y)));
        }
        return false;
    };
    return intersect(a, b, tl, tr) || intersect(a, b, tr, br) ||
           intersect(a, b, br, bl) || intersect(a, b, bl, tl);
}

PackedInt32Array VFXEditorNode::screen_select_box(const Rect2& screen_rect) const {
    PackedInt32Array result;
    if (!mesh.is_valid() || !camera || screen_rect.size.x < 2.0f || screen_rect.size.y < 2.0f)
        return result;

    Transform3D gt = _get_active_mesh_transform();
    Transform3D cam_inv = camera->get_global_transform().affine_inverse();
    float near_z = camera->get_near();

    if (edit_mode == MODE_VERTEX) {
        for (auto* v : mesh->get_vertices()) {
            if (v->deleted) continue;
            Vector3 world = gt.xform(v->position);
            Vector3 local = cam_inv.xform(world);
            if (local.z >= -near_z) continue;
            Vector2 sp = camera->unproject_position(world);
            if (screen_rect.has_point(sp))
                result.push_back((int)v->id);
        }
    }
    else if (edit_mode == MODE_EDGE) {
        for (auto* e : mesh->get_edges()) {
            if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;

            Vector3 a_world = gt.xform(e->next->vertex->position);
            Vector3 b_world = gt.xform(e->vertex->position);
            Vector3 a_local = cam_inv.xform(a_world);
            Vector3 b_local = cam_inv.xform(b_world);

            if (a_local.z >= -near_z && b_local.z >= -near_z) continue;

            Vector2 a_screen, b_screen;
            if (a_local.z >= -near_z) {
                float t = (-near_z - b_local.z) / (a_local.z - b_local.z);
                Vector3 clipped = b_local + (a_local - b_local) * t;
                a_screen = camera->unproject_position(camera->get_global_transform().xform(clipped));
                b_screen = camera->unproject_position(b_world);
            } else if (b_local.z >= -near_z) {
                float t = (-near_z - a_local.z) / (b_local.z - a_local.z);
                Vector3 clipped = a_local + (b_local - a_local) * t;
                b_screen = camera->unproject_position(camera->get_global_transform().xform(clipped));
                a_screen = camera->unproject_position(a_world);
            } else {
                a_screen = camera->unproject_position(a_world);
                b_screen = camera->unproject_position(b_world);
            }

            if (_segment_intersects_rect_2d(a_screen, b_screen, screen_rect))
                result.push_back((int)e->id);
        }
    }
    else if (edit_mode == MODE_FACE) {
        for (auto* f : mesh->get_faces()) {
            if (f->deleted || !f->halfedge) continue;

            std::vector<vfx::HEVertex*> fverts;
            mesh->get_face_vertices(f->id, fverts);
            if (fverts.size() < 3) continue;

            bool any_inside = false;
            bool all_in_front = true;
            float avg_depth = 0.0f;
            int in_front_count = 0;

            for (auto* v : fverts) {
                Vector3 world = gt.xform(v->position);
                Vector3 local = cam_inv.xform(world);
                if (local.z < -near_z) {
                    Vector2 sp = camera->unproject_position(world);
                    if (screen_rect.has_point(sp)) any_inside = true;
                    avg_depth += -local.z;
                    in_front_count++;
                } else {
                    all_in_front = false;
                }
            }

            if (any_inside && in_front_count > 0) {
                result.push_back((int)f->id);
                continue;
            }

            // Fallback: face center inside rect
            if (!all_in_front && in_front_count > 0) {
                Vector3 c_world = gt.xform(mesh->get_face_center(f->id));
                Vector3 c_local = cam_inv.xform(c_world);
                if (c_local.z < -near_z) {
                    Vector2 c_screen = camera->unproject_position(c_world);
                    if (screen_rect.has_point(c_screen))
                        result.push_back((int)f->id);
                }
            }
        }
    }
    return result;
}

void VFXEditorNode::box_select(const Rect2& screen_rect) {
    PackedInt32Array hits = screen_select_box(screen_rect);
    if (hits.size() == 0) {
        if (selection_mode == SELECTION_MODE_SINGLE)
            clear_selection();
        return;
    }

    switch (selection_mode) {
        case SELECTION_MODE_SINGLE:
            clear_selection();
            // fallthrough to populate
        case SELECTION_MODE_MULTI:
            if (edit_mode == MODE_VERTEX) {
                for (int i = 0; i < hits.size(); i++) selected_vertices.insert(hits[i]);
                selected_vertex = hits[0];
            } else if (edit_mode == MODE_EDGE) {
                for (int i = 0; i < hits.size(); i++) selected_edges.insert(hits[i]);
                selected_edge = hits[0];
            } else if (edit_mode == MODE_FACE) {
                for (int i = 0; i < hits.size(); i++) selected_faces.insert(hits[i]);
                selected_face = hits[0];
            }
            break;

        case SELECTION_MODE_LOOP:
            if (!mesh.is_valid()) {
                // Fallback to raw selection
                if (edit_mode == MODE_VERTEX) {
                    clear_selection();
                    for (int i = 0; i < hits.size(); i++) selected_vertices.insert(hits[i]);
                    selected_vertex = hits[0];
                } else if (edit_mode == MODE_EDGE) {
                    clear_selection();
                    for (int i = 0; i < hits.size(); i++) selected_edges.insert(hits[i]);
                    selected_edge = hits[0];
                } else if (edit_mode == MODE_FACE) {
                    clear_selection();
                    for (int i = 0; i < hits.size(); i++) selected_faces.insert(hits[i]);
                    selected_face = hits[0];
                }
                break;
            }
            if (edit_mode == MODE_VERTEX) {
                clear_selection();
                std::unordered_set<int> all_verts;
                for (int i = 0; i < hits.size(); i++) {
                    PackedInt32Array loop = mesh->get_vertex_loop(hits[i]);
                    for (int j = 0; j < loop.size(); j++) all_verts.insert(loop[j]);
                }
                for (int v : all_verts) selected_vertices.insert(v);
                selected_vertex = hits[0];
            } else if (edit_mode == MODE_EDGE) {
                clear_selection();
                std::unordered_set<int> all_edges;
                for (int i = 0; i < hits.size(); i++) {
                    PackedInt32Array loop = mesh->get_edge_loop(hits[i]);
                    for (int j = 0; j < loop.size(); j++) all_edges.insert(loop[j]);
                }
                for (int e : all_edges) selected_edges.insert(e);
                selected_edge = hits[0];
            } else if (edit_mode == MODE_FACE) {
                clear_selection();
                std::unordered_set<int> all_faces;
                for (int i = 0; i < hits.size(); i++) {
                    PackedInt32Array loop = mesh->get_face_loop(hits[i]);
                    for (int j = 0; j < loop.size(); j++) all_faces.insert(loop[j]);
                }
                for (int f : all_faces) selected_faces.insert(f);
                selected_face = hits[0];
            }
            break;
    }

    _build_selection_mesh();
    _update_gizmo_for_selection();
}

