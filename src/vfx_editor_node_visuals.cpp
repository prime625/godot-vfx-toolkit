#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

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

    auto is_face_sel = [&](int fid) -> bool {
        if (edit_mode == MODE_FACE && selected_face == fid) return true;
        for (int sf : selected_faces) if (sf == fid) return true;
        return false;
    };
    auto is_edge_sel = [&](int eid) -> bool {
        if (edit_mode == MODE_EDGE && selected_edge == eid) return true;
        for (int se : selected_edges) if (se == eid) return true;
        return false;
    };
    auto is_vert_sel = [&](int vid) -> bool {
        if (edit_mode == MODE_VERTEX && selected_vertex == vid) return true;
        for (int sv : selected_vertices) if (sv == vid) return true;
        return false;
    };

    // Wireframe edges
    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool sel = is_edge_sel((int)e->id);
        Color col = sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.4f, 0.4f, 0.4f, 0.4f);
        float r = sel ? 0.012f : 0.004f;
        vfx_editor::append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices
    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        bool sel = is_vert_sel((int)v->id);
        Color col = sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.7f, 0.7f, 0.7f, 0.8f);
        float s = sel ? 0.035f : 0.018f;
        vfx_editor::append_box(verts, cols, idx, v->position, s, col);
    }

    // Face centers
    for (auto* f : mesh->get_faces()) {
        if (f->deleted || !f->halfedge) continue;
        Vector3 c = mesh->get_face_center(f->id);
        bool sel = is_face_sel((int)f->id);
        Color col = sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.3f, 0.6f, 1.0f, 0.6f);
        float s = sel ? 0.045f : 0.028f;
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
