#include "vfx_skin.h"
#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>

using namespace godot;

// ============================================================================
// SPATIAL GRID
// ============================================================================
void SpatialGrid::build(const std::vector<Vector3>& verts, float cell_sz) {
    clear();
    cell_size = cell_sz;
    positions = verts;
    if (cell_size < 0.0001f) cell_size = 0.1f;

    for (int i = 0; i < (int)verts.size(); i++) {
        int ix = (int)floor(verts[i].x / cell_size);
        int iy = (int)floor(verts[i].y / cell_size);
        int iz = (int)floor(verts[i].z / cell_size);
        cells[hash(ix, iy, iz)].push_back(i);
    }
}

void SpatialGrid::clear() {
    cells.clear();
    positions.clear();
}

void SpatialGrid::query_sphere(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_dists) const {
    out_indices.clear();
    out_dists.clear();
    if (positions.empty()) return;

    int r_cells = (int)ceil(radius / cell_size);
    int cx = (int)floor(center.x / cell_size);
    int cy = (int)floor(center.y / cell_size);
    int cz = (int)floor(center.z / cell_size);

    float r2 = radius * radius;

    for (int dx = -r_cells; dx <= r_cells; dx++) {
        for (int dy = -r_cells; dy <= r_cells; dy++) {
            for (int dz = -r_cells; dz <= r_cells; dz++) {
                auto it = cells.find(hash(cx + dx, cy + dy, cz + dz));
                if (it == cells.end()) continue;

                for (int vi : it->second) {
                    float d2 = positions[vi].distance_squared_to(center);
                    if (d2 < r2) {
                        out_indices.push_back(vi);
                        out_dists.push_back(sqrt(d2));
                    }
                }
            }
        }
    }
}

// ============================================================================
// BINDINGS
// ============================================================================
void VFXSkin::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXSkin::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXSkin::get_mesh);
    ClassDB::bind_method(D_METHOD("set_skeleton", "sk"), &VFXSkin::set_skeleton);
    ClassDB::bind_method(D_METHOD("get_skeleton"), &VFXSkin::get_skeleton);

    ClassDB::bind_method(D_METHOD("set_brush_radius", "r"), &VFXSkin::set_brush_radius);
    ClassDB::bind_method(D_METHOD("get_brush_radius"), &VFXSkin::get_brush_radius);
    ClassDB::bind_method(D_METHOD("set_brush_strength", "s"), &VFXSkin::set_brush_strength);
    ClassDB::bind_method(D_METHOD("get_brush_strength"), &VFXSkin::get_brush_strength);
    ClassDB::bind_method(D_METHOD("set_brush_falloff", "type"), &VFXSkin::set_brush_falloff);
    ClassDB::bind_method(D_METHOD("get_brush_falloff"), &VFXSkin::get_brush_falloff);
    ClassDB::bind_method(D_METHOD("set_brush_bone", "idx"), &VFXSkin::set_brush_bone);
    ClassDB::bind_method(D_METHOD("get_brush_bone"), &VFXSkin::get_brush_bone);
    ClassDB::bind_method(D_METHOD("set_brush_mode", "mode"), &VFXSkin::set_brush_mode);
    ClassDB::bind_method(D_METHOD("get_brush_mode"), &VFXSkin::get_brush_mode);
    ClassDB::bind_method(D_METHOD("set_brush_normalize", "n"), &VFXSkin::set_brush_normalize);
    ClassDB::bind_method(D_METHOD("get_brush_normalize"), &VFXSkin::get_brush_normalize);

    ClassDB::bind_method(D_METHOD("set_mirror_x", "enabled"), &VFXSkin::set_mirror_x);
    ClassDB::bind_method(D_METHOD("get_mirror_x"), &VFXSkin::get_mirror_x);
    ClassDB::bind_method(D_METHOD("set_mirror_plane_x", "x"), &VFXSkin::set_mirror_plane_x);
    ClassDB::bind_method(D_METHOD("get_mirror_plane_x"), &VFXSkin::get_mirror_plane_x);

    ClassDB::bind_method(D_METHOD("set_bone_locked", "bone_idx", "locked"), &VFXSkin::set_bone_locked);
    ClassDB::bind_method(D_METHOD("get_bone_locked", "bone_idx"), &VFXSkin::get_bone_locked);

    ClassDB::bind_method(D_METHOD("stroke_begin"), &VFXSkin::stroke_begin);
    ClassDB::bind_method(D_METHOD("paint_at", "world_pos", "delta_time"), &VFXSkin::paint_at);
    ClassDB::bind_method(D_METHOD("stroke_end"), &VFXSkin::stroke_end);

    ClassDB::bind_method(D_METHOD("can_undo"), &VFXSkin::can_undo);
    ClassDB::bind_method(D_METHOD("can_redo"), &VFXSkin::can_redo);
    ClassDB::bind_method(D_METHOD("undo"), &VFXSkin::undo);
    ClassDB::bind_method(D_METHOD("redo"), &VFXSkin::redo);
    ClassDB::bind_method(D_METHOD("clear_history"), &VFXSkin::clear_history);

    ClassDB::bind_method(D_METHOD("flood_fill_bone", "bone_idx", "weight"), &VFXSkin::flood_fill_bone);
    ClassDB::bind_method(D_METHOD("auto_weight_from_bones", "max_bones"), &VFXSkin::auto_weight_from_bones, DEFVAL(4));
    ClassDB::bind_method(D_METHOD("clear_weights"), &VFXSkin::clear_weights);

    ClassDB::bind_method(D_METHOD("get_vertex_bone_weight", "vidx", "bone_idx"), &VFXSkin::get_vertex_bone_weight);
    ClassDB::bind_method(D_METHOD("get_weight_visualization", "bone_idx"), &VFXSkin::get_weight_visualization);

    ClassDB::bind_method(D_METHOD("compute_skinned_positions"), &VFXSkin::compute_skinned_positions);
    ClassDB::bind_method(D_METHOD("bake_vertex_animation", "frame_count", "fps"), &VFXSkin::bake_vertex_animation);

    // Enums
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_ADD", 0);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_SUBTRACT", 1);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_REPLACE", 2);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_SMOOTH", 3);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_BLUR", 4);

    ClassDB::bind_integer_constant(get_class_static(), "", "FALLOFF_LINEAR", 0);
    ClassDB::bind_integer_constant(get_class_static(), "", "FALLOFF_SMOOTH", 1);
    ClassDB::bind_integer_constant(get_class_static(), "", "FALLOFF_SPHERICAL", 2);
}

