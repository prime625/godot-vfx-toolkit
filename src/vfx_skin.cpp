#include "vfx_skin.h"\n#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>

using namespace godot;

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
    ClassDB::bind_method(D_METHOD("set_brush_add_mode", "add"), &VFXSkin::set_brush_add_mode);
    ClassDB::bind_method(D_METHOD("get_brush_add_mode"), &VFXSkin::get_brush_add_mode);
    ClassDB::bind_method(D_METHOD("set_brush_normalize", "n"), &VFXSkin::set_brush_normalize);
    ClassDB::bind_method(D_METHOD("get_brush_normalize"), &VFXSkin::get_brush_normalize);

    ClassDB::bind_method(D_METHOD("paint_at", "world_pos", "delta_time"), &VFXSkin::paint_at);
    ClassDB::bind_method(D_METHOD("flood_fill_bone", "bone_idx", "weight"), &VFXSkin::flood_fill_bone);
    ClassDB::bind_method(D_METHOD("auto_weight_from_bones", "max_bones"), &VFXSkin::auto_weight_from_bones, DEFVAL(4));
    ClassDB::bind_method(D_METHOD("clear_weights"), &VFXSkin::clear_weights);
    ClassDB::bind_method(D_METHOD("get_vertex_bone_weight", "vidx", "bone_idx"), &VFXSkin::get_vertex_bone_weight);
    ClassDB::bind_method(D_METHOD("get_weight_visualization", "bone_idx"), &VFXSkin::get_weight_visualization);
    ClassDB::bind_method(D_METHOD("compute_skinned_positions"), &VFXSkin::compute_skinned_positions);
    ClassDB::bind_method(D_METHOD("bake_vertex_animation", "frame_count", "fps"), &VFXSkin::bake_vertex_animation);
}

VFXSkin::VFXSkin() {}
VFXSkin::~VFXSkin() {}

void VFXSkin::set_mesh(const Ref<VFXMesh>& p_mesh) { mesh = p_mesh; }
Ref<VFXMesh> VFXSkin::get_mesh() const { return mesh; }
void VFXSkin::set_skeleton(const Ref<VFXSkeleton>& p_sk) { skeleton = p_sk; }
Ref<VFXSkeleton> VFXSkin::get_skeleton() const { return skeleton; }

void VFXSkin::set_brush_radius(float r) { brush.radius = r; }
float VFXSkin::get_brush_radius() const { return brush.radius; }
void VFXSkin::set_brush_strength(float s) { brush.strength = s; }
float VFXSkin::get_brush_strength() const { return brush.strength; }
void VFXSkin::set_brush_falloff(int type) { brush.falloff = type; }
int VFXSkin::get_brush_falloff() const { return brush.falloff; }
void VFXSkin::set_brush_bone(int idx) { brush.bone_index = idx; }
int VFXSkin::get_brush_bone() const { return brush.bone_index; }
void VFXSkin::set_brush_add_mode(bool add) { brush.add_mode = add; }
bool VFXSkin::get_brush_add_mode() const { return brush.add_mode; }
void VFXSkin::set_brush_normalize(bool n) { brush.normalize = n; }
bool VFXSkin::get_brush_normalize() const { return brush.normalize; }

float VFXSkin::_brush_falloff(float dist, float radius, int type) const {
    if (dist >= radius) return 0.0f;
    float t = dist / radius;
    switch (type) {
        case 0: return 1.0f - t;  // linear
        case 1: return 1.0f - t * t;  // smooth
        case 2: return cos(t * 3.14159f * 0.5f);  // spherical
        default: return 1.0f - t;
    }
}

void VFXSkin::_find_vertices_in_radius(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_distances) const {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        Vector3 pos = mesh->get_vertex_position(i);
        float d = (pos - center).length();
        if (d < radius) {
            out_indices.push_back(i);
            out_distances.push_back(d);
        }
    }
}

