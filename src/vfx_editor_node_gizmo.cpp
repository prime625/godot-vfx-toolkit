#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include "vfx_math.h"
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

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
            if (vfx_editor::ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.18f, t)) {
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
    } else if (gizmo_mode == GIZMO_ROTATE) {
        auto test_ring = [&](int axis, const Vector3& normal, float radius) {
            Plane pl(normal, 0.0f);
            Vector3 hit;
            if (!vfx_editor::ray_vs_plane(ro, rd, pl, hit)) return;
            float dist = hit.length();
            float tol = s * 0.12f;
            if (fabs(dist - radius) < tol) {
                float t = (hit - ro).dot(rd);
                if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
            }
        };
        float ring_r = s * 0.85f;
        test_ring(GIZMO_X, Vector3(1,0,0), ring_r);
        test_ring(GIZMO_Y, Vector3(0,1,0), ring_r);
        test_ring(GIZMO_Z, Vector3(0,0,1), ring_r);
    } else if (gizmo_mode == GIZMO_SCALE) {
        auto test_axis = [&](int axis, const Vector3& dir) {
            float t;
            if (vfx_editor::ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.18f, t)) {
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
    float tol = select_pixel_tolerance;
    float best_score = tol * tol;
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
            tri.push_back(sa);
            tri.push_back(sb);
            tri.push_back(sc);

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

        for (int i = 0; i < 3; i++) {
            Vector3 u = visual.basis.get_column((i + 1) % 3).normalized();
            Vector3 v = visual.basis.get_column((i + 2) % 3).normalized();

            float closest_d2 = 1e20f;
            for (int j = 0; j < segs; j++) {
                float ang0 = (float)j / segs * 3.14159265f * 2.0f;
                float ang1 = (float)((j + 1) % segs) / segs * 3.14159265f * 2.0f;

                Vector3 p0 = o + (u * cosf(ang0) + v * sinf(ang0)) * ring_r;
                Vector3 p1 = o + (u * cosf(ang1) + v * sinf(ang1)) * ring_r;

                Vector3 l0 = cam_inv.xform(p0);
                Vector3 l1 = cam_inv.xform(p1);
                if (l0.z >= 0.0f || l1.z >= 0.0f) continue;

                Vector2 s0 = camera->unproject_position(p0);
                Vector2 s1 = camera->unproject_position(p1);
                float d2 = _point_segment_dist_sq_2d(screen_pos, s0, s1);
                if (d2 < closest_d2) closest_d2 = d2;
            }

            if (closest_d2 < best_score) {
                best_score = closest_d2;
                best = i;
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

    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();
    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
            mesh_edit_verts.push_back(selected_vertex);
            mesh_edit_initial_positions.push_back(mesh->get_vertex_position(selected_vertex));
        } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
            int v0, v1;
            mesh->get_edge_endpoints(selected_edge, v0, v1);
            if (v0 >= 0) { mesh_edit_verts.push_back(v0); mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v0)); }
            if (v1 >= 0) { mesh_edit_verts.push_back(v1); mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v1)); }
        } else if (edit_mode == MODE_FACE && selected_face >= 0) {
            std::vector<vfx::HEVertex*> verts;
            mesh->get_face_vertices(selected_face, verts);
            for (auto* v : verts) {
                mesh_edit_verts.push_back((int)v->id);
                mesh_edit_initial_positions.push_back(v->position);
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
    } else if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 normal;
        if (axis == GIZMO_X) normal = gizmo_transform.basis.xform(Vector3(1,0,0)).normalized();
        else if (axis == GIZMO_Y) normal = gizmo_transform.basis.xform(Vector3(0,1,0)).normalized();
        else normal = gizmo_transform.basis.xform(Vector3(0,0,1)).normalized();

        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (vfx_editor::ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
            gizmo_drag_start_point = hit;
            gizmo_drag_start_rotation = gizmo_transform.basis.get_rotation_quaternion();
        }
    } else if (gizmo_mode == GIZMO_SCALE) {
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
    if (!gizmo_dragging || gizmo_drag_axis < 0) return;

    float t = _ray_plane_intersect(ray_origin, ray_dir, gizmo_drag_plane_normal, gizmo_drag_plane_d);
    if (t < 0.0f) return;
    Vector3 hit = ray_origin + ray_dir * t;

    // --- OBJECT / BONE MODE ---
    if (edit_mode == MODE_OBJECT) {
        if (active_scene_node.is_valid()) {
            if (gizmo_mode == GIZMO_TRANSLATE) {
                Vector3 delta = hit - gizmo_drag_start_point;
                Vector3 mask;
                switch (gizmo_drag_axis) {
                    case GIZMO_X:  mask = Vector3(1, 0, 0); break;
                    case GIZMO_Y:  mask = Vector3(0, 1, 0); break;
                    case GIZMO_Z:  mask = Vector3(0, 0, 1); break;
                    case GIZMO_XY: mask = Vector3(1, 1, 0); break;
                    case GIZMO_XZ: mask = Vector3(1, 0, 1); break;
                    case GIZMO_YZ: mask = Vector3(0, 1, 1); break;
                    case GIZMO_XYZ: mask = Vector3(1, 1, 1); break;
                    default: mask = Vector3(1, 0, 0); break;
                }
                gizmo_transform.origin = gizmo_drag_start_transform.origin + delta * mask;
                active_scene_node->set_local_position(gizmo_transform.origin);
            } else if (gizmo_mode == GIZMO_ROTATE) {
                Vector3 axis;
                switch (gizmo_drag_axis) {
                    case GIZMO_X: axis = Vector3(1, 0, 0); break;
                    case GIZMO_Y: axis = Vector3(0, 1, 0); break;
                    case GIZMO_Z: axis = Vector3(0, 0, 1); break;
                    default: axis = Vector3(0, 1, 0); break;
                }
                Vector3 local_hit = gizmo_drag_start_transform.affine_inverse().xform(hit);
                Vector3 local_start = gizmo_drag_start_transform.affine_inverse().xform(gizmo_drag_start_point);
                float angle = atan2(local_hit.x - local_start.x, local_hit.y - local_start.y);
                Quaternion delta_rot(axis, angle);
                gizmo_transform.basis = Basis(delta_rot * gizmo_drag_start_rotation);
                active_scene_node->set_local_rotation(Quaternion(gizmo_transform.basis.get_rotation_quaternion()));
            } else if (gizmo_mode == GIZMO_SCALE) {
                Vector3 delta = hit - gizmo_drag_start_point;
                float s = 1.0f + delta.length() * (delta.dot(gizmo_drag_plane_normal) > 0 ? 1.0f : -1.0f);
                s = MAX(s, 0.01f);
                Vector3 mask;
                switch (gizmo_drag_axis) {
                    case GIZMO_X:  mask = Vector3(s, 1, 1); break;
                    case GIZMO_Y:  mask = Vector3(1, s, 1); break;
                    case GIZMO_Z:  mask = Vector3(1, 1, s); break;
                    case GIZMO_XY: mask = Vector3(s, s, 1); break;
                    case GIZMO_XZ: mask = Vector3(s, 1, s); break;
                    case GIZMO_YZ: mask = Vector3(1, s, s); break;
                    case GIZMO_XYZ: mask = Vector3(s, s, s); break;
                    default: mask = Vector3(1, 1, 1); break;
                }
                gizmo_transform.basis = gizmo_drag_start_transform.basis;
                gizmo_transform.basis.scale(mask);
                active_scene_node->set_local_scale(gizmo_transform.basis.get_scale());
            }
            if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
            mark_scene_dirty();
            return;
        }

        if (selected_bone >= 0 && skeleton.is_valid()) {
            // Use set_bone_local_transform (the actual API)
            if (gizmo_mode == GIZMO_TRANSLATE) {
                Vector3 delta = hit - gizmo_drag_start_point;
                Vector3 mask;
                switch (gizmo_drag_axis) {
                    case GIZMO_X:  mask = Vector3(1, 0, 0); break;
                    case GIZMO_Y:  mask = Vector3(0, 1, 0); break;
                    case GIZMO_Z:  mask = Vector3(0, 0, 1); break;
                    case GIZMO_XY: mask = Vector3(1, 1, 0); break;
                    case GIZMO_XZ: mask = Vector3(1, 0, 1); break;
                    case GIZMO_YZ: mask = Vector3(0, 1, 1); break;
                    case GIZMO_XYZ: mask = Vector3(1, 1, 1); break;
                    default: mask = Vector3(1, 0, 0); break;
                }
                gizmo_transform.origin = gizmo_drag_start_transform.origin + delta * mask;
            } else if (gizmo_mode == GIZMO_ROTATE) {
                Vector3 axis;
                switch (gizmo_drag_axis) {
                    case GIZMO_X: axis = Vector3(1, 0, 0); break;
                    case GIZMO_Y: axis = Vector3(0, 1, 0); break;
                    case GIZMO_Z: axis = Vector3(0, 0, 1); break;
                    default: axis = Vector3(0, 1, 0); break;
                }
                Vector3 local_hit = gizmo_drag_start_transform.affine_inverse().xform(hit);
                Vector3 local_start = gizmo_drag_start_transform.affine_inverse().xform(gizmo_drag_start_point);
                float angle = atan2(local_hit.x - local_start.x, local_hit.y - local_start.y);
                Quaternion delta_rot(axis, angle);
                gizmo_transform.basis = Basis(delta_rot * gizmo_drag_start_rotation);
            } else if (gizmo_mode == GIZMO_SCALE) {
                Vector3 delta = hit - gizmo_drag_start_point;
                float s = 1.0f + delta.length() * (delta.dot(gizmo_drag_plane_normal) > 0 ? 1.0f : -1.0f);
                s = MAX(s, 0.01f);
                Vector3 mask;
                switch (gizmo_drag_axis) {
                    case GIZMO_X:  mask = Vector3(s, 1, 1); break;
                    case GIZMO_Y:  mask = Vector3(1, s, 1); break;
                    case GIZMO_Z:  mask = Vector3(1, 1, s); break;
                    case GIZMO_XY: mask = Vector3(s, s, 1); break;
                    case GIZMO_XZ: mask = Vector3(s, 1, s); break;
                    case GIZMO_YZ: mask = Vector3(1, s, s); break;
                    case GIZMO_XYZ: mask = Vector3(s, s, s); break;
                    default: mask = Vector3(1, 1, 1); break;
                }
                gizmo_transform.basis = gizmo_drag_start_transform.basis;
                gizmo_transform.basis.scale(mask);
            }
            skeleton->set_bone_pose(selected_bone, gizmo_transform);
            if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
            _build_skeleton_mesh();
            if (auto_update) _update_godot_mesh();
            return;
        }
        return;
    }

    // --- MESH EDIT MODE with PROPORTIONAL EDITING ---
    auto _apply_proportional = [&](const Transform3D& t, const Vector3& center) {
        std::unordered_set<int> selected_set;
        for (int vi : mesh_edit_verts) selected_set.insert(vi);

        for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
            int vi = mesh_edit_verts[i];
            mesh->set_vertex_position(vi, t.xform(mesh_edit_initial_positions[i]));
        }

        if (proportional_enabled && mesh.is_valid()) {
            int vc = mesh->get_vertex_count();
            for (int vi = 0; vi < vc; vi++) {
                if (selected_set.find(vi) != selected_set.end()) continue;
                Vector3 vpos = mesh->get_vertex_position(vi);
                float dist = (vpos - center).length();
                if (dist < proportional_radius) {
                    float falloff = vfx::evaluate_falloff(dist / proportional_radius, proportional_falloff);
                    mesh->set_vertex_position(vi, vpos.lerp(t.xform(vpos), falloff));
                }
            }
        }
    };

    if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        if (gizmo_mode == GIZMO_TRANSLATE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            Transform3D t;
            t.origin = delta;
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_ROTATE) {
            Vector3 axis;
            switch (gizmo_drag_axis) {
                case GIZMO_X: axis = Vector3(1, 0, 0); break;
                case GIZMO_Y: axis = Vector3(0, 1, 0); break;
                case GIZMO_Z: axis = Vector3(0, 0, 1); break;
                default: axis = Vector3(0, 1, 0); break;
            }
            Vector3 local_hit = gizmo_drag_start_transform.affine_inverse().xform(hit);
            Vector3 local_start = gizmo_drag_start_transform.affine_inverse().xform(gizmo_drag_start_point);
            float angle = atan2(local_hit.x - local_start.x, local_hit.y - local_start.y);
            Quaternion delta_rot(axis, angle);
            Transform3D t;
            t.basis = Basis(delta_rot * gizmo_drag_start_rotation);
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_SCALE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            float s = 1.0f + delta.length() * (delta.dot(gizmo_drag_plane_normal) > 0 ? 1.0f : -1.0f);
            s = MAX(s, 0.01f);
            Transform3D t;
            t.basis = Basis().scaled(Vector3(s, s, s));
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        }
        mesh->recalculate_normals();
        if (auto_update) _update_godot_mesh();
        return;
    }

    if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        if (gizmo_mode == GIZMO_TRANSLATE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            Transform3D t; t.origin = delta;
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_ROTATE) {
            Vector3 axis;
            switch (gizmo_drag_axis) {
                case GIZMO_X: axis = Vector3(1, 0, 0); break;
                case GIZMO_Y: axis = Vector3(0, 1, 0); break;
                case GIZMO_Z: axis = Vector3(0, 0, 1); break;
                default: axis = Vector3(0, 1, 0); break;
            }
            Vector3 local_hit = gizmo_drag_start_transform.affine_inverse().xform(hit);
            Vector3 local_start = gizmo_drag_start_transform.affine_inverse().xform(gizmo_drag_start_point);
            float angle = atan2(local_hit.x - local_start.x, local_hit.y - local_start.y);
            Quaternion delta_rot(axis, angle);
            Transform3D t;
            t.basis = Basis(delta_rot * gizmo_drag_start_rotation);
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_SCALE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            float s = 1.0f + delta.length() * (delta.dot(gizmo_drag_plane_normal) > 0 ? 1.0f : -1.0f);
            s = MAX(s, 0.01f);
            Transform3D t;
            t.basis = Basis().scaled(Vector3(s, s, s));
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        }
        mesh->recalculate_normals();
        if (auto_update) _update_godot_mesh();
        return;
    }

    if (edit_mode == MODE_FACE && selected_face >= 0) {
        if (gizmo_mode == GIZMO_TRANSLATE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            Transform3D t; t.origin = delta;
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_ROTATE) {
            Vector3 axis;
            switch (gizmo_drag_axis) {
                case GIZMO_X: axis = Vector3(1, 0, 0); break;
                case GIZMO_Y: axis = Vector3(0, 1, 0); break;
                case GIZMO_Z: axis = Vector3(0, 0, 1); break;
                default: axis = Vector3(0, 1, 0); break;
            }
            Vector3 local_hit = gizmo_drag_start_transform.affine_inverse().xform(hit);
            Vector3 local_start = gizmo_drag_start_transform.affine_inverse().xform(gizmo_drag_start_point);
            float angle = atan2(local_hit.x - local_start.x, local_hit.y - local_start.y);
            Quaternion delta_rot(axis, angle);
            Transform3D t;
            t.basis = Basis(delta_rot * gizmo_drag_start_rotation);
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        } else if (gizmo_mode == GIZMO_SCALE) {
            Vector3 delta = hit - gizmo_drag_start_point;
            float s = 1.0f + delta.length() * (delta.dot(gizmo_drag_plane_normal) > 0 ? 1.0f : -1.0f);
            s = MAX(s, 0.01f);
            Transform3D t;
            t.basis = Basis().scaled(Vector3(s, s, s));
            t.origin = gizmo_transform.origin - t.basis.xform(gizmo_transform.origin);
            _apply_proportional(t, gizmo_transform.origin);
        }
        mesh->recalculate_normals();
        if (auto_update) _update_godot_mesh();
        return;
    }
}

