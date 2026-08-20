#include "vfx_uv_editor.h"
#include "vfx_math.h"
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <stack>
#include <algorithm>

using namespace godot;

void VFXUVEditor::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXUVEditor::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXUVEditor::get_mesh);
    ClassDB::bind_method(D_METHOD("set_active_layer", "layer"), &VFXUVEditor::set_active_layer);
    ClassDB::bind_method(D_METHOD("get_active_layer"), &VFXUVEditor::get_active_layer);

    ClassDB::bind_method(D_METHOD("get_uv_vert_count"), &VFXUVEditor::get_uv_vert_count);
    ClassDB::bind_method(D_METHOD("get_uv_vert", "uv_idx"), &VFXUVEditor::get_uv_vert);
    ClassDB::bind_method(D_METHOD("set_uv_vert", "uv_idx", "uv"), &VFXUVEditor::set_uv_vert);
    ClassDB::bind_method(D_METHOD("get_face_uv_polygon", "face_idx"), &VFXUVEditor::get_face_uv_polygon);
    ClassDB::bind_method(D_METHOD("get_face_uv_indices", "face_idx"), &VFXUVEditor::get_face_uv_indices);
    ClassDB::bind_method(D_METHOD("get_face_uv_edges", "face_idx"), &VFXUVEditor::get_face_uv_edges);
    ClassDB::bind_method(D_METHOD("get_uv_vert_linked_faces", "uv_idx"), &VFXUVEditor::get_uv_vert_linked_faces);

    ClassDB::bind_method(D_METHOD("set_select_mode", "mode"), &VFXUVEditor::set_select_mode);
    ClassDB::bind_method(D_METHOD("get_select_mode"), &VFXUVEditor::get_select_mode);
    ClassDB::bind_method(D_METHOD("select_all"), &VFXUVEditor::select_all);
    ClassDB::bind_method(D_METHOD("deselect_all"), &VFXUVEditor::deselect_all);
    ClassDB::bind_method(D_METHOD("invert_selection"), &VFXUVEditor::invert_selection);
    ClassDB::bind_method(D_METHOD("select_at", "uv_pos", "radius", "add"), &VFXUVEditor::select_at);
    ClassDB::bind_method(D_METHOD("select_edge_at", "uv_pos", "radius", "add"), &VFXUVEditor::select_edge_at);
    ClassDB::bind_method(D_METHOD("select_face_at", "uv_pos", "add"), &VFXUVEditor::select_face_at);
    ClassDB::bind_method(D_METHOD("select_island_at", "uv_pos"), &VFXUVEditor::select_island_at);
    ClassDB::bind_method(D_METHOD("select_box", "box", "add"), &VFXUVEditor::select_box);
    ClassDB::bind_method(D_METHOD("select_island_box", "box", "add"), &VFXUVEditor::select_island_box);
    ClassDB::bind_method(D_METHOD("select_island", "island_idx"), &VFXUVEditor::select_island);
    ClassDB::bind_method(D_METHOD("deselect_island", "island_idx"), &VFXUVEditor::deselect_island);
    ClassDB::bind_method(D_METHOD("get_selected_verts"), &VFXUVEditor::get_selected_verts);
    ClassDB::bind_method(D_METHOD("get_selected_faces"), &VFXUVEditor::get_selected_faces);
    ClassDB::bind_method(D_METHOD("is_uv_selected", "uv_idx"), &VFXUVEditor::is_uv_selected);
    ClassDB::bind_method(D_METHOD("is_face_selected", "face_idx"), &VFXUVEditor::is_face_selected);

    ClassDB::bind_method(D_METHOD("translate_selected", "delta"), &VFXUVEditor::translate_selected);
    ClassDB::bind_method(D_METHOD("rotate_selected", "angle_rad", "pivot"), &VFXUVEditor::rotate_selected);
    ClassDB::bind_method(D_METHOD("scale_selected", "scale", "pivot"), &VFXUVEditor::scale_selected);
    ClassDB::bind_method(D_METHOD("mirror_selected_horizontal", "pivot_x"), &VFXUVEditor::mirror_selected_horizontal, DEFVAL(0.5f));
    ClassDB::bind_method(D_METHOD("mirror_selected_vertical", "pivot_y"), &VFXUVEditor::mirror_selected_vertical, DEFVAL(0.5f));

    ClassDB::bind_method(D_METHOD("project_planar", "normal"), &VFXUVEditor::project_planar);
    ClassDB::bind_method(D_METHOD("project_cylindrical", "axis"), &VFXUVEditor::project_cylindrical);
    ClassDB::bind_method(D_METHOD("project_spherical", "axis"), &VFXUVEditor::project_spherical);
    ClassDB::bind_method(D_METHOD("project_box"), &VFXUVEditor::project_box);

    ClassDB::bind_method(D_METHOD("unwrap_selected_faces"), &VFXUVEditor::unwrap_selected_faces);
    ClassDB::bind_method(D_METHOD("unwrap_island", "island_idx"), &VFXUVEditor::unwrap_island);

    ClassDB::bind_method(D_METHOD("split_selected_verts"), &VFXUVEditor::split_selected_verts);
    ClassDB::bind_method(D_METHOD("weld_selected_verts"), &VFXUVEditor::weld_selected_verts);
    ClassDB::bind_method(D_METHOD("stitch_selected"), &VFXUVEditor::stitch_selected);

    ClassDB::bind_method(D_METHOD("get_island_count"), &VFXUVEditor::get_island_count);
    ClassDB::bind_method(D_METHOD("get_island_faces", "island_idx"), &VFXUVEditor::get_island_faces);
    ClassDB::bind_method(D_METHOD("get_island_bounds", "island_idx"), &VFXUVEditor::get_island_bounds);
    ClassDB::bind_method(D_METHOD("pack_islands", "padding"), &VFXUVEditor::pack_islands, DEFVAL(0.01f));

    ClassDB::bind_method(D_METHOD("align_selected_left"), &VFXUVEditor::align_selected_left);
    ClassDB::bind_method(D_METHOD("align_selected_right"), &VFXUVEditor::align_selected_right);
    ClassDB::bind_method(D_METHOD("align_selected_top"), &VFXUVEditor::align_selected_top);
    ClassDB::bind_method(D_METHOD("align_selected_bottom"), &VFXUVEditor::align_selected_bottom);
    ClassDB::bind_method(D_METHOD("align_selected_center_horizontal"), &VFXUVEditor::align_selected_center_horizontal);
    ClassDB::bind_method(D_METHOD("align_selected_center_vertical"), &VFXUVEditor::align_selected_center_vertical);
    ClassDB::bind_method(D_METHOD("distribute_selected_horizontal"), &VFXUVEditor::distribute_selected_horizontal);
    ClassDB::bind_method(D_METHOD("distribute_selected_vertical"), &VFXUVEditor::distribute_selected_vertical);

    ClassDB::bind_method(D_METHOD("get_selection_bounds"), &VFXUVEditor::get_selection_bounds);
    ClassDB::bind_method(D_METHOD("get_total_bounds"), &VFXUVEditor::get_total_bounds);
    ClassDB::bind_method(D_METHOD("normalize_uvs"), &VFXUVEditor::normalize_uvs);
    ClassDB::bind_method(D_METHOD("snap_selected_to_grid", "grid_size"), &VFXUVEditor::snap_selected_to_grid);
    ClassDB::bind_method(D_METHOD("copy_layer", "from_layer", "to_layer"), &VFXUVEditor::copy_layer);

    ClassDB::bind_integer_constant(get_class_static(), "", "UV_SELECT_VERTEX", UV_SELECT_VERTEX);
    ClassDB::bind_integer_constant(get_class_static(), "", "UV_SELECT_EDGE", UV_SELECT_EDGE);
    ClassDB::bind_integer_constant(get_class_static(), "", "UV_SELECT_FACE", UV_SELECT_FACE);
    ClassDB::bind_integer_constant(get_class_static(), "", "UV_SELECT_ISLAND", UV_SELECT_ISLAND);
}

