#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

using namespace godot;

// ============================================================================
// GIZMO RAYCAST (simplified — uses sphere intersections along axes)
// ============================================================================
int VFXEditorNode::raycast_gizmo(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!gizmo_node || !gizmo_node->is_visible()) return GIZMO_NONE;

    Transform3D world = get_global_transform() * _get_visual_gizmo_transform();
    Vector3 origin = world.origin;

    float best_t = 1e20f;
    int best_axis = GIZMO_NONE;

    // === TRANSLATE / SCALE ===
    if (gizmo_mode == GIZMO_TRANSLATE || gizmo_mode == GIZMO_SCALE) {
        Vector3 axes[3] = { world.basis.get_column(0), world.basis.get_column(1), world.basis.get_column(2) };
        for (int i = 0; i < 3; i++) {
            Vector3 axis_dir = axes[i].normalized();
            // Sample points along axis as small spheres
            for (int s = 1; s <= 6; s++) {
                Vector3 sp = origin + axis_dir * (s * 0.2f);
                float t = _ray_sphere_intersect(ray_origin, ray_dir, sp, 0.06f);
                if (t >= 0.0f && t < best_t) { best_t = t; best_axis = i; }
            }
        }
        // Planes — test center area
        Vector3 plane_normals[3] = { axes[2], axes[1], axes[0] };
        for (int i = 0; i < 3; i++) {
            Vector3 pn = plane_normals[i].normalized();
            float pd = pn.dot(origin);
            float t = _ray_plane_intersect(ray_origin, ray_dir, pn, pd);
            if (t >= 0.0f && t < best_t) {
                Vector3 hit = ray_origin + ray_dir * t;
                Vector3 local = hit - origin;
                float dx = fabs(local.dot(axes[0].normalized()));
                float dy = fabs(local.dot(axes[1].normalized()));
                float dz = fabs(local.dot(axes[2].normalized()));
                bool in_plane = false;
                if (i == 0 && dx < 0.25f && dy < 0.25f) in_plane = true; // XY
                if (i == 1 && dx < 0.25f && dz < 0.25f) in_plane = true; // XZ
                if (i == 2 && dy < 0.25f && dz < 0.25f) in_plane = true; // YZ
                if (in_plane) { best_t = t; best_axis = GIZMO_XY + i; }
            }
        }
        // Center sphere
        float t = _ray_sphere_intersect(ray_origin, ray_dir, origin, 0.12f);
        if (t >= 0.0f && t < best_t) { best_t = t; best_axis = GIZMO_XYZ; }
    }

    // === ROTATE ===
    if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 axes[3] = { world.basis.get_column(0), world.basis.get_column(1), world.basis.get_column(2) };
        for (int i = 0; i < 3; i++) {
            Vector3 axis_dir = axes[i].normalized();
            // Sample torus as ring of spheres
            for (int s = 0; s < 24; s++) {
                float angle = s * (Math_PI * 2.0f / 24.0f);
                Vector3 perp = (fabs(axis_dir.dot(Vector3(0,1,0))) < 0.9f) ? Vector3(0,1,0) : Vector3(1,0,0);
                perp = perp.cross(axis_dir).normalized();
                Vector3 perp2 = axis_dir.cross(perp);
                Vector3 sp = origin + (perp * cosf(angle) + perp2 * sinf(angle)) * 0.9f;
                float t = _ray_sphere_intersect(ray_origin, ray_dir, sp, 0.04f);
                if (t >= 0.0f && t < best_t) { best_t = t; best_axis = i; }
            }
        }
    }

    return best_axis;
}

