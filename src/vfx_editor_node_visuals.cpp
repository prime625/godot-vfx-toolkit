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
    if (am.is_null()) return;

    if (show_weights && skin.is_valid()) {
        mesh_instance->set_surface_override_material(0, weight_material);
    } else {
        mesh_instance->set_surface_override_material(0, base_material);
    }

    mesh_instance->set_mesh(am);
}

// ============================================================================
// SELECTION VISUAL — WIREFRAME + VERTEX/FACE/EDGE HIGHLIGHTS
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

    // Wireframe edges
    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool is_sel = (edit_mode == MODE_EDGE && selected_edge == (int)e->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.4f, 0.4f, 0.4f, 0.4f);
        float r = is_sel ? 0.012f : 0.004f;
        vfx_editor::append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices
    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        bool is_sel = (edit_mode == MODE_VERTEX && selected_vertex == (int)v->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.7f, 0.7f, 0.7f, 0.8f);
        float s = is_sel ? 0.035f : 0.018f;
        vfx_editor::append_box(verts, cols, idx, v->position, s, col);
    }

    // Face centers
    for (auto* f : mesh->get_faces()) {
        if (f->deleted || !f->halfedge) continue;
        Vector3 c = mesh->get_face_center(f->id);
        bool is_sel = (edit_mode == MODE_FACE && selected_face == (int)f->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.3f, 0.6f, 1.0f, 0.6f);
        float s = is_sel ? 0.045f : 0.028f;
        vfx_editor::append_box(verts, cols, idx, c, s, col);
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

    auto add_octa = [&](const Vector3& head, const Vector3& tail, const Color& col) {
        Vector3 dir = tail - head;
        float len = dir.length();
        if (len < 0.0001f) return;
        dir /= len;

        Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
        Vector3 right = dir.cross(up).normalized();
        up = right.cross(dir).normalized();

        float width = len * 0.18f;
        Vector3 center = (head + tail) * 0.5f;

        int base = verts.size();
        verts.push_back(head);
        verts.push_back(tail);
        verts.push_back(center + right * width);
        verts.push_back(center - right * width);
        verts.push_back(center + up * width);
        verts.push_back(center - up * width);

        for (int i = 0; i < 6; i++) colors.push_back(col);

        const int tri[] = {
            0, 2, 4, 0, 4, 3, 0, 3, 5, 0, 5, 2,
            1, 4, 2, 1, 3, 4, 1, 5, 3, 1, 2, 5
        };
        for (int i = 0; i < 24; i++) indices.push_back(base + tri[i]);
    };

    auto add_sphere = [&](const Vector3& center, float radius, int segs, int rings, const Color& col) {
        int base = verts.size();

        for (int r = 0; r <= rings; r++) {
            float phi = (float)r / rings * 3.14159265f;
            for (int s = 0; s <= segs; s++) {
                float theta = (float)s / segs * 3.14159265f * 2.0f;
                Vector3 p(
                    sinf(phi) * cosf(theta) * radius,
                    cosf(phi) * radius,
                    sinf(phi) * sinf(theta) * radius
                );
                verts.push_back(center + p);
                colors.push_back(col);
            }
        }

        for (int r = 0; r < rings; r++) {
            for (int s = 0; s < segs; s++) {
                int a = base + r * (segs + 1) + s;
                int b = base + (r + 1) * (segs + 1) + s;
                int c = base + (r + 1) * (segs + 1) + (s + 1);
                int d = base + r * (segs + 1) + (s + 1);

                indices.push_back(a); indices.push_back(b); indices.push_back(d);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
    };

    skeleton->update_transforms();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        bool is_selected = (i == selected_bone);
        Color col = is_selected ? Color(1.0f, 0.6f, 0.0f) : Color(0.25f, 0.55f, 0.85f);
        if (i == 0) col = Color(0.6f, 0.6f, 0.6f);

        add_octa(head, tail, col);

        float joint_radius = (tail - head).length() * 0.22f;
        if (joint_radius < 0.04f) joint_radius = 0.04f;
        if (joint_radius > 0.12f) joint_radius = 0.12f;

        Color joint_col = is_selected ? Color(1.0f, 0.8f, 0.2f) : Color(0.35f, 0.65f, 0.95f);
        if (i == 0) joint_col = Color(0.7f, 0.7f, 0.7f);

        add_sphere(head, joint_radius, 8, 6, joint_col);

        if (skeleton->get_bone_children(i).size() == 0) {
            add_sphere(tail, joint_radius * 0.6f, 6, 4, col);
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
    skel_visual->set_material_override(mat);
    skel_visual->set_mesh(am);
}