VFXUVEditor::VFXUVEditor() {}
VFXUVEditor::~VFXUVEditor() {}

void VFXUVEditor::set_mesh(const Ref<VFXMesh>& p_mesh) {
    mesh = p_mesh;
    islands_dirty = true;
    _ensure_selection_size();
}

Ref<VFXMesh> VFXUVEditor::get_mesh() const { return mesh; }

void VFXUVEditor::set_active_layer(int layer) {
    active_layer = layer;
    islands_dirty = true;
    _ensure_selection_size();
}

int VFXUVEditor::get_active_layer() const { return active_layer; }

void VFXUVEditor::_ensure_selection_size() {
    if (mesh.is_null()) return;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return;
    int n = mesh->uv_layers[active_layer].coords.size();
    if ((int)sel_verts.size() != n) sel_verts.resize(n, false);
    int fc = mesh->get_face_count();
    if ((int)sel_faces.size() != fc) sel_faces.resize(fc, false);
}

void VFXUVEditor::_rebuild_islands() {
    if (mesh.is_null()) return;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return;
    const auto& layer = mesh->uv_layers[active_layer];

    int fc = mesh->get_face_count();
    island_map.assign(fc, -1);
    island_bounds.clear();

    std::unordered_map<uint64_t, std::vector<int>> edge_to_faces;
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || !face->halfedge) continue;
        if (face->id >= layer.face_corners.size()) continue;
        const auto& corners = layer.face_corners[face->id];
        int n = corners.size();
        for (int i = 0; i < n; i++) {
            int a = corners[i];
            int b = corners[(i + 1) % n];
            uint64_t key = ((uint64_t)std::min(a, b) << 32) | (uint64_t)std::max(a, b);
            edge_to_faces[key].push_back((int)face->id);
        }
    }

    int island_id = 0;
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || !face->halfedge) continue;
        int fid = (int)face->id;
        if (island_map[fid] >= 0) continue;

        std::stack<int> stack;
        stack.push(fid);
        island_map[fid] = island_id;
        Rect2 bounds;
        bool first = true;

        while (!stack.empty()) {
            int cf = stack.top(); stack.pop();
            if (cf >= (int)layer.face_corners.size()) continue;
            const auto& corners = layer.face_corners[cf];
            for (int cidx : corners) {
                if (cidx >= 0 && cidx < (int)layer.coords.size()) {
                    Vector2 p = layer.coords[cidx];
                    if (first) { bounds = Rect2(p, Vector2()); first = false; }
                    else bounds = bounds.expand(p);
                }
            }
            const auto& cc = layer.face_corners[cf];
            int cn = cc.size();
            for (int i = 0; i < cn; i++) {
                int a = cc[i];
                int b = cc[(i + 1) % cn];
                uint64_t key = ((uint64_t)std::min(a, b) << 32) | (uint64_t)std::max(a, b);
                auto it = edge_to_faces.find(key);
                if (it != edge_to_faces.end()) {
                    for (int nf : it->second) {
                        if (nf != cf && island_map[nf] < 0) {
                            island_map[nf] = island_id;
                            stack.push(nf);
                        }
                    }
                }
            }
        }
        island_bounds.push_back(bounds);
        island_id++;
    }
    islands_dirty = false;
}

