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

    mesh_instance->set_mesh(am);  // SET MESH FIRST

    if (show_weights && skin.is_valid()) {
        mesh_instance->set_surface_override_material(0, weight_material);
    } else {
        mesh_instance->set_surface_override_material(0, base_material);
    }
}

// ============================================================================
// SELECTION VISUAL — PRISMA3D-STYLE WIREFRAME + VERTEX/FACE/EDGE HIGHLIGHTS
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

    // Build set of edge IDs that border selected faces (for face-mode highlighting)
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

    // Wireframe edges — Prisma3D style: thin, dark gray, semi-transparent
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
            col = Color(1.0f, 0.8f, 0.2f, 1.0f);   // bright yellow active
            r = 0.010f;
        } else if (in_set) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);   // orange selected
            r = 0.008f;
        } else if (is_face_selected) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);   // orange for selected face border
            r = 0.008f;
        } else {
            col = Color(0.22f, 0.22f, 0.22f, 0.35f); // dark gray
            r = 0.0025f;                              // thinner
        }
        vfx_editor::append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices — keep dots for verts (Blender/Prisma3D style)
    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        bool in_set = selected_vertices.find((int)v->id) != selected_vertices.end();
        bool is_active = (edit_mode == MODE_VERTEX && selected_vertex == (int)v->id);

        Color col;
        float s;
        if (is_active) {
            col = Color(1.0f, 0.8f, 0.2f, 1.0f);
            s = 0.030f;
        } else if (in_set) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);
            s = 0.025f;
        } else {
            col = Color(0.25f, 0.25f, 0.25f, 0.40f); // dark gray
            s = 0.014f;                               // smaller
        }
        vfx_editor::append_box(verts, cols, idx, v->position, s, col);
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
// SKELETON VISUAL — BLENDER-STYLE OCTAHEDRAL + JOINT SPHERES
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

    auto add_tapered_bone = [&](const Vector3& head, const Vector3& tail, const Color& col) {
        Vector3 dir = tail - head;
        float len = dir.length();
        if (len < 0.0001f) return;

        Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
        Vector3 right = dir.cross(up).normalized();
        up = right.cross(dir).normalized();

        // Thickness scales with bone length so short bones don't become dots
        float r_head = MIN(MAX(len * 0.30f, 0.005f), 0.018f);
        float r_tail = MIN(MAX(len * 0.12f, 0.003f), 0.008f);

        int base = verts.size();
        for (int i = 0; i <= 4; i++) {
            float ang = (float)i / 4.0f * 3.14159265f * 2.0f;
            Vector3 offset = right * cosf(ang) + up * sinf(ang);
            verts.push_back(head + offset * r_head); colors.push_back(col);
            verts.push_back(tail + offset * r_tail); colors.push_back(col);
        }

        for (int i = 0; i < 4; i++) {
            int a = base + i * 2;
            int b = base + i * 2 + 1;
            int c = base + ((i + 1) % 4) * 2 + 1;
            int d = base + ((i + 1) % 4) * 2;
            indices.push_back(a); indices.push_back(b); indices.push_back(d);
            indices.push_back(b); indices.push_back(c); indices.push_back(d);
        }
    };

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        bool is_selected = (i == selected_bone);
        Color bone_col = is_selected ? Color(1.0f, 0.95f, 0.5f)
                                     : Color(0.88f, 0.88f, 0.90f);

        add_tapered_bone(head, tail, bone_col);

        // HEAD dot — bright white, slightly larger (the "start" joint)
        Color head_col = is_selected ? Color(1.0f, 0.9f, 0.3f)
                                     : Color(0.98f, 0.98f, 1.0f);
        vfx_editor::append_box(verts, colors, indices, head, 0.020f, head_col);

        // TAIL dot — bone color, slightly smaller (the "end" tip)
        // Render for EVERY bone so you can see where each bone ends
        Color tail_col = is_selected ? Color(1.0f, 0.7f, 0.2f)
                                     : Color(0.75f, 0.75f, 0.78f);
        vfx_editor::append_box(verts, colors, indices, tail, 0.012f, tail_col);
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
