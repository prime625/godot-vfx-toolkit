#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include "vfx_math.h"
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <cstdlib>

using namespace godot;

// ============================================================================
// INLINE HELPERS — cone for arrowheads
// ============================================================================
static void append_cone(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
    const Vector3& tip, const Vector3& dir, float height, float radius, int segs, const Color& col) {
    Vector3 base_center = tip - dir.normalized() * height;
    Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = dir.cross(up).normalized();
    up = right.cross(dir).normalized();

    int tip_idx = verts.size();
    verts.push_back(tip); cols.push_back(col);

    int base_start = verts.size();
    for (int i = 0; i < segs; i++) {
        float ang = (float)i / (float)segs * 3.14159265f * 2.0f;
        Vector3 p = base_center + right * cosf(ang) * radius + up * sinf(ang) * radius;
        verts.push_back(p); cols.push_back(col);
    }

    for (int i = 0; i < segs; i++) {
        int a = base_start + i;
        int b = base_start + (i + 1) % segs;
        idx.push_back(tip_idx); idx.push_back(b); idx.push_back(a);
    }
}

// ============================================================================
// GIZMO — RAYCAST (3D)
// ============================================================================
int VFXEditorNode::raycast_gizmo(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!gizmo_node || !gizmo_node->is_visible()) return GIZMO_NONE;

    Transform3D inv = _get_visual_gizmo_transform().affine_inverse();
    Vector3 ro = inv.xform(ray_origin);
    Vector3 rd = inv.basis.xform(ray_dir).normalized();

    float s = gizmo_screen_scale;
    float best_t = 1e20f;
    int best = GIZMO_NONE;

    if (gizmo_mode == GIZMO_TRANSLATE) {
        auto test_axis = [&](int axis, const Vector3& dir) {
            float t;
            if (vfx_editor::ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.12f, t)) {
                if (t < best_t) { best_t = t; best = axis; }
            }
        };

        test_axis(GIZMO_X, Vector3(1, 0, 0));
        test_axis(GIZMO_Y, Vector3(0, 1, 0));
        test_axis(GIZMO_Z, Vector3(0, 0, 1));

        auto test_plane_axis = [&](int axis, const Plane& pl, float c1min, float c1max, float c2min, float c2max, int c1_axis, int c2_axis) {
            Vector3 hit;
            if (!vfx_editor::ray_vs_plane(ro, rd, pl, hit)) return;
            float c1 = (c1_axis == 0) ? hit.x : (c1_axis == 1) ? hit.y : hit.z;
            float c2 = (c2_axis == 0) ? hit.x : (c2_axis == 1) ? hit.y : hit.z;
            if (c1 >= c1min && c1 <= c1max && c2 >= c2min && c2 <= c2max) {
                float t = (hit - ro).dot(rd);
                if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
            }
        };

        float p1 = s * 0.08f;
        float p2 = s * 0.28f;
        test_plane_axis(GIZMO_XY, Plane(Vector3(0, 0, 1), 0), p1, p2, p1, p2, 0, 1);
        test_plane_axis(GIZMO_XZ, Plane(Vector3(0, 1, 0), 0), p1, p2, p1, p2, 0, 2);
        test_plane_axis(GIZMO_YZ, Plane(Vector3(1, 0, 0), 0), p1, p2, p1, p2, 1, 2);
    }
    else if (gizmo_mode == GIZMO_ROTATE) {
        float ring_r = s * 0.85f;

        // === TIER 1: Sphere intersection (rotation ball) ===
        Vector3 hit_pos, hit_normal;
        float ray_len = s * ring_r * 4.0f;
        if (vfx_editor::ray_vs_sphere(ro, rd, Vector3(), s * ring_r, hit_pos, hit_normal)) {
            Vector3 local_hit = hit_pos.abs();
            int min_axis = (local_hit.x < local_hit.y) ? ((local_hit.x < local_hit.z) ? 0 : 2) : ((local_hit.y < local_hit.z) ? 1 : 2);
            if (local_hit[min_axis] < s * 0.06f) {
                best = min_axis;
            }
        }

        // === TIER 2: Ray vs ring plane (face-on rings) ===
        if (best == GIZMO_NONE) {
            for (int i = 0; i < 3; i++) {
                Vector3 normal = (i == 0) ? Vector3(1,0,0) : (i == 1) ? Vector3(0,1,0) : Vector3(0,0,1);
                Plane pl(normal, 0.0f);
                Vector3 hit;
                if (!vfx_editor::ray_vs_plane(ro, rd, pl, hit)) continue;
                float dist = hit.length();
                float tol = s * 0.08f;
                if (fabs(dist - ring_r) < tol) {
                    float t = (hit - ro).dot(rd);
                    if (t >= 0.0f && t < best_t) { best_t = t; best = i; }
                }
            }
        }

        // === TIER 3: View rotation ring (outer circle) ===
        if (best == GIZMO_NONE) {
            Vector3 cam_dir = inv.basis.xform(camera->get_global_transform().basis.get_column(2)).normalized();
            Plane view_pl(cam_dir, 0.0f);
            Vector3 hit;
            if (vfx_editor::ray_vs_plane(ro, rd, view_pl, hit)) {
                float dist = hit.length();
                float view_r = s * 1.05f;
                float tol = s * 0.06f;
                if (fabs(dist - view_r) < tol) {
                    float t = (hit - ro).dot(rd);
                    if (t >= 0.0f && t < best_t) { best_t = t; best = GIZMO_VIEW_ROTATE; }
                }
            }
        }

        // === TIER 4: Trackball (inner sphere) ===
        if (best == GIZMO_NONE) {
            Vector3 hit;
            if (vfx_editor::ray_vs_sphere(ro, rd, Vector3(), s * (ring_r - 0.08f), hit, hit_normal)) {
                float t = (hit - ro).dot(rd);
                if (t >= 0.0f && t < best_t) { best_t = t; best = GIZMO_TRACKBALL; }
            }
        }
    }
    else if (gizmo_mode == GIZMO_SCALE) {
        auto test_axis = [&](int axis, const Vector3& dir) {
            float t;
            if (vfx_editor::ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.12f, t)) {
                if (t < best_t) { best_t = t; best = axis; }
            }
        };
        test_axis(GIZMO_X, Vector3(1, 0, 0));
        test_axis(GIZMO_Y, Vector3(0, 1, 0));
        test_axis(GIZMO_Z, Vector3(0, 0, 1));

        float box = s * 0.08f;
        float h = box * 0.5f;
        auto test_box = [&](int axis, const Vector3& center) {
            for (int f = 0; f < 6; f++) {
                Vector3 n;
                switch (f) {
                    case 0: n = Vector3( 1, 0, 0); break;
                    case 1: n = Vector3(-1, 0, 0); break;
                    case 2: n = Vector3(0, 1, 0); break;
                    case 3: n = Vector3(0, -1, 0); break;
                    case 4: n = Vector3(0, 0, 1); break;
                    case 5: n = Vector3(0, 0, -1); break;
                }
                Plane pl(n, -n.dot(center));
                Vector3 hit;
                if (!vfx_editor::ray_vs_plane(ro, rd, pl, hit)) continue;
                Vector3 local = hit - center;
                if (fabs(local.x) <= h && fabs(local.y) <= h && fabs(local.z) <= h) {
                    float t = (hit - ro).dot(rd);
                    if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
                }
            }
        };
        test_box(GIZMO_XYZ, Vector3());
    }

    if (best != gizmo_hover_axis) {
        gizmo_hover_axis = best;
        _build_gizmo_mesh();
    }
    return best;
}