void VFXUVEditor::_deselect_all() {
    std::fill(sel_verts.begin(), sel_verts.end(), false);
    std::fill(sel_faces.begin(), sel_faces.end(), false);
}

// ============================================================================
// QUERY
// ============================================================================
int VFXUVEditor::get_uv_vert_count() const {
    if (mesh.is_null()) return 0;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return 0;
    return mesh->uv_layers[active_layer].coords.size();
}

Vector2 VFXUVEditor::get_uv_vert(int uv_idx) const {
    if (mesh.is_null()) return Vector2();
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return Vector2();
    const auto& layer = mesh->uv_layers[active_layer];
    if (uv_idx < 0 || uv_idx >= (int)layer.coords.size()) return Vector2();
    return layer.coords[uv_idx];
}

void VFXUVEditor::set_uv_vert(int uv_idx, const Vector2& uv) {
    if (mesh.is_null()) return;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return;
    auto& layer = mesh->uv_layers[active_layer];
    if (uv_idx < 0 || uv_idx >= (int)layer.coords.size()) return;
    layer.coords[uv_idx] = uv;
}

PackedVector2Array VFXUVEditor::get_face_uv_polygon(int face_idx) const {
    PackedVector2Array arr;
    if (mesh.is_null()) return arr;
    arr = mesh->get_face_uvs(active_layer, face_idx);
    return arr;
}

PackedInt32Array VFXUVEditor::get_face_uv_indices(int face_idx) const {
    PackedInt32Array arr;
    if (mesh.is_null()) return arr;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return arr;
    if (face_idx < 0 || face_idx >= (int)mesh->uv_layers[active_layer].face_corners.size()) return arr;
    for (int idx : mesh->uv_layers[active_layer].face_corners[face_idx]) arr.push_back(idx);
    return arr;
}

PackedInt32Array VFXUVEditor::get_face_uv_edges(int face_idx) const {
    PackedInt32Array arr;
    if (mesh.is_null()) return arr;
    PackedInt32Array idxs = get_face_uv_indices(face_idx);
    int n = idxs.size();
    for (int i = 0; i < n; i++) {
        arr.push_back(idxs[i]);
        arr.push_back(idxs[(i + 1) % n]);
    }
    return arr;
}

PackedInt32Array VFXUVEditor::get_uv_vert_linked_faces(int uv_idx) const {
    PackedInt32Array arr;
    if (mesh.is_null()) return arr;
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return arr;
    const auto& layer = mesh->uv_layers[active_layer];
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        for (int c : layer.face_corners[face->id]) {
            if (c == uv_idx) { arr.push_back((int)face->id); break; }
        }
    }
    return arr;
}

// ============================================================================
// SELECTION
// ============================================================================
void VFXUVEditor::set_select_mode(int mode) {}
int VFXUVEditor::get_select_mode() const { return UV_SELECT_VERTEX; }

void VFXUVEditor::select_all() {
    _ensure_selection_size();
    std::fill(sel_verts.begin(), sel_verts.end(), true);
    std::fill(sel_faces.begin(), sel_faces.end(), true);
}

void VFXUVEditor::deselect_all() {
    _ensure_selection_size();
    _deselect_all();
}

void VFXUVEditor::invert_selection() {
    _ensure_selection_size();
    for (size_t i = 0; i < sel_verts.size(); i++) sel_verts[i] = !sel_verts[i];
    for (size_t i = 0; i < sel_faces.size(); i++) sel_faces[i] = !sel_faces[i];
}

