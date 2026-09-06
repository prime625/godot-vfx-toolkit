#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>

using namespace godot;

// ============================================================================
// MESH INSTANCE — UPDATE FROM VFXMesh
// ============================================================================
void VFXEditorNode::_update_godot_mesh() {
    _ensure_mesh_instance();
    if (mesh.is_null()) return;

    Ref<ArrayMesh> am = _build_array_mesh_for_node(mesh, skeleton, skin, show_weights, visualize_bone);
    if (am.is_null() || am->get_surface_count() == 0) {
        mesh_instance->set_mesh(Ref<ArrayMesh>());
        return;
    }

    mesh_instance->set_mesh(am);

    if (show_weights && skin.is_valid()) {
        mesh_instance->set_surface_override_material(0, weight_material);
    } else {
        mesh_instance->set_surface_override_material(0, base_material);
    }
}

// ============================================================================
// SELECTION VISUAL — PRISMA3D-STYLE WIREFRAME
// ============================================================================
void VFXEditorNode::_build_selection_mesh() {
    if (!selection_visual) _ensure_selection_visual();
    if (mesh.is_null() || !show_wireframe) {
        selection_visual->set_mesh(Ref<ArrayMesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    // === SCREEN-SPACE CONSISTENT SCALE (Prisma3D style) ===
    float cam_dist = 5.0f;
    if (camera) {
        Vector3 mesh_pos = _get_active_mesh_transform().get_origin();
        cam_dist = camera->get_global_transform().get_origin().distance_to(mesh_pos);
        if (cam_dist < 0.001f) cam_dist = 0.001f;
    }
    float dist_scale = cam_dist / 5.0f;
    if (dist_scale < 0.2f) dist_scale = 0.2f;
    if (dist_scale > 3.0f) dist_scale = 3.0f;

    const float BASE_EDGE_R = 0.008f;
    const float BASE_VERT_S = 0.030f;
    float base_edge_r = BASE_EDGE_R * dist_scale;
    float base_vert_s = BASE_VERT_S * dist_scale;

    // Build set of edge IDs that border selected faces
    std::set<int> selected_face_edge_ids;
    if (edit_mode == MODE_FACE) {
        for (auto* f : mesh->get_faces()) {
            if (f->deleted || !f->halfedge) continue;
            if (selected_faces.find((int)f->id) == selected_faces.end()) continue;
            auto* he = f->halfedge;
            auto* start = he;
            do {
                if (he && he->id >= 0) selected_face_edge_ids.insert((int)he->id);
                he = he->next;
            } while (he && he != start);
        }
    }

    // Wireframe edges — black, screen-space consistent thickness
    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool in_set = selected_edges.find((int)e->id) != selected_edges.end();
        bool is_active = (edit_mode == MODE_EDGE && selected_edge == (int)e->id);
        bool is_face_selected = (edit_mode == MODE_FACE && selected_face_edge_ids.find((int)e->id) != selected_face_edge_ids.end());

        Color col;
        float r;
        if (is_active) {
            col = Color(1.0f, 0.8f, 0.2f, 1.0f);
            r = base_edge_r * 4.0f;
        } else if (in_set) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);
            r = base_edge_r * 3.2f;
        } else if (is_face_selected) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);
            r = base_edge_r * 3.2f;
        } else {
            col = Color(0.0f, 0.0f, 0.0f, 1.0f);
            r = base_edge_r;
        }
        vfx_editor::append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices — ONLY in vertex mode, black dots
    if (edit_mode == MODE_VERTEX) {
        for (auto* v : mesh->get_vertices()) {
            if (v->deleted) continue;
            bool in_set = selected_vertices.find((int)v->id) != selected_vertices.end();
            bool is_active = (selected_vertex == (int)v->id);

            Color col;
            float s;
            if (is_active) {
                col = Color(1.0f, 0.8f, 0.2f, 1.0f);
                s = base_vert_s * 2.5f;
            } else if (in_set) {
                col = Color(1.0f, 0.5f, 0.0f, 1.0f);
                s = base_vert_s * 2.0f;
            } else {
                col = Color(0.0f, 0.0f, 0.0f, 1.0f);
                s = base_vert_s;
            }
            vfx_editor::append_box(verts, cols, idx, v->position, s, col);
        }
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
    selection_visual->set_material_override(mat);
    selection_visual->set_mesh(am);
    selection_visual->set_transform(_get_active_mesh_transform());
}

// ============================================================================
// CUSTOM MESH SURFACE APPENDER
// ============================================================================
void VFXEditorNode::_append_mesh_surface_transformed(
    PackedVector3Array& r_verts,
    PackedColorArray& r_cols,
    PackedInt32Array& r_idx,
    const Ref<Mesh>& p_mesh,
    const Transform3D& p_transform,
    const Color& p_tint)
{
    if (p_mesh.is_null()) return;

    int base_vert = r_verts.size();

    for (int surf = 0; surf < p_mesh->get_surface_count(); surf++) {
        Array arrays = p_mesh->surface_get_arrays(surf);
        if (arrays.is_empty()) continue;

        PackedVector3Array src_verts = arrays[Mesh::ARRAY_VERTEX];
        PackedColorArray src_cols = arrays[Mesh::ARRAY_COLOR];
        PackedInt32Array src_idx = arrays[Mesh::ARRAY_INDEX];

        if (src_verts.is_empty()) continue;

        bool has_color = src_cols.size() == src_verts.size();

        for (int i = 0; i < src_verts.size(); i++) {
            r_verts.append(p_transform.xform(src_verts[i]));
            if (has_color) {
                Color c = src_cols[i] * p_tint;
                c.a = src_cols[i].a;
                r_cols.append(c);
            } else {
                r_cols.append(p_tint);
            }
        }

        if (!src_idx.is_empty()) {
            for (int i = 0; i < src_idx.size(); i++) {
                r_idx.append(src_idx[i] + base_vert);
            }
        } else {
            for (int i = 0; i < src_verts.size(); i++) {
                r_idx.append(base_vert + i);
            }
        }

        base_vert = r_verts.size();
    }
}