// ============================================================================
// GIZMO — SCREEN-SPACE RAYCAST
// ============================================================================
int VFXEditorNode::screen_raycast_gizmo(const Vector2& screen_pos) {
    if (!gizmo_node || !gizmo_node->is_visible() || !camera) return GIZMO_NONE;

    Transform3D visual = _get_visual_gizmo_transform();
    float s = gizmo_screen_scale;
    float best_score = gizmo_select_pixel_tolerance * gizmo_select_pixel_tolerance;
    int best = GIZMO_NONE;

    auto to_screen = [&](const Vector3& wp) -> Vector2 {
        return camera->unproject_position(wp);
    };

    Vector3 o = visual.get_origin();
    Vector2 o_screen = to_screen(o);

    Vector3 o_local = camera->get_global_transform().affine_inverse().xform(o);
    if (o_local.z >= 0.0f) return GIZMO_NONE;

    if (gizmo_mode == GIZMO_TRANSLATE) {
        for (int i = 0; i < 3; i++) {
            Vector3 tip = o + visual.basis.get_column(i).normalized() * s;
            Vector2 tip_screen = to_screen(tip);
            float d2 = _point_segment_dist_sq_2d(screen_pos, o_screen, tip_screen);
            if (d2 < best_score) {
                best_score = d2;
                best = i;
            }
        }

        float p1 = s * 0.18f;
        float p2 = s * 0.38f;

        auto test_plane = [&](int axis, const Vector3& local_a, const Vector3& local_b, const Vector3& local_c) {
            Vector2 sa = to_screen(o + visual.basis.xform(local_a));
            Vector2 sb = to_screen(o + visual.basis.xform(local_b));
            Vector2 sc = to_screen(o + visual.basis.xform(local_c));

            PackedVector2Array tri;
            tri.push_back(sa); tri.push_back(sb); tri.push_back(sc);

            if (_point_in_polygon_2d(screen_pos, tri)) {
                Vector2 center = (sa + sb + sc) / 3.0f;
                float d2 = screen_pos.distance_squared_to(center);
                if (d2 < best_score) {
                    best_score = d2;
                    best = axis;
                }
            }
        };

        test_plane(GIZMO_XY, Vector3(p1, p2, 0), Vector3(p2, p1, 0), Vector3(p1, p1, 0));
        test_plane(GIZMO_XZ, Vector3(p1, 0, p2), Vector3(p2, 0, p1), Vector3(p1, 0, p1));
        test_plane(GIZMO_YZ, Vector3(0, p1, p2), Vector3(0, p2, p1), Vector3(0, p1, p1));
    }
    else if (gizmo_mode == GIZMO_ROTATE) {
        float ring_r = s * 0.85f;
        int segs = 32;
        Transform3D cam_inv = camera->get_global_transform().affine_inverse();

        // === AXIS RINGS ===
        for (int i = 0; i < 3; i++) {
            Vector3 u = visual.basis.get_column((i + 1) % 3).normalized();
            Vector3 v = visual.basis.get_column((i + 2) % 3).normalized();

            float closest_d2 = 1e20f;
            int visible_segs = 0;

            for (int j = 0; j < segs; j++) {
                float ang0 = (float)j / segs * 3.14159265f * 2.0f;
                float ang1 = (float)((j + 1) % segs) / segs * 3.14159265f * 2.0f;

                Vector3 p0 = o + (u * cosf(ang0) + v * sinf(ang0)) * ring_r;
                Vector3 p1 = o + (u * cosf(ang1) + v * sinf(ang1)) * ring_r;

                Vector3 l0 = cam_inv.xform(p0);
                Vector3 l1 = cam_inv.xform(p1);
                if (l0.z >= 0.0f || l1.z >= 0.0f) continue;

                visible_segs++;
                Vector2 s0 = camera->unproject_position(p0);
                Vector2 s1 = camera->unproject_position(p1);
                float d2 = _point_segment_dist_sq_2d(screen_pos, s0, s1);
                if (d2 < closest_d2) closest_d2 = d2;
            }

            // Edge-on fallback: treat as screen-space circle
            if (visible_segs < 4) {
                Vector2 center_screen = camera->unproject_position(o);
                float dist_from_center = screen_pos.distance_to(center_screen);
                float expected_radius = (camera->unproject_position(o + u * ring_r) - center_screen).length();
                float d2 = (dist_from_center - expected_radius) * (dist_from_center - expected_radius);
                if (d2 < closest_d2) closest_d2 = d2;
            }

            if (closest_d2 < best_score) {
                best_score = closest_d2;
                best = i;
            }
        }

        // === VIEW ROTATION RING (outer) ===
        if (best == GIZMO_NONE) {
            Vector2 center_screen = camera->unproject_position(o);
            float view_r = (camera->unproject_position(o + visual.basis.get_column(0) * s * 1.05f) - center_screen).length();
            float d2 = (screen_pos.distance_to(center_screen) - view_r) * (screen_pos.distance_to(center_screen) - view_r);
            if (d2 < best_score) {
                best_score = d2;
                best = GIZMO_VIEW_ROTATE;
            }
        }

        // === TRACKBALL (inner sphere area) ===
        if (best == GIZMO_NONE) {
            float d2 = screen_pos.distance_squared_to(o_screen);
            if (d2 < best_score) {
                best_score = d2;
                best = GIZMO_TRACKBALL;
            }
        }
    }
    else if (gizmo_mode == GIZMO_SCALE) {
        for (int i = 0; i < 3; i++) {
            Vector3 tip = o + visual.basis.get_column(i).normalized() * s;
            Vector2 tip_screen = to_screen(tip);
            float d2 = _point_segment_dist_sq_2d(screen_pos, o_screen, tip_screen);
            if (d2 < best_score) {
                best_score = d2;
                best = i;
            }
        }

        float d2 = screen_pos.distance_squared_to(o_screen);
        if (d2 < best_score) {
            best_score = d2;
            best = GIZMO_XYZ;
        }
    }

    if (best != gizmo_hover_axis) {
        gizmo_hover_axis = best;
        _build_gizmo_mesh();
    }
    return best;
}