void VFXUVEditor::select_at(const Vector2& uv_pos, float radius, bool add) {
    if (mesh.is_null()) return;
    _ensure_selection_size();
    if (!add) _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    float r2 = radius * radius;
    for (int i = 0; i < (int)layer.coords.size(); i++) {
        if (layer.coords[i].distance_squared_to(uv_pos) < r2) sel_verts[i] = true;
    }
}

void VFXUVEditor::select_edge_at(const Vector2& uv_pos, float radius, bool add) {
    if (mesh.is_null()) return;
    _ensure_selection_size();
    if (!add) _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        const auto& corners = layer.face_corners[face->id];
        int n = corners.size();
        for (int i = 0; i < n; i++) {
            Vector2 a = layer.coords[corners[i]];
            Vector2 b = layer.coords[corners[(i + 1) % n]];
            if (_point_segment_dist(uv_pos, a, b) < radius) {
                sel_verts[corners[i]] = true;
                sel_verts[corners[(i + 1) % n]] = true;
            }
        }
    }
}

void VFXUVEditor::select_face_at(const Vector2& uv_pos, bool add) {
    if (mesh.is_null()) return;
    _ensure_selection_size();
    if (!add) _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        PackedVector2Array poly;
        for (int c : layer.face_corners[face->id]) poly.push_back(layer.coords[c]);
        if (_point_in_polygon(uv_pos, poly)) {
            sel_faces[face->id] = true;
            for (int c : layer.face_corners[face->id]) sel_verts[c] = true;
        }
    }
}

void VFXUVEditor::select_island_at(const Vector2& uv_pos) {
    if (mesh.is_null()) return;
    if (islands_dirty) _rebuild_islands();
    _ensure_selection_size();
    _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= (int)island_map.size()) continue;
        PackedVector2Array poly;
        for (int c : layer.face_corners[face->id]) poly.push_back(layer.coords[c]);
        if (_point_in_polygon(uv_pos, poly)) {
            int iid = island_map[face->id];
            for (int i = 0; i < (int)island_map.size(); i++) {
                if (island_map[i] == iid) {
                    sel_faces[i] = true;
                    if (i < (int)layer.face_corners.size()) {
                        for (int c : layer.face_corners[i]) sel_verts[c] = true;
                    }
                }
            }
            return;
        }
    }
}

void VFXUVEditor::select_box(const Rect2& box, bool add) {
    if (mesh.is_null()) return;
    _ensure_selection_size();
    if (!add) _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)layer.coords.size(); i++) {
        if (box.has_point(layer.coords[i])) sel_verts[i] = true;
    }
}

void VFXUVEditor::select_island_box(const Rect2& box, bool add) {
    if (mesh.is_null()) return;
    if (islands_dirty) _rebuild_islands();
    _ensure_selection_size();
    if (!add) _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= (int)island_map.size()) continue;
        bool inside = true;
        for (int c : layer.face_corners[face->id]) {
            if (!box.has_point(layer.coords[c])) { inside = false; break; }
        }
        if (inside) {
            int iid = island_map[face->id];
            for (int i = 0; i < (int)island_map.size(); i++) {
                if (island_map[i] == iid) {
                    sel_faces[i] = true;
                    if (i < (int)layer.face_corners.size()) {
                        for (int c : layer.face_corners[i]) sel_verts[c] = true;
                    }
                }
            }
        }
    }
}

void VFXUVEditor::select_island(int island_idx) {
    if (mesh.is_null()) return;
    if (islands_dirty) _rebuild_islands();
    _ensure_selection_size();
    _deselect_all();
    const auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)island_map.size(); i++) {
        if (island_map[i] == island_idx) {
            sel_faces[i] = true;
            if (i < (int)layer.face_corners.size()) {
                for (int c : layer.face_corners[i]) sel_verts[c] = true;
            }
        }
    }
}

void VFXUVEditor::deselect_island(int island_idx) {
    if (mesh.is_null()) return;
    if (islands_dirty) _rebuild_islands();
    const auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)island_map.size(); i++) {
        if (island_map[i] == island_idx) {
            sel_faces[i] = false;
            if (i < (int)layer.face_corners.size()) {
                for (int c : layer.face_corners[i]) sel_verts[c] = false;
            }
        }
    }
}

PackedInt32Array VFXUVEditor::get_selected_verts() const {
    PackedInt32Array arr;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) arr.push_back(i);
    return arr;
}

PackedInt32Array VFXUVEditor::get_selected_faces() const {
    PackedInt32Array arr;
    for (int i = 0; i < (int)sel_faces.size(); i++) if (sel_faces[i]) arr.push_back(i);
    return arr;
}

bool VFXUVEditor::is_uv_selected(int uv_idx) const {
    if (uv_idx < 0 || uv_idx >= (int)sel_verts.size()) return false;
    return sel_verts[uv_idx];
}

bool VFXUVEditor::is_face_selected(int face_idx) const {
    if (face_idx < 0 || face_idx >= (int)sel_faces.size()) return false;
    return sel_faces[face_idx];
}

