#include "vfx_editor_node.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

using namespace godot;

// ============================================================================
// SINGLE-FACE WRAPPERS (backward compatible)
// ============================================================================
void VFXEditorNode::extrude_selected_face(float distance) {
    if (mesh.is_null()) return;
    if (selected_face >= 0) {
        mesh->extrude_face(selected_face, distance);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::inset_selected_face(float amount) {
    if (mesh.is_null()) return;
    if (selected_face >= 0) {
        mesh->inset_face(selected_face, amount);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::delete_selected_face() {
    if (mesh.is_null()) return;
    if (selected_face >= 0) {
        mesh->delete_face(selected_face);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::subdivide_selected_face() {
    if (mesh.is_null()) return;
    if (selected_face >= 0) {
        mesh->subdivide_face(selected_face);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::flip_normals() {
    if (mesh.is_null()) return;
    mesh->flip_all_normals();
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
}

void VFXEditorNode::mesh_cleanup() {
    if (mesh.is_null()) return;
    mesh->cleanup();
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
}

// ============================================================================
// MULTI-SELECT MODELING OPS
// ============================================================================
void VFXEditorNode::extrude_selection(float distance) {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_FACE) {
        if (!selected_faces.empty()) {
            // Descending order so indices don't shift
            std::vector<int> to_extrude = selected_faces;
            std::sort(to_extrude.begin(), to_extrude.end(), std::greater<int>());
            for (int fid : to_extrude) {
                if (fid >= 0 && fid < (int)mesh->get_faces().size()) {
                    mesh->extrude_face(fid, distance);
                }
            }
            selected_faces.clear();
            selected_face = -1;
        } else if (selected_face >= 0) {
            mesh->extrude_face(selected_face, distance);
            selected_face = -1;
        }
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->bevel_edge(selected_edge, distance);
        selected_edge = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::inset_selection(float amount) {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_FACE && !selected_faces.empty()) {
        std::vector<int> to_inset = selected_faces;
        std::sort(to_inset.begin(), to_inset.end(), std::greater<int>());
        for (int fid : to_inset) {
            if (fid >= 0 && fid < (int)mesh->get_faces().size()) {
                mesh->inset_face(fid, amount);
            }
        }
        selected_faces.clear();
        selected_face = -1;
    } else if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->inset_face(selected_face, amount);
        selected_face = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::delete_selection() {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_FACE && !selected_faces.empty()) {
        std::vector<int> to_del = selected_faces;
        std::sort(to_del.begin(), to_del.end(), std::greater<int>());
        for (int fid : to_del) {
            if (fid >= 0 && fid < (int)mesh->get_faces().size()) {
                mesh->delete_face(fid);
            }
        }
        selected_faces.clear();
        selected_face = -1;
    } else if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->delete_face(selected_face);
        selected_face = -1;
    } else if (edit_mode == MODE_EDGE && !selected_edges.empty()) {
        std::vector<int> to_del = selected_edges;
        std::sort(to_del.begin(), to_del.end(), std::greater<int>());
        for (int eid : to_del) {
            if (eid >= 0 && eid < (int)mesh->get_edges().size()) {
                mesh->dissolve_edge(eid);
            }
        }
        selected_edges.clear();
        selected_edge = -1;
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->dissolve_edge(selected_edge);
        selected_edge = -1;
    } else if (edit_mode == MODE_VERTEX && !selected_vertices.empty()) {
        std::vector<int> to_del = selected_vertices;
        std::sort(to_del.begin(), to_del.end(), std::greater<int>());
        for (int vid : to_del) {
            if (vid >= 0 && vid < (int)mesh->get_vertices().size()) {
                mesh->dissolve_vertex(vid);
            }
        }
        selected_vertices.clear();
        selected_vertex = -1;
    } else if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        mesh->dissolve_vertex(selected_vertex);
        selected_vertex = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::subdivide_selection() {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_FACE && !selected_faces.empty()) {
        std::vector<int> to_sub = selected_faces;
        std::sort(to_sub.begin(), to_sub.end(), std::greater<int>());
        for (int fid : to_sub) {
            if (fid >= 0 && fid < (int)mesh->get_faces().size()) {
                mesh->subdivide_face(fid);
            }
        }
        selected_faces.clear();
        selected_face = -1;
    } else if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->subdivide_face(selected_face);
        selected_face = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::bevel_selection(float amount) {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_EDGE && !selected_edges.empty()) {
        std::vector<int> to_bev = selected_edges;
        std::sort(to_bev.begin(), to_bev.end(), std::greater<int>());
        for (int eid : to_bev) {
            if (eid >= 0 && eid < (int)mesh->get_edges().size()) {
                mesh->bevel_edge(eid, amount);
            }
        }
        selected_edges.clear();
        selected_edge = -1;
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->bevel_edge(selected_edge, amount);
        selected_edge = -1;
    } else if (edit_mode == MODE_VERTEX && !selected_vertices.empty()) {
        std::vector<int> to_bev = selected_vertices;
        std::sort(to_bev.begin(), to_bev.end(), std::greater<int>());
        for (int vid : to_bev) {
            if (vid >= 0 && vid < (int)mesh->get_vertices().size()) {
                mesh->bevel_vertex(vid, amount);
            }
        }
        selected_vertices.clear();
        selected_vertex = -1;
    } else if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        mesh->bevel_vertex(selected_vertex, amount);
        selected_vertex = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::knife_selection(const Vector3& p0, const Vector3& p1) {
    if (mesh.is_null()) return;

    if (edit_mode == MODE_FACE && !selected_faces.empty()) {
        for (int fid : selected_faces) {
            if (fid >= 0 && fid < (int)mesh->get_faces().size()) {
                mesh->knife_face(fid, p0, p1);
            }
        }
        selected_faces.clear();
        selected_face = -1;
    } else if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->knife_face(selected_face, p0, p1);
        selected_face = -1;
    }

    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}