// ============================================================================
// GIZMO — DRAG BEGIN
// ============================================================================
void VFXEditorNode::gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir) {
    gizmo_drag_axis = axis;
    gizmo_drag_start_transform = gizmo_transform;
    gizmo_is_trackball = false;
    gizmo_rotation_angle = 0.0f;
    gizmo_drag_initial_vector = Vector3();

    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();
    mesh_edit_weights.clear();

    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        std::unordered_set<int> unique_verts;
        Vector3 center = gizmo_transform.get_origin();

        if (edit_mode == MODE_VERTEX) {
            for (int v : selected_vertices) {
                if (v >= 0 && v < mesh->get_vertex_count() && !mesh->get_vertices()[v]->deleted && unique_verts.insert(v).second) {
                    mesh_edit_verts.push_back(v);
                    mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v));
                    mesh_edit_weights.push_back(1.0f);
                }
            }
        } else if (edit_mode == MODE_EDGE) {
            for (int e : selected_edges) {
                int v0, v1;
                mesh->get_edge_endpoints(e, v0, v1);
                if (v0 >= 0 && !mesh->get_vertices()[v0]->deleted && unique_verts.insert(v0).second) {
                    mesh_edit_verts.push_back(v0);
                    mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v0));
                    mesh_edit_weights.push_back(1.0f);
                }
                if (v1 >= 0 && !mesh->get_vertices()[v1]->deleted && unique_verts.insert(v1).second) {
                    mesh_edit_verts.push_back(v1);
                    mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v1));
                    mesh_edit_weights.push_back(1.0f);
                }
            }
        } else if (edit_mode == MODE_FACE) {
            for (int f : selected_faces) {
                std::vector<vfx::HEVertex*> fverts;
                mesh->get_face_vertices(f, fverts);
                for (auto* v : fverts) {
                    if (!v->deleted && unique_verts.insert((int)v->id).second) {
                        mesh_edit_verts.push_back((int)v->id);
                        mesh_edit_initial_positions.push_back(v->position);
                        mesh_edit_weights.push_back(1.0f);
                    }
                }
            }
        }

        if (proportional_editing && proportional_radius > 0.001f) {
            for (auto* v : mesh->get_vertices()) {
                if (v->deleted || unique_verts.find((int)v->id) != unique_verts.end()) continue;
                float dist = (v->position - center).length();
                if (dist < proportional_radius) {
                    float t = dist / proportional_radius;
                    float w = _falloff_weight(t);
                    if (w > 0.001f) {
                        mesh_edit_verts.push_back((int)v->id);
                        mesh_edit_initial_positions.push_back(v->position);
                        mesh_edit_weights.push_back(w);
                    }
                }
            }
        }
    }

    Vector3 origin = gizmo_transform.get_origin();

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Vector3 normal;
        if (axis <= GIZMO_Z) {
            Vector3 cam_dir = ray_dir.normalized();
            Vector3 axis_vec = gizmo_transform.basis.get_column(axis).normalized();
            if (fabs(axis_vec.dot(cam_dir)) < 0.3f) {
                normal = cam_dir;
            } else {
                Vector3 alt = (fabs(axis_vec.dot(Vector3(0, 1, 0))) < 0.9f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
                normal = axis_vec.cross(alt).normalized();
                if (fabs(normal.dot(cam_dir)) < 0.1f) normal = cam_dir;
            }
        } else if (axis == GIZMO_XY) {
            normal = gizmo_transform.basis.xform(Vector3(0, 0, 1)).normalized();
        } else if (axis == GIZMO_XZ) {
            normal = gizmo_transform.basis.xform(Vector3(0, 1, 0)).normalized();
        } else {
            normal = gizmo_transform.basis.xform(Vector3(1, 0, 0)).normalized();
        }
        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (vfx_editor::ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit))
            gizmo_drag_start_point = hit;
    }
    else if (gizmo_mode == GIZMO_ROTATE) {
        gizmo_drag_plane = Plane(Vector3(), 0.0f); // unused for rotation, we use screen-space

        Vector3 normal;
        if (axis == GIZMO_X) normal = gizmo_transform.basis.xform(Vector3(1,0,0)).normalized();
        else if (axis == GIZMO_Y) normal = gizmo_transform.basis.xform(Vector3(0,1,0)).normalized();
        else if (axis == GIZMO_Z) normal = gizmo_transform.basis.xform(Vector3(0,0,1)).normalized();
        else if (axis == GIZMO_VIEW_ROTATE) normal = camera->get_global_transform().basis.get_column(2).normalized();
        else normal = Vector3(0, 1, 0);

        // For axis rotation: intersect with the ring plane to get initial vector
        if (axis <= GIZMO_Z || axis == GIZMO_VIEW_ROTATE) {
            Plane ring_plane(normal, origin);
            Vector3 hit;
            if (vfx_editor::ray_vs_plane(ray_origin, ray_dir, ring_plane, hit)) {
                gizmo_drag_start_point = hit;
                gizmo_drag_initial_vector = (hit - origin).normalized();
                if (gizmo_drag_initial_vector.length_squared() < 0.0001f) {
                    // Fallback: pick a perpendicular vector
                    Vector3 perp = (fabs(normal.dot(Vector3(0,1,0))) < 0.9f) ? Vector3(0,1,0) : Vector3(1,0,0);
                    gizmo_drag_initial_vector = normal.cross(perp).normalized();
                }
            }
        }
        else if (axis == GIZMO_TRACKBALL) {
            // Trackball: use camera-facing plane
            Vector3 cam_normal = camera->get_global_transform().basis.get_column(2).normalized();
            gizmo_drag_plane = Plane(cam_normal, origin);
            Vector3 hit;
            if (vfx_editor::ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
                gizmo_drag_start_point = hit;
                gizmo_drag_initial_vector = (hit - origin).normalized();
            }
            gizmo_is_trackball = true;
        }

        gizmo_drag_start_rotation = gizmo_transform.basis.get_rotation_quaternion();
    }
    else if (gizmo_mode == GIZMO_SCALE) {
        Vector3 cam_dir = ray_dir.normalized();
        Vector3 normal = cam_dir;
        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (vfx_editor::ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
            gizmo_drag_start_point = hit;
            gizmo_drag_start_scale = gizmo_transform.basis.get_scale();
            if (axis <= GIZMO_Z) {
                Vector3 world_axis = gizmo_transform.basis.get_column(axis).normalized();
                gizmo_drag_start_point = Vector3((hit - origin).dot(world_axis), 0, 0);
            } else {
                gizmo_drag_start_point = Vector3((hit - origin).length(), 0, 0);
            }
        }
    }
}