// ============================================================================
// TRANSFORM
// ============================================================================
void VFXUVEditor::translate_selected(const Vector2& delta) {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i] += delta;
    islands_dirty = true;
}

void VFXUVEditor::rotate_selected(float angle_rad, const Vector2& pivot) {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float c = cosf(angle_rad), s = sinf(angle_rad);
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) {
            Vector2 local = layer.coords[i] - pivot;
            layer.coords[i] = Vector2(local.x * c - local.y * s, local.x * s + local.y * c) + pivot;
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::scale_selected(const Vector2& scale, const Vector2& pivot) {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) {
            Vector2 local = layer.coords[i] - pivot;
            layer.coords[i] = Vector2(local.x * scale.x, local.y * scale.y) + pivot;
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::mirror_selected_horizontal(float pivot_x) {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) layer.coords[i].x = pivot_x * 2.0f - layer.coords[i].x;
    }
    islands_dirty = true;
}

void VFXUVEditor::mirror_selected_vertical(float pivot_y) {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) layer.coords[i].y = pivot_y * 2.0f - layer.coords[i].y;
    }
    islands_dirty = true;
}

// ============================================================================
// PROJECTIONS
// ============================================================================
void VFXUVEditor::project_planar(const Vector3& normal) {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    if (active_layer < 0 || active_layer >= mesh->get_uv_layer_count()) return;
    auto& layer = mesh->uv_layers[active_layer];

    Vector3 n = normal.normalized();
    Vector3 up = fabs(n.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 tangent = up.cross(n).normalized();
    Vector3 bitangent = n.cross(tangent);

    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        std::vector<VFXMesh::Vertex*> fverts;
        mesh->get_face_vertices((int)face->id, fverts);
        const auto& corners = layer.face_corners[face->id];
        for (size_t i = 0; i < fverts.size() && i < corners.size(); i++) {
            Vector3 p = fverts[i]->position;
            Vector2 uv(Vector3(tangent).dot(p), Vector3(bitangent).dot(p));
            int ci = corners[i];
            if (ci >= 0 && ci < (int)layer.coords.size()) layer.coords[ci] = uv;
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::project_cylindrical(const Vector3& axis) {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    auto& layer = mesh->uv_layers[active_layer];
    Vector3 a = axis.normalized();
    Vector3 up = fabs(a.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = a.cross(up).normalized();
    up = right.cross(a).normalized();

    float min_y = 1e30f, max_y = -1e30f;
    for (auto* v : mesh->get_vertices()) {
        if (!v->deleted) {
            float h = Vector3(v->position).dot(a);
            min_y = fmin(min_y, h);
            max_y = fmax(max_y, h);
        }
    }
    float height = max_y - min_y;
    if (height < 0.0001f) height = 1.0f;

    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        std::vector<VFXMesh::Vertex*> fverts;
        mesh->get_face_vertices((int)face->id, fverts);
        const auto& corners = layer.face_corners[face->id];
        for (size_t i = 0; i < fverts.size() && i < corners.size(); i++) {
            Vector3 p = fverts[i]->position;
            Vector3 local = p - a * Vector3(p).dot(a);
            float angle = atan2(Vector3(local).dot(up), Vector3(local).dot(right));
            float u = (angle / (2.0f * 3.14159265f)) + 0.5f;
            float v = (Vector3(p).dot(a) - min_y) / height;
            int ci = corners[i];
            if (ci >= 0 && ci < (int)layer.coords.size()) layer.coords[ci] = Vector2(u, v);
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::project_spherical(const Vector3& axis) {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    auto& layer = mesh->uv_layers[active_layer];
    Vector3 a = axis.normalized();
    Vector3 up = fabs(a.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = a.cross(up).normalized();
    up = right.cross(a).normalized();

    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        std::vector<VFXMesh::Vertex*> fverts;
        mesh->get_face_vertices((int)face->id, fverts);
        const auto& corners = layer.face_corners[face->id];
        for (size_t i = 0; i < fverts.size() && i < corners.size(); i++) {
            Vector3 p = fverts[i]->position.normalized();
            float angle = atan2(Vector3(p).dot(up), Vector3(p).dot(right));
            float u = (angle / (2.0f * 3.14159265f)) + 0.5f;
            float v = asin(vfx::clampf(Vector3(p).dot(a), -1.0f, 1.0f)) / 3.14159265f + 0.5f;
            int ci = corners[i];
            if (ci >= 0 && ci < (int)layer.coords.size()) layer.coords[ci] = Vector2(u, v);
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::project_box() {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    auto& layer = mesh->uv_layers[active_layer];

    vfx::AABB bounds;
    for (auto* v : mesh->get_vertices()) if (!v->deleted) bounds.expand(v->position);
    Vector3 size = bounds.max - bounds.min;
    if (size.x < 0.0001f) size.x = 1.0f;
    if (size.y < 0.0001f) size.y = 1.0f;
    if (size.z < 0.0001f) size.z = 1.0f;

    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        Vector3 fn = mesh->get_face_normal((int)face->id).abs();
        int dom = (fn.x > fn.y && fn.x > fn.z) ? 0 : (fn.y > fn.z) ? 1 : 2;

        std::vector<VFXMesh::Vertex*> fverts;
        mesh->get_face_vertices((int)face->id, fverts);
        const auto& corners = layer.face_corners[face->id];
        for (size_t i = 0; i < fverts.size() && i < corners.size(); i++) {
            Vector3 p = fverts[i]->position;
            Vector2 uv;
            if (dom == 0) uv = Vector2((p.z - bounds.min.z) / size.z, (p.y - bounds.min.y) / size.y);
            else if (dom == 1) uv = Vector2((p.x - bounds.min.x) / size.x, (p.z - bounds.min.z) / size.z);
            else uv = Vector2((p.x - bounds.min.x) / size.x, (p.y - bounds.min.y) / size.y);
            int ci = corners[i];
            if (ci >= 0 && ci < (int)layer.coords.size()) layer.coords[ci] = uv;
        }
    }
    islands_dirty = true;
}

// ============================================================================
// UNWRAP
// ============================================================================
void VFXUVEditor::unwrap_selected_faces() {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    auto& layer = mesh->uv_layers[active_layer];

    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= (int)sel_faces.size()) continue;
        if (!sel_faces[face->id]) continue;

        std::vector<VFXMesh::Vertex*> fverts;
        mesh->get_face_vertices((int)face->id, fverts);
        if (fverts.size() < 3) continue;

        Vector3 e1 = fverts[1]->position - fverts[0]->position;
        Vector3 e2 = fverts[2]->position - fverts[0]->position;
        Vector3 n = e1.cross(e2).normalized();
        Vector3 up = fabs(n.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
        Vector3 tangent = up.cross(n).normalized();
        Vector3 bitangent = n.cross(tangent);

        const auto& corners = layer.face_corners[face->id];
        for (size_t i = 0; i < fverts.size() && i < corners.size(); i++) {
            Vector3 local = fverts[i]->position - fverts[0]->position;
            Vector2 uv(Vector3(tangent).dot(local), Vector3(bitangent).dot(local));
            int ci = corners[i];
            if (ci >= 0 && ci < (int)layer.coords.size()) layer.coords[ci] = uv;
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::unwrap_island(int island_idx) {
    if (islands_dirty) _rebuild_islands();
    _deselect_all();
    for (int i = 0; i < (int)island_map.size(); i++) {
        if (island_map[i] == island_idx) sel_faces[i] = true;
    }
    unwrap_selected_faces();
}

// ============================================================================
// SEAM & WELD
// ============================================================================
void VFXUVEditor::split_selected_verts() {
    if (mesh.is_null()) return;
    mesh->sync_uv_layers();
    auto& layer = mesh->uv_layers[active_layer];

    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (!sel_verts[i]) continue;
        // Find all faces using this UV vert
        for (auto* face : mesh->get_faces()) {
            if (face->deleted || face->id >= layer.face_corners.size()) continue;
            auto& corners = layer.face_corners[face->id];
            for (size_t c = 0; c < corners.size(); c++) {
                if (corners[c] == i) {
                    // Create unique copy for this corner
                    int new_idx = layer.coords.size();
                    layer.coords.push_back(layer.coords[i]);
                    corners[c] = new_idx;
                }
            }
        }
    }
    islands_dirty = true;
    _ensure_selection_size();
}

void VFXUVEditor::weld_selected_verts() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];

    Vector2 avg;
    int count = 0;
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) { avg += layer.coords[i]; count++; }
    }
    if (count == 0) return;
    avg /= count;

    // Find the first selected index to use as master
    int master = -1;
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) { master = i; break; }
    }
    if (master < 0) return;
    layer.coords[master] = avg;

    // Remap all selected verts to master
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= layer.face_corners.size()) continue;
        auto& corners = layer.face_corners[face->id];
        for (size_t c = 0; c < corners.size(); c++) {
            if (corners[c] >= 0 && corners[c] < (int)sel_verts.size() && sel_verts[corners[c]]) {
                corners[c] = master;
            }
        }
    }
    islands_dirty = true;
    _ensure_selection_size();
}

void VFXUVEditor::stitch_selected() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    if (islands_dirty) _rebuild_islands();

    // For each selected face, find boundary edges and try to match with neighbor
    // Simplified: average matching boundary UVs
    for (auto* face : mesh->get_faces()) {
        if (face->deleted || face->id >= (int)sel_faces.size() || !sel_faces[face->id]) continue;
        if (face->id >= layer.face_corners.size()) continue;
        const auto& corners = layer.face_corners[face->id];
        int n = corners.size();
        for (int i = 0; i < n; i++) {
            int a = corners[i];
            int b = corners[(i + 1) % n];
            // Find twin face
            for (auto* of : mesh->get_faces()) {
                if (of->deleted || of == face || of->id >= layer.face_corners.size()) continue;
                const auto& oc = layer.face_corners[of->id];
                int on = oc.size();
                for (int j = 0; j < on; j++) {
                    if (oc[j] == b && oc[(j + 1) % on] == a) {
                        // Shared edge - average the two
                        Vector2 mid_a = (layer.coords[a] + layer.coords[oc[(j + 1) % on]]) * 0.5f;
                        Vector2 mid_b = (layer.coords[b] + layer.coords[oc[j]]) * 0.5f;
                        layer.coords[a] = mid_a;
                        layer.coords[oc[(j + 1) % on]] = mid_a;
                        layer.coords[b] = mid_b;
                        layer.coords[oc[j]] = mid_b;
                    }
                }
            }
        }
    }
    islands_dirty = true;
}