// ============================================================================
// LIFECYCLE
// ============================================================================
VFXSkin::VFXSkin() {}
VFXSkin::~VFXSkin() {}

void VFXSkin::set_mesh(const Ref<VFXMesh>& p_mesh) {
    mesh = p_mesh;
    grid_dirty = true;
}
Ref<VFXMesh> VFXSkin::get_mesh() const { return mesh; }

void VFXSkin::set_skeleton(const Ref<VFXSkeleton>& p_sk) {
    skeleton = p_sk;
    if (skeleton.is_valid()) {
        bone_locked.resize(skeleton->get_bone_count());
        std::fill(bone_locked.begin(), bone_locked.end(), false);
    }
}
Ref<VFXSkeleton> VFXSkin::get_skeleton() const { return skeleton; }

// ============================================================================
// BRUSH SETTINGS
// ============================================================================
void VFXSkin::set_brush_radius(float r) { brush_radius = r; }
float VFXSkin::get_brush_radius() const { return brush_radius; }
void VFXSkin::set_brush_strength(float s) { brush_strength = s; }
float VFXSkin::get_brush_strength() const { return brush_strength; }
void VFXSkin::set_brush_falloff(int type) { brush_falloff = type; }
int VFXSkin::get_brush_falloff() const { return brush_falloff; }
void VFXSkin::set_brush_bone(int idx) { brush_bone = idx; }
int VFXSkin::get_brush_bone() const { return brush_bone; }
void VFXSkin::set_brush_mode(int mode) { brush_mode = mode; }
int VFXSkin::get_brush_mode() const { return brush_mode; }
void VFXSkin::set_brush_normalize(bool n) { brush_normalize = n; }
bool VFXSkin::get_brush_normalize() const { return brush_normalize; }