// ============================================================================
// GIZMO — DRAG UPDATE
// ============================================================================
void VFXEditorNode::gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (gizmo_drag_axis == GIZMO_NONE) return;
    Vector3 hit;
    if (!vfx_editor::ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) return;

    Vector3 origin = gizmo_transform.get_origin();

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Vector3 delta = hit - gizmo_drag_start_point;
        Transform3D t = gizmo_drag_start_transform;
        if (gizmo_drag_axis <= GIZMO_Z) {
            Vector3 axis = t.basis.get_column(gizmo_drag_axis).normalized();
            float proj = delta.dot(axis);
            t.set_origin(t.get_origin() + axis * proj);
        } else if (gizmo_drag_axis == GIZMO_XY || gizmo_drag_axis == GIZMO_XZ || gizmo_drag_axis == GIZMO_YZ) {
            t.set_origin(t.get_origin() + delta);
        }
        gizmo_transform = t;
    }
    else if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 normal;
        if (gizmo_drag_axis == GIZMO_X) normal = gizmo_transform.basis.xform(Vector3(1,0,0)).normalized();
        else if (gizmo_drag_axis == GIZMO_Y) normal = gizmo_transform.basis.xform(Vector3(0,1,0)).normalized();
        else if (gizmo_drag_axis == GIZMO_Z) normal = gizmo_transform.basis.xform(Vector3(0,0,1)).normalized();
        else if (gizmo_drag_axis == GIZMO_VIEW_ROTATE) normal = camera->get_global_transform().basis.get_column(2).normalized();
        else normal = Vector3(0, 1, 0);

        if (gizmo_is_trackball || gizmo_drag_axis == GIZMO_TRACKBALL) {
            // === TRACKBALL ROTATION ===
            Vector3 current_vec = (hit - origin).normalized();
            if (current_vec.length_squared() < 0.0001f) return;
            if (gizmo_drag_initial_vector.length_squared() < 0.0001f) return;

            float dot = gizmo_drag_initial_vector.dot(current_vec);
            dot = vfx::clampf(dot, -1.0f, 1.0f);
            float angle = acosf(dot);
            if (angle < 0.0001f) return;

            Vector3 rot_axis = gizmo_drag_initial_vector.cross(current_vec).normalized();
            if (rot_axis.length_squared() < 0.0001f) return;

            Quaternion rot(rot_axis, angle);
            Basis new_basis = Basis(rot) * gizmo_drag_start_transform.basis;
            gizmo_transform.set_basis(new_basis);
            gizmo_rotation_angle = angle;
        }
        else {
            // === AXIS / VIEW ROTATION (tangent-space signed angle) ===
            Plane ring_plane(normal, origin);
            Vector3 current_hit;
            if (!vfx_editor::ray_vs_plane(ray_origin, ray_dir, ring_plane, current_hit)) return;

            Vector3 current_vec = (current_hit - origin).normalized();
            if (current_vec.length_squared() < 0.0001f) return;
            if (gizmo_drag_initial_vector.length_squared() < 0.0001f) return;

            // Project both vectors onto the ring plane
            Vector3 v0 = gizmo_drag_initial_vector - normal * normal.dot(gizmo_drag_initial_vector);
            Vector3 v1 = current_vec - normal * normal.dot(current_vec);
            if (v0.length_squared() < 0.0001f || v1.length_squared() < 0.0001f) return;
            v0.normalize();
            v1.normalize();

            float dot = v0.dot(v1);
            dot = vfx::clampf(dot, -1.0f, 1.0f);
            float angle = acosf(dot);

            Vector3 cross = v0.cross(v1);
            if (cross.dot(normal) < 0.0f) angle = -angle;

            Vector3 axis_local;
            if (gizmo_drag_axis == GIZMO_X) axis_local = Vector3(1,0,0);
            else if (gizmo_drag_axis == GIZMO_Y) axis_local = Vector3(0,1,0);
            else if (gizmo_drag_axis == GIZMO_Z) axis_local = Vector3(0,0,1);
            else axis_local = Vector3(0,0,1); // view rotation uses Z in view space

            Quaternion rot(axis_local, angle);
            Basis new_basis = Basis(rot) * gizmo_drag_start_transform.basis;
            gizmo_transform.set_basis(new_basis);
            gizmo_rotation_angle = angle;
        }
    }
    else if (gizmo_mode == GIZMO_SCALE) {
        Vector3 scale = gizmo_drag_start_scale;
        if (gizmo_drag_axis <= GIZMO_Z) {
            Vector3 world_axis = gizmo_drag_start_transform.basis.get_column(gizmo_drag_axis).normalized();
            float current_proj = (hit - origin).dot(world_axis);
            float start_proj = gizmo_drag_start_point.x;
            if (fabs(start_proj) > 0.0001f) {
                float ratio = current_proj / start_proj;
                ratio = vfx::clampf(ratio, 0.001f, 1000.0f);
                scale[gizmo_drag_axis] = gizmo_drag_start_scale[gizmo_drag_axis] * ratio;
            }
        } else if (gizmo_drag_axis == GIZMO_XYZ) {
            float current_dist = (hit - origin).length();
            float start_dist = gizmo_drag_start_point.x;
            if (start_dist > 0.0001f) {
                float ratio = current_dist / start_dist;
                ratio = vfx::clampf(ratio, 0.001f, 1000.0f);
                scale = gizmo_drag_start_scale * ratio;
            }
        }
        scale.x = vfx::clampf(scale.x, 0.001f, 1000.0f);
        scale.y = vfx::clampf(scale.y, 0.001f, 1000.0f);
        scale.z = vfx::clampf(scale.z, 0.001f, 1000.0f);
        Basis b = gizmo_drag_start_transform.basis;
        b.set_column(0, b.get_column(0).normalized() * scale.x);
        b.set_column(1, b.get_column(1).normalized() * scale.y);
        b.set_column(2, b.get_column(2).normalized() * scale.z);
        gizmo_transform.set_basis(b);
    }

    if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());

    if (edit_mode != MODE_OBJECT && !mesh_edit_verts.empty() && mesh.is_valid()) {
        Transform3D delta = gizmo_transform * gizmo_drag_start_transform.affine_inverse();
        for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
            Vector3 fully_transformed = delta.xform(mesh_edit_initial_positions[i]);
            Vector3 offset = fully_transformed - mesh_edit_initial_positions[i];
            Vector3 new_pos = mesh_edit_initial_positions[i] + offset * mesh_edit_weights[i];
            mesh->set_vertex_position(mesh_edit_verts[i], new_pos);
        }
        if (auto_update) _update_godot_mesh();
        _build_selection_mesh();
        _update_gizmo_for_selection();
        return;
    }

    if (edit_mode == MODE_OBJECT && active_scene_node.is_valid()) {
        active_scene_node->set_local_transform(gizmo_transform);
        mark_scene_dirty();
        return;
    }

    if (selected_bone >= 0 && skeleton.is_valid()) {
        int parent = skeleton->get_bone_parent(selected_bone);
        Transform3D parent_world = (parent >= 0) ? skeleton->get_bone_model_transform(parent) : Transform3D();
        Transform3D local = parent_world.affine_inverse() * gizmo_transform;
        skeleton->set_bone_pose(selected_bone, local);

        if (symmetry_enabled) {
            int sym_bone = skeleton->get_symmetric_bone(selected_bone);
            if (sym_bone >= 0) {
                Transform3D mirrored_local = local;
                Vector3 pos = mirrored_local.get_origin();
                if (symmetry_axis == 0) pos.x = -pos.x;
                else if (symmetry_axis == 1) pos.y = -pos.y;
                else if (symmetry_axis == 2) pos.z = -pos.z;
                mirrored_local.set_origin(pos);

                if (symmetry_axis == 0) {
                    Basis b = mirrored_local.get_basis();
                    Vector3 euler = b.get_euler();
                    euler.x = -euler.x;
                    euler.z = -euler.z;
                    mirrored_local.set_basis(Basis::from_euler(euler));
                }

                skeleton->set_bone_pose(sym_bone, mirrored_local);
            }
        }

        skeleton->update_transforms();
        gizmo_transform = skeleton->get_bone_model_transform(selected_bone);
        if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
        _build_skeleton_mesh();

        Vector3 bone_pos = skeleton->get_bone_model_transform(selected_bone).get_origin();
        UtilityFunctions::print("Bone ", selected_bone, " pos: ", bone_pos);
        _update_godot_mesh();
    }
}