// ============================================================================
// ISLANDS & PACKING
// ============================================================================
int VFXUVEditor::get_island_count() const {
    if (islands_dirty) const_cast<VFXUVEditor*>(this)->_rebuild_islands();
    return (int)island_bounds.size();
}

PackedInt32Array VFXUVEditor::get_island_faces(int island_idx) const {
    PackedInt32Array arr;
    if (islands_dirty) const_cast<VFXUVEditor*>(this)->_rebuild_islands();
    for (int i = 0; i < (int)island_map.size(); i++) {
        if (island_map[i] == island_idx) arr.push_back(i);
    }
    return arr;
}

Rect2 VFXUVEditor::get_island_bounds(int island_idx) const {
    if (islands_dirty) const_cast<VFXUVEditor*>(this)->_rebuild_islands();
    if (island_idx >= 0 && island_idx < (int)island_bounds.size()) return island_bounds[island_idx];
    return Rect2();
}

void VFXUVEditor::pack_islands(float padding) {
    if (mesh.is_null()) return;
    if (islands_dirty) _rebuild_islands();
    int count = get_island_count();
    if (count == 0) return;

    struct IslandData {
        int idx;
        Rect2 bounds;
        float area;
        Vector2 center;
    };
    std::vector<IslandData> islands;
    for (int i = 0; i < count; i++) {
        IslandData d;
        d.idx = i;
        d.bounds = get_island_faces(i).size() > 0 ? get_island_bounds(i) : Rect2();
        d.center = d.bounds.get_center();
        d.area = d.bounds.size.x * d.bounds.size.y;
        islands.push_back(d);
    }
    std::sort(islands.begin(), islands.end(), [](const IslandData& a, const IslandData& b) {
        return a.area > b.area;
    });

    // Simple shelf packing
    float shelf_x = 0.0f;
    float shelf_y = 0.0f;
    float shelf_h = 0.0f;
    float max_w = 1.0f;

    auto& layer = mesh->uv_layers[active_layer];

    for (const auto& isle : islands) {
        float w = isle.bounds.size.x + padding * 2.0f;
        float h = isle.bounds.size.y + padding * 2.0f;

        if (shelf_x + w > max_w && shelf_x > 0.0f) {
            shelf_x = 0.0f;
            shelf_y += shelf_h;
            shelf_h = 0.0f;
        }

        Vector2 offset = Vector2(shelf_x + padding, shelf_y + padding) - isle.bounds.position;
        PackedInt32Array faces = get_island_faces(isle.idx);
        for (int fi : faces) {
            if (fi < 0 || fi >= (int)layer.face_corners.size()) continue;
            for (int& c : layer.face_corners[fi]) {
                if (c >= 0 && c < (int)layer.coords.size()) layer.coords[c] += offset;
            }
        }

        shelf_x += w;
        shelf_h = fmax(shelf_h, h);
    }
    islands_dirty = true;
}