void VFXEditorNode::gizmo_end_drag() {
    gizmo_drag_axis = GIZMO_NONE;
}

bool VFXEditorNode::is_gizmo_dragging() const {
    return gizmo_drag_axis != GIZMO_NONE;
}


void VFXEditorNode::_build_gizmo_mesh() {
    if (!gizmo_node) _ensure_gizmo_node();

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    float s = gizmo_screen_scale;
    float r = s * 0.025f;

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);

        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(s, 0, 0), r, 8, cx);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s, 0), r, 8, cy);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s), r, 8, cz);

        float p1 = s * 0.18f;
        float p2 = s * 0.38f;

        Color cxy = (gizmo_hover_axis == GIZMO_XY) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(1.0f, 1.0f, 0.0f, 0.35f);
        vfx_editor::append_triangle(verts, cols, idx, Vector3(p1, p2, 0), Vector3(p2, p1, 0), Vector3(p1, p1, 0), cxy);

        Color cxz = (gizmo_hover_axis == GIZMO_XZ) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(1.0f, 0.0f, 1.0f, 0.35f);
        vfx_editor::append_triangle(verts, cols, idx, Vector3(p1, 0, p2), Vector3(p2, 0, p1), Vector3(p1, 0, p1), cxz);

        Color cyz = (gizmo_hover_axis == GIZMO_YZ) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(0.0f, 1.0f, 1.0f, 0.35f);
        vfx_editor::append_triangle(verts, cols, idx, Vector3(0, p1, p2), Vector3(0, p2, p1), Vector3(0, p1, p1), cyz);
    } else if (gizmo_mode == GIZMO_ROTATE) {
        float ring_r = s * 0.85f;
        float tube = s * 0.02f;

        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);

        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(1,0,0), ring_r, 32, tube, cx);
        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(0,1,0), ring_r, 32, tube, cy);
        vfx_editor::append_ring(verts, cols, idx, Vector3(), Vector3(0,0,1), ring_r, 32, tube, cz);
    } else if (gizmo_mode == GIZMO_SCALE) {
        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);
        Color cxyz = (gizmo_hover_axis == GIZMO_XYZ) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 1.0f, 1.0f, 0.8f);

        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(s, 0, 0), r, 8, cx);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s, 0), r, 8, cy);
        vfx_editor::append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s), r, 8, cz);

        float box = s * 0.08f;
        vfx_editor::append_box(verts, cols, idx, Vector3(s, 0, 0), box, cx);
        vfx_editor::append_box(verts, cols, idx, Vector3(0, s, 0), box, cy);
        vfx_editor::append_box(verts, cols, idx, Vector3(0, 0, s), box, cz);
        vfx_editor::append_box(verts, cols, idx, Vector3(), box * 0.8f, cxyz);
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