void VFXEditorNode::gizmo_end_drag() {
    gizmo_drag_axis = GIZMO_NONE;
    gizmo_is_trackball = false;
    gizmo_rotation_angle = 0.0f;
}

bool VFXEditorNode::is_gizmo_dragging() const {
    return gizmo_drag_axis != GIZMO_NONE;
}

// ============================================================================
// GIZMO — MESH BUILDING (Godot-style visuals)
// ============================================================================
void VFXEditorNode::_build_gizmo_mesh() {
    if (!gizmo_node) _ensure_gizmo_node();

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    float s = gizmo_screen_scale;

    // Godot-style colors
    Color RED    = Color(0.96f, 0.20f, 0.20f, 1.0f);
    Color GREEN  = Color(0.20f, 0.96f, 0.20f, 1.0f);
    Color BLUE   = Color(0.20f, 0.40f, 0.96f, 1.0f);
    Color YELLOW = Color(1.0f, 1.0f, 0.0f, 1.0f);
    Color WHITE  = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color GRAY   = Color(0.7f, 0.7f, 0.7f, 1.0f);

    if (gizmo_mode == GIZMO_TRANSLATE) {
        float shaft_r = s * 0.015f;
        float cone_h = s * 0.12f;
        float cone_r = s * 0.045f;

        Color cx = (gizmo_hover_axis == GIZMO_X) ? YELLOW : RED;
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? YELLOW : GREEN;
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? YELLOW : BLUE;

        // Shafts
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(s * 0.88f, 0, 0), shaft_r, 8, cx);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s * 0.88f, 0), shaft_r, 8, cy);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s * 0.88f), shaft_r, 8, cz);

        // Arrowheads (cones)
        append_cone(verts, cols, idx, Vector3(s, 0, 0), Vector3(1, 0, 0), cone_h, cone_r, 8, cx);
        append_cone(verts, cols, idx, Vector3(0, s, 0), Vector3(0, 1, 0), cone_h, cone_r, 8, cy);
        append_cone(verts, cols, idx, Vector3(0, 0, s), Vector3(0, 0, 1), cone_h, cone_r, 8, cz);

        // Plane handles
        float p1 = s * 0.12f;
        float p2 = s * 0.32f;

        Color cxy = (gizmo_hover_axis == GIZMO_XY) ? Color(YELLOW.r, YELLOW.g, YELLOW.b, 0.9f) : Color(RED.r + GREEN.r, RED.g + GREEN.g, RED.b + GREEN.b, 0.35f) * 0.5f;
        Color cxz = (gizmo_hover_axis == GIZMO_XZ) ? Color(YELLOW.r, YELLOW.g, YELLOW.b, 0.9f) : Color(RED.r + BLUE.r, RED.g + BLUE.g, RED.b + BLUE.b, 0.35f) * 0.5f;
        Color cyz = (gizmo_hover_axis == GIZMO_YZ) ? Color(YELLOW.r, YELLOW.g, YELLOW.b, 0.9f) : Color(GREEN.r + BLUE.r, GREEN.g + BLUE.g, GREEN.b + BLUE.b, 0.35f) * 0.5f;

        vfx_editor::append_triangle(verts, cols, idx, Vector3(p1, p2, 0), Vector3(p2, p1, 0), Vector3(p1, p1, 0), cxy);
        vfx_editor::append_triangle(verts, cols, idx, Vector3(p1, 0, p2), Vector3(p2, 0, p1), Vector3(p1, 0, p1), cxz);
        vfx_editor::append_triangle(verts, cols, idx, Vector3(0, p1, p2), Vector3(0, p2, p1), Vector3(0, p1, p1), cyz);
    }
    else if (gizmo_mode == GIZMO_ROTATE) {
        float ring_r = s * 0.85f;
        float tube = s * 0.018f;

        Color cx = (gizmo_hover_axis == GIZMO_X) ? YELLOW : RED;
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? YELLOW : GREEN;
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? YELLOW : BLUE;
        Color cview = (gizmo_hover_axis == GIZMO_VIEW_ROTATE) ? YELLOW : GRAY;
        Color ctrack = (gizmo_hover_axis == GIZMO_TRACKBALL) ? YELLOW : Color(WHITE.r, WHITE.g, WHITE.b, 0.3f);

        // Axis rings
        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(1,0,0), ring_r, 32, tube, cx);
        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(0,1,0), ring_r, 32, tube, cy);
        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(0,0,1), ring_r, 32, tube, cz);

        // View rotation ring (outer, always perpendicular to camera)
        if (camera) {
            Vector3 cam_dir = _get_visual_gizmo_transform().basis.xform_inv(
                camera->get_global_transform().basis.get_column(2)).normalized();
            vfx_editor::append_ring(verts, cols, idx, Vector3(), cam_dir, s * 1.05f, 48, tube * 0.7f, cview);
        }

        // Trackball indicator (small inner sphere wireframe)
        float track_r = s * 0.08f;
        int track_segs = 16;
        Color track_col = ctrack;
        auto add_track_ring = [&](const Vector3& axis) {
            Vector3 up = fabs(axis.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            Vector3 right = axis.cross(up).normalized();
            up = right.cross(axis).normalized();
            int base = verts.size();
            for (int i = 0; i <= track_segs; i++) {
                float ang = (float)i / track_segs * 3.14159265f * 2.0f;
                Vector3 p = right * cosf(ang) * track_r + up * sinf(ang) * track_r;
                verts.push_back(p); cols.push_back(track_col);
            }
            for (int i = 0; i < track_segs; i++) {
                idx.push_back(base + i);
                idx.push_back(base + i + 1);
            }
        };
        add_track_ring(Vector3(1, 0, 0));
        add_track_ring(Vector3(0, 1, 0));
        add_track_ring(Vector3(0, 0, 1));
    }
    else if (gizmo_mode == GIZMO_SCALE) {
        float shaft_r = s * 0.015f;
        float box_s = s * 0.07f;

        Color cx = (gizmo_hover_axis == GIZMO_X) ? YELLOW : RED;
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? YELLOW : GREEN;
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? YELLOW : BLUE;
        Color cxyz = (gizmo_hover_axis == GIZMO_XYZ) ? YELLOW : WHITE;

        // Shafts
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(s, 0, 0), shaft_r, 8, cx);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s, 0), shaft_r, 8, cy);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s), shaft_r, 8, cz);

        // Boxes at tips
        vfx_editor::append_box(verts, cols, idx, Vector3(s, 0, 0), box_s, cx);
        vfx_editor::append_box(verts, cols, idx, Vector3(0, s, 0), box_s, cy);
        vfx_editor::append_box(verts, cols, idx, Vector3(0, 0, s), box_s, cz);
        vfx_editor::append_box(verts, cols, idx, Vector3(), box_s * 0.8f, cxyz);
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    gizmo_node->set_material_override(mat);
    gizmo_node->set_mesh(am);
    gizmo_node->set_transform(_get_visual_gizmo_transform());
}