// ============================================================================
// ALIGN / DISTRIBUTE
// ============================================================================
void VFXUVEditor::align_selected_left() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float x = 1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) x = fmin(x, layer.coords[i].x);
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].x = x;
    islands_dirty = true;
}

void VFXUVEditor::align_selected_right() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float x = -1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) x = fmax(x, layer.coords[i].x);
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].x = x;
    islands_dirty = true;
}

void VFXUVEditor::align_selected_top() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float y = -1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) y = fmax(y, layer.coords[i].y);
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].y = y;
    islands_dirty = true;
}

void VFXUVEditor::align_selected_bottom() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float y = 1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) y = fmin(y, layer.coords[i].y);
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].y = y;
    islands_dirty = true;
}

void VFXUVEditor::align_selected_center_horizontal() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float min_x = 1e30f, max_x = -1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) {
        min_x = fmin(min_x, layer.coords[i].x);
        max_x = fmax(max_x, layer.coords[i].x);
    }
    float cx = (min_x + max_x) * 0.5f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].x = cx;
    islands_dirty = true;
}

void VFXUVEditor::align_selected_center_vertical() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    float min_y = 1e30f, max_y = -1e30f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) {
        min_y = fmin(min_y, layer.coords[i].y);
        max_y = fmax(max_y, layer.coords[i].y);
    }
    float cy = (min_y + max_y) * 0.5f;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) layer.coords[i].y = cy;
    islands_dirty = true;
}