// ============================================================================
// GIZMO BEGIN DRAG (multi-select aware)
// ============================================================================
void VFXEditorNode::gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir) {
    gizmo_drag_axis = axis;
    gizmo_drag_start_transform = gizmo_transform;

    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();

    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        if (edit_mode == MODE_VERTEX) {
            if (!selected_vertices.empty()) {
                for (int vid : selected_vertices) {
                    mesh_edit_verts.push_back(vid);
                    mesh_edit_initial_positions.push_back(mesh->get_vertex_position(vid));
                }
            } else if (selected_vertex >= 0) {
                mesh_edit_verts.push_back(selected_vertex);
                mesh_edit_initial_positions.push_back(mesh->get_vertex_position(selected_vertex));
            }
        } else if (edit_mode == MODE_EDGE) {
            std::vector<int> unique_verts;
            if (!selected_edges.empty()) {
                for (int eid : selected_edges) {
                    int v0, v1;
                    mesh->get_edge_endpoints(eid, v0, v1);
                    if (v0 >= 0 && std::find(unique_verts.begin(), unique_verts.end(), v0) == unique_verts.end()) unique_verts.push_back(v0);
                    if (v1 >= 0 && std::find(unique_verts.begin(), unique_verts.end(), v1) == unique_verts.end()) unique_verts.push_back(v1);
                }
            } else if (selected_edge >= 0) {
                int v0, v1;
                mesh->get_edge_endpoints(selected_edge, v0, v1);
                if (v0 >= 0) unique_verts.push_back(v0);
                if (v1 >= 0) unique_verts.push_back(v1);
            }
            for (int vid : unique_verts) {
                mesh_edit_verts.push_back(vid);
                mesh_edit_initial_positions.push_back(mesh->get_vertex_position(vid));
            }
        } else if (edit_mode == MODE_FACE) {
            std::vector<int> unique_verts;
            if (!selected_faces.empty()) {
                for (int fid : selected_faces) {
                    std::vector<vfx::HEVertex*> verts;
                    mesh->get_face_vertices(fid, verts);
                    for (auto* v : verts) {
                        if (std::find(unique_verts.begin(), unique_verts.end(), (int)v->id) == unique_verts.end())
                            unique_verts.push_back((int)v->id);
                    }
                }
            } else if (selected_face >= 0) {
                std::vector<vfx::HEVertex*> verts;
                mesh->get_face_vertices(selected_face, verts);
                for (auto* v : verts) {
                    unique_verts.push_back((int)v->id);
                }
            }
            for (int vid : unique_verts) {
                mesh_edit_verts.push_back(vid);
                mesh_edit_initial_positions.push_back(mesh->get_vertex_position(vid));
            }
        }
    }

    if (edit_mode == MODE_OBJECT) {
        if (active_scene_node.is_valid()) {
            gizmo_drag_start_transform = active_scene_node->get_local_transform();
        } else if (skeleton.is_valid() && selected_bone >= 0) {
            gizmo_drag_start_transform = skeleton->get_bone_model_transform(selected_bone);
        }
    }

    if (gizmo_mode == GIZMO_ROTATE) {
        gizmo_drag_start_rotation = gizmo_drag_start_transform.basis.get_rotation_quaternion();
    }
    if (gizmo_mode == GIZMO_SCALE) {
        gizmo_drag_start_scale = gizmo_drag_start_transform.basis.get_scale();
    }

    // Build drag plane
    Transform3D world = get_global_transform() * _get_visual_gizmo_transform();
    Vector3 origin = world.origin;
    Vector3 axis_dir;

    if (axis < 3) {
        axis_dir = world.basis.get_column(axis).normalized();
    } else if (axis == GIZMO_XY) {
        axis_dir = world.basis.get_column(2).normalized();
    } else if (axis == GIZMO_XZ) {
        axis_dir = world.basis.get_column(1).normalized();
    } else if (axis == GIZMO_YZ) {
        axis_dir = world.basis.get_column(0).normalized();
    } else {
        axis_dir = camera ? camera->get_global_transform().basis.get_column(2) : Vector3(0, 0, 1);
    }

    gizmo_drag_plane_normal = axis_dir;
    gizmo_drag_plane_d = gizmo_drag_plane_normal.dot(origin);

    float t = _ray_plane_intersect(ray_origin, ray_dir, gizmo_drag_plane_normal, gizmo_drag_plane_d);
    if (t >= 0.0f) {
        gizmo_drag_start_point = ray_origin + ray_dir * t;
    } else {
        gizmo_drag_start_point = origin;
    }
    gizmo_dragging = true;
}