// ============================================================================
// PROPORTIONAL EDITING — FALLOFF & CURSOR
// ============================================================================

float VFXEditorNode::_falloff_weight(float t) const {
    t = vfx::clampf(t, 0.0f, 1.0f);
    switch (falloff_mode) {
        case FALLOFF_SMOOTH:
            return 1.0f - t * t * (3.0f - 2.0f * t);
        case FALLOFF_SPHERE:
            return sqrtf(vfx::clampf(1.0f - t * t, 0.0f, 1.0f));
        case FALLOFF_ROOT:
            return 1.0f - sqrtf(t);
        case FALLOFF_INVERSE_SQUARE:
            return (1.0f - t) * (1.0f - t);
        case FALLOFF_SHARP:
            return 1.0f - t * t;
        case FALLOFF_LINEAR:
            return 1.0f - t;
        case FALLOFF_CONSTANT:
            return 1.0f;
        case FALLOFF_RANDOM: {
            float r = (float)rand() / (float)RAND_MAX;
            return r * (1.0f - t);
        }
        default:
            return 1.0f - t;
    }
}

void VFXEditorNode::_ensure_proportional_cursor() {
    if (!proportional_cursor) {
        proportional_cursor = memnew(MeshInstance3D);
        proportional_cursor->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        proportional_cursor->set_visible(false);
        add_child(proportional_cursor);
        proportional_cursor->set_owner(this);
    }
}