void VFXUVEditor::distribute_selected_horizontal() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    std::vector<std::pair<float, int>> items;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) items.push_back({layer.coords[i].x, i});
    if (items.size() < 3) return;
    std::sort(items.begin(), items.end());
    float min_x = items.front().first;
    float max_x = items.back().first;
    float step = (max_x - min_x) / (items.size() - 1);
    for (size_t i = 0; i < items.size(); i++) layer.coords[items[i].second].x = min_x + step * i;
    islands_dirty = true;
}

void VFXUVEditor::distribute_selected_vertical() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    std::vector<std::pair<float, int>> items;
    for (int i = 0; i < (int)sel_verts.size(); i++) if (sel_verts[i]) items.push_back({layer.coords[i].y, i});
    if (items.size() < 3) return;
    std::sort(items.begin(), items.end());
    float min_y = items.front().first;
    float max_y = items.back().first;
    float step = (max_y - min_y) / (items.size() - 1);
    for (size_t i = 0; i < items.size(); i++) layer.coords[items[i].second].y = min_y + step * i;
    islands_dirty = true;
}

// ============================================================================
// UTILS
// ============================================================================
Rect2 VFXUVEditor::get_selection_bounds() const {
    if (mesh.is_null()) return Rect2();
    const auto& layer = mesh->uv_layers[active_layer];
    Rect2 bounds;
    bool first = true;
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) {
            if (first) { bounds = Rect2(layer.coords[i], Vector2()); first = false; }
            else bounds = bounds.expand(layer.coords[i]);
        }
    }
    return bounds;
}

Rect2 VFXUVEditor::get_total_bounds() const {
    if (mesh.is_null()) return Rect2();
    const auto& layer = mesh->uv_layers[active_layer];
    Rect2 bounds;
    bool first = true;
    for (const auto& c : layer.coords) {
        if (first) { bounds = Rect2(c, Vector2()); first = false; }
        else bounds = bounds.expand(c);
    }
    return bounds;
}

void VFXUVEditor::normalize_uvs() {
    if (mesh.is_null()) return;
    auto& layer = mesh->uv_layers[active_layer];
    Rect2 b = get_total_bounds();
    if (b.size.x < 0.0001f || b.size.y < 0.0001f) return;
    for (auto& c : layer.coords) {
        c.x = (c.x - b.position.x) / b.size.x;
        c.y = (c.y - b.position.y) / b.size.y;
    }
    islands_dirty = true;
}

void VFXUVEditor::snap_selected_to_grid(float grid_size) {
    if (mesh.is_null() || grid_size < 0.0001f) return;
    auto& layer = mesh->uv_layers[active_layer];
    for (int i = 0; i < (int)sel_verts.size(); i++) {
        if (sel_verts[i]) {
            layer.coords[i].x = roundf(layer.coords[i].x / grid_size) * grid_size;
            layer.coords[i].y = roundf(layer.coords[i].y / grid_size) * grid_size;
        }
    }
    islands_dirty = true;
}

void VFXUVEditor::copy_layer(int from_layer, int to_layer) {
    if (mesh.is_null()) return;
    if (from_layer < 0 || from_layer >= mesh->get_uv_layer_count()) return;
    if (to_layer < 0 || to_layer >= mesh->get_uv_layer_count()) return;
    mesh->uv_layers[to_layer] = mesh->uv_layers[from_layer];
    islands_dirty = true;
}

Color VFXUVEditor::get_uv_vert_color(int uv_idx) const {
    if (uv_idx < 0 || uv_idx >= (int)sel_verts.size()) return Color(0.7f, 0.7f, 0.7f);
    return sel_verts[uv_idx] ? Color(1.0f, 0.5f, 0.0f) : Color(0.7f, 0.7f, 0.7f);
}

Color VFXUVEditor::get_face_wire_color(int face_idx) const {
    if (face_idx < 0 || face_idx >= (int)sel_faces.size()) return Color(0.4f, 0.4f, 0.4f);
    return sel_faces[face_idx] ? Color(1.0f, 0.5f, 0.0f) : Color(0.4f, 0.4f, 0.4f);
}

// ============================================================================
// STATIC HELPERS
// ============================================================================
float VFXUVEditor::_point_segment_dist(const Vector2& p, const Vector2& a, const Vector2& b) {
    Vector2 ab = b - a;
    float len2 = ab.length_squared();
    if (len2 < 0.0001f) return p.distance_to(a);
    float t = vfx::clampf((p - a).dot(ab) / len2, 0.0f, 1.0f);
    return p.distance_to(a + ab * t);
}

bool VFXUVEditor::_point_in_polygon(const Vector2& p, const PackedVector2Array& poly) {
    bool inside = false;
    int n = poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
            (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x))
            inside = !inside;
    }
    return inside;
}