// ============================================================================
// GIZMO DRAG
// ============================================================================
void VFXEditorNode::gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!gizmo_dragging) return;

    float t = _ray_plane_intersect(ray_origin, ray_dir, gizmo_drag_plane_normal, gizmo_drag_plane_d);
    if (t < 0.0f) return;
    Vector3 current_point = ray_origin + ray_dir * t;
    Vector3 delta = current_point - gizmo_drag_start_point;

    Transform3D world = get_global_transform() * _get_visual_gizmo_transform();
    Vector3 origin = world.origin;

    // === TRANSLATE ===
    if (gizmo_mode == GIZMO_TRANSLATE) {
        Vector3 move = Vector3();
        if (gizmo_drag_axis < 3) {
            Vector3 axis = world.basis.get_column(gizmo_drag_axis).normalized();
            move = axis * delta.dot(axis);
        } else if (gizmo_drag_axis == GIZMO_XY) {
            Vector3 x = world.basis.get_column(0).normalized();
            Vector3 y = world.basis.get_column(1).normalized();
            move = x * delta.dot(x) + y * delta.dot(y);
        } else if (gizmo_drag_axis == GIZMO_XZ) {
            Vector3 x = world.basis.get_column(0).normalized();
            Vector3 z = world.basis.get_column(2).normalized();
            move = x * delta.dot(x) + z * delta.dot(z);
        } else if (gizmo_drag_axis == GIZMO_YZ) {
            Vector3 y = world.basis.get_column(1).normalized();
            Vector3 z = world.basis.get_column(2).normalized();
            move = y * delta.dot(y) + z * delta.dot(z);
        } else {
            move = delta;
        }

        gizmo_transform = gizmo_drag_start_transform;
        gizmo_transform.origin += move;

        if (edit_mode == MODE_OBJECT) {
            if (active_scene_node.is_valid()) {
                Transform3D t = gizmo_drag_start_transform;
                t.origin += move;
                active_scene_node->set_local_transform(t);
                if (mesh_instance) mesh_instance->set_transform(active_scene_node->get_local_transform());
            } else if (skeleton.is_valid() && selected_bone >= 0) {
                Transform3D t = gizmo_drag_start_transform;
                t.origin += move;
                skeleton->set_bone_model_transform(selected_bone, t);
            }
        } else if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
            if ((edit_mode == MODE_VERTEX && (selected_vertex >= 0 || !selected_vertices.empty())) ||
                (edit_mode == MODE_EDGE   && (selected_edge >= 0   || !selected_edges.empty())) ||
                (edit_mode == MODE_FACE   && (selected_face >= 0   || !selected_faces.empty()))) {
                _apply_proportional(move, gizmo_transform.origin);
            }
        }
    }

    // === ROTATE ===
    if (gizmo_mode == GIZMO_ROTATE) {
        if (gizmo_drag_axis < 3) {
            Vector3 axis = world.basis.get_column(gizmo_drag_axis).normalized();
            float angle = delta.length() * 0.05f;
            if (delta.dot(axis.cross(camera ? camera->get_global_transform().basis.get_column(0) : Vector3(1,0,0))) < 0)
                angle = -angle;
            Quaternion rot(axis, angle);
            gizmo_transform.basis = Basis(rot * gizmo_drag_start_rotation);

            if (edit_mode == MODE_OBJECT) {
                if (active_scene_node.is_valid()) {
                    Transform3D t = gizmo_drag_start_transform;
                    t.basis = gizmo_transform.basis;
                    active_scene_node->set_local_transform(t);
                } else if (skeleton.is_valid() && selected_bone >= 0) {
                    Transform3D t = gizmo_drag_start_transform;
                    t.basis = gizmo_transform.basis;
                    skeleton->set_bone_model_transform(selected_bone, t);
                }
            } else if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
                if ((edit_mode == MODE_VERTEX && (selected_vertex >= 0 || !selected_vertices.empty())) ||
                    (edit_mode == MODE_EDGE   && (selected_edge >= 0   || !selected_edges.empty())) ||
                    (edit_mode == MODE_FACE   && (selected_face >= 0   || !selected_faces.empty()))) {
                    Vector3 center = gizmo_transform.origin;
                    for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
                        Vector3 local = mesh_edit_initial_positions[i] - center;
                        Vector3 rotated = rot.xform(local);
                        mesh->set_vertex_position(mesh_edit_verts[i], center + rotated);
                    }
                }
            }
        }
    }

    // === SCALE ===
    if (gizmo_mode == GIZMO_SCALE) {
        Vector3 scale = Vector3(1, 1, 1);
        float s = 1.0f + delta.dot(Vector3(1,1,1).normalized()) * 0.05f;
        if (gizmo_drag_axis < 3) {
            scale[gizmo_drag_axis] = s;
        } else if (gizmo_drag_axis >= GIZMO_XY && gizmo_drag_axis <= GIZMO_YZ) {
            if (gizmo_drag_axis == GIZMO_XY) { scale.x = s; scale.y = s; }
            if (gizmo_drag_axis == GIZMO_XZ) { scale.x = s; scale.z = s; }
            if (gizmo_drag_axis == GIZMO_YZ) { scale.y = s; scale.z = s; }
        } else {
            scale = Vector3(s, s, s);
        }

        gizmo_transform.basis = Basis();
        gizmo_transform.basis.scale(gizmo_drag_start_scale * scale);

        if (edit_mode == MODE_OBJECT) {
            if (active_scene_node.is_valid()) {
                Transform3D t = gizmo_drag_start_transform;
                t.basis = Basis().scaled(gizmo_drag_start_scale * scale);
                active_scene_node->set_local_transform(t);
            } else if (skeleton.is_valid() && selected_bone >= 0) {
                Transform3D t = gizmo_drag_start_transform;
                t.basis = Basis().scaled(gizmo_drag_start_scale * scale);
                skeleton->set_bone_model_transform(selected_bone, t);
            }
        } else if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
            if ((edit_mode == MODE_VERTEX && (selected_vertex >= 0 || !selected_vertices.empty())) ||
                (edit_mode == MODE_EDGE   && (selected_edge >= 0   || !selected_edges.empty())) ||
                (edit_mode == MODE_FACE   && (selected_face >= 0   || !selected_faces.empty()))) {
                Vector3 center = gizmo_transform.origin;
                for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
                    Vector3 local = mesh_edit_initial_positions[i] - center;
                    mesh->set_vertex_position(mesh_edit_verts[i], center + local * scale);
                }
            }
        }
    }

    if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
}

// ============================================================================
// GIZMO END DRAG
// ============================================================================
void VFXEditorNode::gizmo_end_drag() {
    gizmo_dragging = false;
    gizmo_drag_axis = GIZMO_NONE;
    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();
}

bool VFXEditorNode::is_gizmo_dragging() const {
    return gizmo_dragging;
}

// ============================================================================
// PROPORTIONAL EDITING APPLY
// ============================================================================
void VFXEditorNode::_apply_proportional(const Vector3& move, const Vector3& center) {
    if (mesh.is_null()) return;
    for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
        int vid = mesh_edit_verts[i];
        Vector3 pos = mesh_edit_initial_positions[i];
        float weight = 1.0f;
        if (proportional_enabled) {
            float dist = (pos - center).length();
            if (dist > proportional_radius) continue;
            weight = vfx::evaluate_falloff(dist / proportional_radius, proportional_falloff);
        }
        mesh->set_vertex_position(vid, pos + move * weight);
    }
}