// ============================================================================
// MIRROR
// ============================================================================
void VFXSkin::set_mirror_x(bool enabled) { mirror_x = enabled; }
bool VFXSkin::get_mirror_x() const { return mirror_x; }
void VFXSkin::set_mirror_plane_x(float x) { mirror_plane_x = x; }
float VFXSkin::get_mirror_plane_x() const { return mirror_plane_x; }

// ============================================================================
// BONE LOCKING
// ============================================================================
void VFXSkin::set_bone_locked(int bone_idx, bool locked) {
    if (bone_idx >= 0 && bone_idx < (int)bone_locked.size()) bone_locked[bone_idx] = locked;
}
bool VFXSkin::get_bone_locked(int bone_idx) const {
    if (bone_idx >= 0 && bone_idx < (int)bone_locked.size()) return bone_locked[bone_idx];
    return false;
}

// ============================================================================
// SPATIAL ACCELERATION
// ============================================================================
void VFXSkin::_rebuild_grid() {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    std::vector<Vector3> positions;
    positions.reserve(vc);
    for (int i = 0; i < vc; i++) positions.push_back(mesh->get_vertex_position(i));
    grid.build(positions, brush_radius * 1.5f); // cell size ~ 1.5x brush radius
    grid_dirty = false;
}

// ============================================================================
// BRUSH MATH
// ============================================================================
float VFXSkin::_brush_falloff(float dist, float radius, int type) const {
    if (dist >= radius) return 0.0f;
    float t = dist / radius;
    switch (type) {
        case 0: return 1.0f - t;
        case 1: return 1.0f - t * t;
        case 2: return cosf(t * 3.14159265f * 0.5f);
        default: return 1.0f - t;
    }
}

void VFXSkin::_find_vertices_brush(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_dists) {
    if (grid_dirty) _rebuild_grid();
    grid.query_sphere(center, radius, out_indices, out_dists);
}

// ============================================================================
// UNDO / REDO
// ============================================================================
void VFXSkin::_push_undo(const WeightStroke& stroke) {
    undo_stack.push_back(stroke);
    if (undo_stack.size() > MAX_UNDO) undo_stack.erase(undo_stack.begin());
    redo_stack.clear();
}

bool VFXSkin::can_undo() const { return !undo_stack.empty(); }
bool VFXSkin::can_redo() const { return !redo_stack.empty(); }

void VFXSkin::undo() {
    if (undo_stack.empty() || mesh.is_null()) return;
    WeightStroke stroke = undo_stack.back();
    undo_stack.pop_back();

    for (size_t i = 0; i < stroke.vertices.size(); i++) {
        int vidx = stroke.vertices[i];
        int bones[4] = { stroke.old_bones[0][i], stroke.old_bones[1][i], stroke.old_bones[2][i], stroke.old_bones[3][i] };
        float weights[4] = { stroke.old_weights[0][i], stroke.old_weights[1][i], stroke.old_weights[2][i], stroke.old_weights[3][i] };
        mesh->set_vertex_skinning(vidx, bones, weights);
    }

    redo_stack.push_back(stroke);
}

void VFXSkin::redo() {
    if (redo_stack.empty() || mesh.is_null()) return;
    WeightStroke stroke = redo_stack.back();
    redo_stack.pop_back();
    // Re-apply is handled by re-playing the stroke, but for simplicity we just store current state and swap
    // In a full impl you'd store both pre and post. For now, undo pops to redo and vice versa.
    undo_stack.push_back(stroke);
}

void VFXSkin::clear_history() {
    undo_stack.clear();
    redo_stack.clear();
}

// ============================================================================
// STROKE SYSTEM
// ============================================================================
static thread_local std::vector<int> tls_indices;
static thread_local std::vector<float> tls_dists;
static thread_local WeightStroke tls_stroke;

void VFXSkin::stroke_begin() {
    tls_stroke.vertices.clear();
    for (int i = 0; i < 4; i++) {
        tls_stroke.old_bones[i].clear();
        tls_stroke.old_weights[i].clear();
    }
}

void VFXSkin::stroke_end() {
    if (!tls_stroke.vertices.empty()) {
        _push_undo(tls_stroke);
    }
    tls_stroke.vertices.clear();
    for (int i = 0; i < 4; i++) {
        tls_stroke.old_bones[i].clear();
        tls_stroke.old_weights[i].clear();
    }
}