void VFXSkin::paint_at(const Vector3& world_pos, float delta_time) {
    if (mesh.is_null()) return;

    std::vector<int> indices;
    std::vector<float> distances;
    _find_vertices_in_radius(world_pos, brush.radius, indices, distances);

    for (size_t i = 0; i < indices.size(); i++) {
        int vidx = indices[i];
        float falloff = _brush_falloff(distances[i], brush.radius, brush.falloff);
        float delta = brush.strength * falloff * delta_time * 5.0f;

        // Get current weights
        PackedFloat32Array skin = mesh->get_skinning_data();
        int base = vidx * 8;
        int bidx[4] = {(int)skin[base+0], (int)skin[base+1], (int)skin[base+2], (int)skin[base+3]};
        float w[4] = {skin[base+4], skin[base+5], skin[base+6], skin[base+7]};

        // Find if bone already assigned
        int slot = -1;
        for (int j = 0; j < 4; j++) {
            if (bidx[j] == brush.bone_index) { slot = j; break; }
            if (bidx[j] < 0 && slot < 0) slot = j;
        }
        if (slot < 0) slot = 0;  // overwrite first if full

        if (brush.add_mode) {
            w[slot] = vfx::clampf(w[slot] + delta, 0.0f, 1.0f);
        } else {
            w[slot] = vfx::clampf(w[slot] - delta, 0.0f, 1.0f);
        }
        bidx[slot] = brush.bone_index;

        mesh->set_vertex_bones(vidx, bidx[0], bidx[1], bidx[2], bidx[3]);
        mesh->set_vertex_weights(vidx, w[0], w[1], w[2], w[3]);

        if (brush.normalize) {
            mesh->normalize_weights(vidx);
        }
    }
}

void VFXSkin::flood_fill_bone(int bone_idx, float weight) {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        mesh->set_vertex_bones(i, bone_idx, -1, -1, -1);
        mesh->set_vertex_weights(i, weight, 0, 0, 0);
    }
}

void VFXSkin::auto_weight_from_bones(int max_bones_per_vertex) {
    if (mesh.is_null() || skeleton.is_null()) return;
    if (!skeleton->get_bone_count()) return;

    skeleton->update_transforms();
    int vc = mesh->get_vertex_count();
    int bc = skeleton->get_bone_count();

    for (int vi = 0; vi < vc; vi++) {
        Vector3 pos = mesh->get_vertex_position(vi);

        // Find nearest bones
        std::vector<std::pair<float, int>> bone_dists;
        for (int bi = 0; bi < bc; bi++) {
            Vector3 bone_pos = skeleton->get_bone_model_transform(bi).get_origin();
            float d = (pos - bone_pos).length();
            bone_dists.push_back({d, bi});
        }
        std::sort(bone_dists.begin(), bone_dists.end());

        int num = std::min(max_bones_per_vertex, (int)bone_dists.size());
        int bidx[4] = {-1,-1,-1,-1};
        float w[4] = {0,0,0,0};
        float total = 0.0f;

        for (int i = 0; i < num; i++) {
            bidx[i] = bone_dists[i].second;
            // Inverse distance weighting
            w[i] = 1.0f / (bone_dists[i].first + 0.0001f);
            total += w[i];
        }

        if (total > 0.0001f) {
            for (int i = 0; i < 4; i++) w[i] /= total;
        }

        mesh->set_vertex_bones(vi, bidx[0], bidx[1], bidx[2], bidx[3]);
        mesh->set_vertex_weights(vi, w[0], w[1], w[2], w[3]);
    }
}

void VFXSkin::clear_weights() {
    if (mesh.is_null()) return;
    int vc = mesh->get_vertex_count();
    for (int i = 0; i < vc; i++) {
        mesh->set_vertex_bones(i, 0, -1, -1, -1);
        mesh->set_vertex_weights(i, 1.0f, 0, 0, 0);
    }
}

float VFXSkin::get_vertex_bone_weight(int vidx, int bone_idx) const {
    if (mesh.is_null()) return 0.0f;
    PackedFloat32Array skin = mesh->get_skinning_data();
    int base = vidx * 8;
    for (int j = 0; j < 4; j++) {
        if ((int)skin[base + j] == bone_idx) return skin[base + 4 + j];
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
        // Heat map: black -> red -> yellow -> white
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

PackedVector3Array VFXSkin::compute_skinned_positions() const {
    if (mesh.is_null() || skeleton.is_null()) return PackedVector3Array();

    skeleton->update_transforms();
    int vc = mesh->get_vertex_count();
    PackedVector3Array result;
    result.resize(vc);
    PackedFloat32Array skin = mesh->get_skinning_data();

    for (int vi = 0; vi < vc; vi++) {
        int base = vi * 8;
        Vector3 skinned = Vector3();

        for (int j = 0; j < 4; j++) {
            int bidx = (int)skin[base + j];
            float w = skin[base + 4 + j];
            if (bidx < 0 || w < 0.0001f) continue;

            Transform3D skin_mat = skeleton->get_bone_model_transform(bidx) * skeleton->get_bone_bind_pose(bidx).affine_inverse();
            skinned += (skin_mat * mesh->get_vertex_position(vi)) * w;
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
    // Per frame: vc * 3 positions + vc * 3 normals
    vat.resize(frame_count * vc * 6);

    // TODO: Sample animation curves per frame, update skeleton, compute skinned positions
    // For now, bake bind pose as frame 0
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