// ============================================================================
// SKELETON VISUAL — custom mesh or procedural fallback
// ============================================================================
void VFXEditorNode::_build_skeleton_mesh() {
    if (!skel_visual) _ensure_skeleton_visual();
    if (skeleton.is_null() || skeleton->get_bone_count() == 0) {
        skel_visual->set_mesh(Ref<ArrayMesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray colors;
    PackedInt32Array indices;

    skeleton->update_transforms();

    // === SCREEN-SPACE CONSISTENT SCALE ===
    float cam_dist = 5.0f;
    if (camera) {
        Vector3 skel_center = skeleton->get_bone_model_transform(0).get_origin();
        cam_dist = camera->get_global_transform().get_origin().distance_to(skel_center);
        if (cam_dist < 0.001f) cam_dist = 0.001f;
    }
    float dist_scale = cam_dist / 5.0f;
    if (dist_scale < 0.2f) dist_scale = 0.2f;
    if (dist_scale > 3.0f) dist_scale = 3.0f;

    bool use_custom = bone_shaft_mesh.is_valid() && bone_joint_mesh.is_valid();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 pos = skeleton->get_bone_model_transform(i).get_origin();
        Vector3 parent_pos = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : pos;

        bool is_selected = (i == selected_bone);
        Color tint = is_selected ? Color(1.0f, 0.92f, 0.35f) : Color(1.0f, 1.0f, 1.0f);

        if (use_custom) {
            // --- CUSTOM MESH MODE ---
            if (parent >= 0) {
                Vector3 dir = pos - parent_pos;
                float len = dir.length();
                if (len > 0.0001f) {
                    Vector3 y = dir / len;
                    Vector3 x = y.cross(Vector3(0, 0, 1)).normalized();
                    if (x.length_squared() < 0.001f) x = Vector3(1, 0, 0);
                    Vector3 z = x.cross(y).normalized();

                    Basis b;
                    b.set_column(0, x * dist_scale);
                    b.set_column(1, y * len);
                    b.set_column(2, z * dist_scale);

                    Transform3D shaft_t;
                    shaft_t.basis = b;
                    shaft_t.origin = parent_pos;

                    _append_mesh_surface_transformed(verts, colors, indices,bone_shaft_mesh, shaft_t, tint);
                }
            }

            Transform3D joint_t;
            joint_t.basis = Basis().scaled(Vector3(dist_scale, dist_scale, dist_scale));
            joint_t.origin = pos;
            _append_mesh_surface_transformed(verts, colors, indices,bone_joint_mesh, joint_t, tint);

        } else {
            // --- BLENDER-STYLE OCTAHEDRAL BONES ---
            float bone_r = bone_shaft_radius * dist_scale;
            float joint_r = bone_joint_radius * dist_scale;

            Color bone_col = is_selected ? Color(1.0f, 0.95f, 0.5f)
                                         : Color(1.0f, 1.0f, 1.0f);
            Color joint_col = is_selected ? Color(1.0f, 0.92f, 0.35f)
                                          : Color(1.0f, 1.0f, 1.0f);

            // Tapered pyramid shaft: wide base at parent, sharp apex at child
            if (parent >= 0 && (pos - parent_pos).length() > 0.0001f) {
                Vector3 dir = pos - parent_pos;
                float len = dir.length();
                Vector3 y = dir / len;
                Vector3 x = y.cross(Vector3(0, 0, 1)).normalized();
                if (x.length_squared() < 0.001f) x = Vector3(1, 0, 0);
                Vector3 z = x.cross(y).normalized();

                float w = bone_r * 2.5f;  // base width

                // Square base at parent (perpendicular to bone axis)
                Vector3 b0 = parent_pos + (x + z) * w;
                Vector3 b1 = parent_pos + (-x + z) * w;
                Vector3 b2 = parent_pos + (-x - z) * w;
                Vector3 b3 = parent_pos + (x - z) * w;
                Vector3 apex = pos;

                int base = verts.size();
                Vector3 v[5] = {apex, b0, b1, b2, b3};
                for (int i = 0; i < 5; i++) {
                    verts.append(v[i]);
                    cols.append(bone_col);
                }
                // 4 side faces
                idx.append(base+0); idx.append(base+1); idx.append(base+2);
                idx.append(base+0); idx.append(base+2); idx.append(base+3);
                idx.append(base+0); idx.append(base+3); idx.append(base+4);
                idx.append(base+0); idx.append(base+4); idx.append(base+1);

                // Base cap (2 triangles)
                int cb = verts.size();
                verts.append(b0); cols.append(bone_col);
                verts.append(b2); cols.append(bone_col);
                verts.append(b1); cols.append(bone_col);
                verts.append(b0); cols.append(bone_col);
                verts.append(b3); cols.append(bone_col);
                verts.append(b2); cols.append(bone_col);
                idx.append(cb+0); idx.append(cb+1); idx.append(cb+2);
                idx.append(cb+3); idx.append(cb+4); idx.append(cb+5);
            }

            // Joint sphere at this bone's position
            float jr = is_selected ? joint_r * 1.3f : joint_r;
            vfx_editor::append_sphere(verts, colors, indices, pos, jr, joint_col);
        }
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = colors;
        arrays[Mesh::ARRAY_INDEX] = indices;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
    skel_visual->set_material_override(mat);
    skel_visual->set_mesh(am);
}