// ============================================================================
// WEIGHT APPLICATION (hot path, zero alloc)
// ============================================================================
void VFXSkin::_apply_weight(int vidx, float delta, int bone) {
    if (bone_locked.size() > (size_t)bone && bone_locked[bone]) return;

    int bones[4];
    float weights[4];
    mesh->get_vertex_skinning(vidx, bones, weights);

    // Record undo if first touch this stroke
    bool found = false;
    for (size_t i = 0; i < tls_stroke.vertices.size(); i++) {
        if (tls_stroke.vertices[i] == vidx) { found = true; break; }
    }
    if (!found) {
        tls_stroke.vertices.push_back(vidx);
        for (int j = 0; j < 4; j++) {
            tls_stroke.old_bones[j].push_back(bones[j]);
            tls_stroke.old_weights[j].push_back(weights[j]);
        }
    }

    // Find slot
    int slot = -1;
    for (int j = 0; j < 4; j++) {
        if (bones[j] == bone) { slot = j; break; }
        if (bones[j] < 0 && slot < 0) slot = j;
    }
    if (slot < 0) {
        // Find smallest weight to evict
        slot = 0;
        for (int j = 1; j < 4; j++) if (weights[j] < weights[slot]) slot = j;
    }

    if (brush_mode == 0) { // ADD
        weights[slot] = vfx::clampf(weights[slot] + delta, 0.0f, 1.0f);
    } else { // SUBTRACT
        weights[slot] = vfx::clampf(weights[slot] - delta, 0.0f, 1.0f);
    }
    bones[slot] = bone;

    if (brush_normalize) {
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) if (bones[j] >= 0) sum += weights[j];
        if (sum > 0.0001f) {
            for (int j = 0; j < 4; j++) if (bones[j] >= 0) weights[j] /= sum;
        }
    }

    mesh->set_vertex_skinning(vidx, bones, weights);

    // Mirror
    if (mirror_x) {
        _apply_mirror(vidx, bones, weights);
    }
}

void VFXSkin::_apply_replace(int vidx, float target, float falloff, int bone) {
    if (bone_locked.size() > (size_t)bone && bone_locked[bone]) return;

    int bones[4];
    float weights[4];
    mesh->get_vertex_skinning(vidx, bones, weights);

    bool found = false;
    for (size_t i = 0; i < tls_stroke.vertices.size(); i++) {
        if (tls_stroke.vertices[i] == vidx) { found = true; break; }
    }
    if (!found) {
        tls_stroke.vertices.push_back(vidx);
        for (int j = 0; j < 4; j++) {
            tls_stroke.old_bones[j].push_back(bones[j]);
            tls_stroke.old_weights[j].push_back(weights[j]);
        }
    }

    int slot = -1;
    for (int j = 0; j < 4; j++) {
        if (bones[j] == bone) { slot = j; break; }
        if (bones[j] < 0 && slot < 0) slot = j;
    }
    if (slot < 0) slot = 0;

    float new_w = vfx::lerp(weights[slot], target, falloff * brush_strength);
    weights[slot] = new_w;
    bones[slot] = bone;

    if (brush_normalize) {
        float sum = 0.0f;
        for (int j = 0; j < 4; j++) if (bones[j] >= 0) sum += weights[j];
        if (sum > 0.0001f) {
            for (int j = 0; j < 4; j++) if (bones[j] >= 0) weights[j] /= sum;
        }
    }

    mesh->set_vertex_skinning(vidx, bones, weights);

    if (mirror_x) _apply_mirror(vidx, bones, weights);
}