void VFXEditorNode::_update_proportional_cursor() {
    _ensure_proportional_cursor();
    if (!proportional_cursor) return;

    bool show = proportional_editing && edit_mode != MODE_OBJECT && (selected_vertex >= 0 || selected_edge >= 0 || selected_face >= 0);
    if (!show) {
        proportional_cursor->set_visible(false);
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();
    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    int rings = 16;
    int segs = 32;
    float r = proportional_radius;
    Color col(0.2f, 0.8f, 1.0f, 0.5f);

    auto add_ring = [&](const Vector3& axis, float radius) {
        Vector3 up = fabs(axis.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
        Vector3 right = axis.cross(up).normalized();
        up = right.cross(axis).normalized();

        int base = verts.size();
        for (int i = 0; i <= segs; i++) {
            float ang = (float)i / segs * 3.14159265f * 2.0f;
            Vector3 p = right * cosf(ang) * radius + up * sinf(ang) * radius;
            verts.push_back(p);
            cols.push_back(col);
        }
        for (int i = 0; i < segs; i++) {
            idx.push_back(base + i);
            idx.push_back(base + i + 1);
        }
    };

    add_ring(Vector3(1, 0, 0), r);
    add_ring(Vector3(0, 1, 0), r);
    add_ring(Vector3(0, 0, 1), r);

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    proportional_cursor->set_material_override(mat);
    proportional_cursor->set_mesh(am);

    Vector3 center = gizmo_transform.get_origin();
    proportional_cursor->set_position(center);
    proportional_cursor->set_visible(true);
}