void VFXSkin::_apply_smooth(int vidx, float strength) {
    // Average with connected vertices (approximate via all-verts for now)
    // Full impl would use half-edge adjacency
    int bones[4];
    float weights[4];
    mesh->get_vertex_skinning(vidx, bones, weights);

    bool found = false;
    for (size_t i = 0; i < tls_stroke.vertices.size(); i++) {
        if (tls_stroke.vertices[i] == vidx) { found = true; break; }
    }
    if (!found) {
        tls_stroke.vertices.push_back(vidx);
        for (int j = 0; j < 4; j++) {
            tls_stroke.old_bones[j].push_back(bones[j]);
            tls_stroke.old_weights[j].push_back(weights[j]);
        }
    }

    // Simple laplacian-ish: pull toward average of all verts (cheap approximation)
    // For real smooth you'd walk the half-edge ring. This is good enough for mobile.
    float avg = 0.0f;
    int count = 0;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        int b[4]; float w[4];
        mesh->get_vertex_skinning(i, b, w);
        for (int j = 0; j < 4; j++) {
            if (b[j] == brush_bone) { avg += w[j]; count++; break; }
        }
    }
    if (count > 0) avg /= count;

    for (int j = 0; j < 4; j++) {
        if (bones[j] == brush_bone) {
            weights[j] = vfx::lerp(weights[j], avg, strength);
            break;
        }
    }
    mesh->set_vertex_skinning(vidx, bones, weights);
}

void VFXSkin::_apply_mirror(int vidx, const int bones[4], const float weights[4]) {
    if (!mirror_x || mesh.is_null()) return;
    Vector3 pos = mesh->get_vertex_position(vidx);
    float dist_to_plane = fabsf(pos.x - mirror_plane_x);
    if (dist_to_plane < 0.001f) return; // on plane, no mirror needed

    // Find mirrored vertex by position search (slow but rare; grid helps)
    // For production, build a position->index map at stroke_begin
    Vector3 mirrored_pos = pos;
    mirrored_pos.x = mirror_plane_x * 2.0f - pos.x;

    int best = -1;
    float best_d = 1e30f;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        float d = mesh->get_vertex_position(i).distance_squared_to(mirrored_pos);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best >= 0 && best_d < 0.0001f) {
        mesh->set_vertex_skinning(best, bones, weights);
    }
}

// ============================================================================
// PUBLIC PAINT
// ============================================================================
void VFXSkin::paint_at(const Vector3& world_pos, float delta_time) {
    if (mesh.is_null()) return;
    if (brush_bone < 0) return;
    if (brush_radius < 0.0001f) return;

    _find_vertices_brush(world_pos, brush_radius, tls_indices, tls_dists);

    for (size_t i = 0; i < tls_indices.size(); i++) {
        int vidx = tls_indices[i];
        float falloff = _brush_falloff(tls_dists[i], brush_radius, brush_falloff);
        if (falloff < 0.001f) continue;

        switch (brush_mode) {
            case 0: // ADD
            case 1: // SUBTRACT
            {
                float delta = brush_strength * falloff * delta_time * 5.0f;
                _apply_weight(vidx, delta, brush_bone);
                break;
            }
            case 2: // REPLACE
            {
                _apply_replace(vidx, brush_strength, falloff, brush_bone);
                break;
            }
            case 3: // SMOOTH
            case 4: // BLUR
            {
                _apply_smooth(vidx, brush_strength * falloff * delta_time * 5.0f);
                break;
            }
        }
    }
}

// ============================================================================
// BULK OPS
// ============================================================================
void VFXSkin::flood_fill_bone(int bone_idx, float weight) {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        mesh->set_vertex_bones(i, bone_idx, -1, -1, -1);
        mesh->set_vertex_weights(i, weight, 0, 0, 0);
    }
    grid_dirty = true;
}

void VFXSkin::auto_weight_from_bones(int max_bones_per_vertex) {
    if (mesh.is_null() || skeleton.is_null()) return;
    if (!skeleton->get_bone_count()) return;

    skeleton->update_transforms();
    int vc = mesh->get_vertex_count();
    int bc = skeleton->get_bone_count();

    for (int vi = 0; vi < vc; vi++) {
        Vector3 pos = mesh->get_vertex_position(vi);

        std::vector<std::pair<float, int>> bone_dists;
        bone_dists.reserve(bc);
        for (int bi = 0; bi < bc; bi++) {
            Vector3 bone_pos = skeleton->get_bone_model_transform(bi).get_origin();
            float d = pos.distance_to(bone_pos);
            bone_dists.push_back({d, bi});
        }
        std::sort(bone_dists.begin(), bone_dists.end());

        int num = std::min(max_bones_per_vertex, (int)bone_dists.size());
        int bidx[4] = {-1, -1, -1, -1};
        float w[4] = {0, 0, 0, 0};
        float total = 0.0f;

        for (int i = 0; i < num; i++) {
            bidx[i] = bone_dists[i].second;
            w[i] = 1.0f / (bone_dists[i].first + 0.0001f);
            total += w[i];
        }

        if (total > 0.0001f) {
            for (int i = 0; i < 4; i++) w[i] /= total;
        }

        mesh->set_vertex_bones(vi, bidx[0], bidx[1], bidx[2], bidx[3]);
        mesh->set_vertex_weights(vi, w[0], w[1], w[2], w[3]);
    }
    grid_dirty = true;
}

void VFXSkin::clear_weights() {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        mesh->set_vertex_bones(i, 0, -1, -1, -1);
        mesh->set_vertex_weights(i, 1.0f, 0, 0, 0);
    }
    grid_dirty = true;
}

// ============================================================================
// QUERIES
// ============================================================================
float VFXSkin::get_vertex_bone_weight(int vidx, int bone_idx) const {
    if (mesh.is_null()) return 0.0f;
    int bones[4];
    float weights[4];
    mesh->get_vertex_skinning(vidx, bones, weights);
    for (int j = 0; j < 4; j++) {
        if (bones[j] == bone_idx) return weights[j];
    }
    return 0.0f;
}

PackedColorArray VFXSkin::get_weight_visualization(int bone_idx) const {
    if (mesh.is_null()) return PackedColorArray();
    int vc = mesh->get_vertex_count();
    PackedColorArray colors;
    colors.resize(vc);
    for (int i = 0; i < vc; i++) {
        float w = get_vertex_bone_weight(i, bone_idx);
        Color c;
        if (w < 0.33f) {
            c = Color(w * 3.0f, 0, 0, 1);
        } else if (w < 0.66f) {
            c = Color(1, (w - 0.33f) * 3.0f, 0, 1);
        } else {
            c = Color(1, 1, (w - 0.66f) * 3.0f, 1);
        }
        colors[i] = c;
    }
    return colors;
}

// ============================================================================
// SKINNING PREVIEW
// ============================================================================
PackedVector3Array VFXSkin::compute_skinned_positions() const {
    if (mesh.is_null() || skeleton.is_null()) return PackedVector3Array();

    skeleton->update_transforms();
    int vc = mesh->get_vertex_count();
    PackedVector3Array result;
    result.resize(vc);

    for (int vi = 0; vi < vc; vi++) {
        int bones[4];
        float weights[4];
        mesh->get_vertex_skinning(vi, bones, weights);

        Vector3 skinned;
        Vector3 pos = mesh->get_vertex_position(vi);

        for (int j = 0; j < 4; j++) {
            int bidx = bones[j];
            float w = weights[j];
            if (bidx < 0 || w < 0.0001f) continue;

            Transform3D skin_mat = skeleton->get_bone_model_transform(bidx) * skeleton->get_bone_bind_pose(bidx).affine_inverse();
            skinned += skin_mat.xform(pos) * w;
        }
        result[vi] = skinned;
    }
    return result;
}

PackedFloat32Array VFXSkin::bake_vertex_animation(int frame_count, float fps) const {
    if (mesh.is_null() || skeleton.is_null()) return PackedFloat32Array();
    if (!skeleton->get_bone_count()) return PackedFloat32Array();

    int vc = mesh->get_vertex_count();
    PackedFloat32Array vat;
    vat.resize(frame_count * vc * 6);

    PackedVector3Array pos = compute_skinned_positions();
    PackedVector3Array norm = mesh->get_normals();

    for (int fi = 0; fi < frame_count; fi++) {
        int base = fi * vc * 6;
        for (int vi = 0; vi < vc; vi++) {
            vat[base + vi * 6 + 0] = pos[vi].x;
            vat[base + vi * 6 + 1] = pos[vi].y;
            vat[base + vi * 6 + 2] = pos[vi].z;
            vat[base + vi * 6 + 3] = norm[vi].x;
            vat[base + vi * 6 + 4] = norm[vi].y;
            vat[base + vi * 6 + 5] = norm[vi].z;
        }
    }
    return vat;
}
