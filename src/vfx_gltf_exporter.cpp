#include "vfx_gltf_exporter.h"
#include "vfx_scene.h"
#include "vfx_scene_node.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <cmath>

using namespace godot;

// glTF constants
static const int GL_FLOAT = 5126;
static const int GL_UNSIGNED_SHORT = 5123;
static const int GL_UNSIGNED_INT = 5125;
static const int GL_ARRAY_BUFFER = 34962;
static const int GL_ELEMENT_ARRAY_BUFFER = 34963;
static const uint32_t GLB_MAGIC = 0x46546C67; // "glTF"
static const uint32_t GLB_VERSION = 2;
static const uint32_t GLB_CHUNK_JSON = 0x4E4F534A; // "JSON"
static const uint32_t GLB_CHUNK_BIN = 0x004E4942;  // "BIN"

void VFXGLTFExporter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("export_glb", "mesh", "skeleton", "filepath"), &VFXGLTFExporter::export_glb);
    ClassDB::bind_method(D_METHOD("export_glb_animated", "mesh", "skeleton", "animator", "clip_idx", "filepath"), &VFXGLTFExporter::export_glb_animated);
    ClassDB::bind_method(D_METHOD("export_vat_glb", "mesh", "skin", "frame_count", "fps", "filepath"), &VFXGLTFExporter::export_vat_glb);
    ClassDB::bind_method(D_METHOD("build_gltf_document", "mesh", "skeleton"), &VFXGLTFExporter::build_gltf_document);
    ClassDB::bind_method(D_METHOD("export_scene_glb", "scene", "filepath"), &VFXGLTFExporter::export_scene_glb);
}


VFXGLTFExporter::VFXGLTFExporter() {}
VFXGLTFExporter::~VFXGLTFExporter() {}

// === Binary helpers ===
int VFXGLTFExporter::_pad_to_4() {
    int pad = (4 - (buffer_data.size() % 4)) % 4;
    for (int i = 0; i < pad; i++) buffer_data.append(0);
    return buffer_data.size();
}

int VFXGLTFExporter::_write_float_array(const PackedFloat32Array& data) {
    int offset = _pad_to_4();
    for (int i = 0; i < data.size(); i++) {
        float v = data[i];
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&v);
        for (int b = 0; b < 4; b++) buffer_data.append(bytes[b]);
    }
    return offset;
}

int VFXGLTFExporter::_write_uint16_array(const PackedInt32Array& data) {
    int offset = _pad_to_4();
    for (int i = 0; i < data.size(); i++) {
        uint16_t v = (uint16_t)data[i];
        buffer_data.append(v & 0xFF);
        buffer_data.append((v >> 8) & 0xFF);
    }
    return offset;
}

int VFXGLTFExporter::_write_vec3_array(const PackedVector3Array& data) {
    PackedFloat32Array flat;
    flat.resize(data.size() * 3);
    for (int i = 0; i < data.size(); i++) {
        flat[i * 3 + 0] = data[i].x;
        flat[i * 3 + 1] = data[i].y;
        flat[i * 3 + 2] = data[i].z;
    }
    return _write_float_array(flat);
}

int VFXGLTFExporter::_write_vec2_array(const PackedVector2Array& data) {
    PackedFloat32Array flat;
    flat.resize(data.size() * 2);
    for (int i = 0; i < data.size(); i++) {
        flat[i * 2 + 0] = data[i].x;
        flat[i * 2 + 1] = data[i].y;
    }
    return _write_float_array(flat);
}

int VFXGLTFExporter::_write_mat4_array(const PackedFloat32Array& data) {
    return _write_float_array(data);
}

// === JSON builders ===
Dictionary VFXGLTFExporter::_build_asset() const {
    Dictionary asset;
    asset["generator"] = "GodotVFXToolkit";
    asset["version"] = "2.0";
    return asset;
}

Dictionary VFXGLTFExporter::_build_scene(int node_idx) const {
    Dictionary scene;
    Array nodes;
    nodes.append(node_idx);
    scene["nodes"] = nodes;
    return scene;
}

Dictionary VFXGLTFExporter::_build_node_mesh(int mesh_idx, int skin_idx) const {
    Dictionary node;
    node["mesh"] = mesh_idx;
    if (skin_idx >= 0) node["skin"] = skin_idx;
    return node;
}

Dictionary VFXGLTFExporter::_build_node_bone(int bone_idx, const std::vector<int>& children) const {
    Dictionary node;
    node["name"] = String("bone_") + String::num_int64(bone_idx);
    if (!children.empty()) {
        Array ch;
        for (int c : children) ch.append(c);
        node["children"] = ch;
    }
    return node;
}

Dictionary VFXGLTFExporter::_build_buffer(int byte_length) const {
    Dictionary buf;
    buf["byteLength"] = byte_length;
    return buf;
}

Dictionary VFXGLTFExporter::_build_buffer_view(int buffer, int offset, int length, int target) const {
    Dictionary bv;
    bv["buffer"] = buffer;
    bv["byteOffset"] = offset;
    bv["byteLength"] = length;
    if (target > 0) bv["target"] = target;
    return bv;
}

Dictionary VFXGLTFExporter::_build_accessor(int view, int count, int component_type, String type, bool normalized) const {
    Dictionary acc;
    acc["bufferView"] = view;
    acc["count"] = count;
    acc["componentType"] = component_type;
    acc["type"] = type;
    if (normalized) acc["normalized"] = true;

    // min/max for positions (required by spec)
    if (type == "VEC3" && component_type == GL_FLOAT) {
        // Will be filled during export
    }
    return acc;
}

Dictionary VFXGLTFExporter::_build_primitive(int pos_acc, int norm_acc, int uv_acc, int indices_acc, int joints_acc, int weights_acc) const {
    Dictionary prim;
    Dictionary attrs;
    attrs["POSITION"] = pos_acc;
    if (norm_acc >= 0) attrs["NORMAL"] = norm_acc;
    if (uv_acc >= 0) attrs["TEXCOORD_0"] = uv_acc;
    if (joints_acc >= 0) attrs["JOINTS_0"] = joints_acc;
    if (weights_acc >= 0) attrs["WEIGHTS_0"] = weights_acc;
    prim["attributes"] = attrs;
    prim["indices"] = indices_acc;
    prim["mode"] = 4; // TRIANGLES
    return prim;
}

Dictionary VFXGLTFExporter::_build_mesh(int prim_idx) const {
    Dictionary mesh;
    Array prims;
    prims.append(prim_idx);
    mesh["primitives"] = prims;
    return mesh;
}

Dictionary VFXGLTFExporter::_build_skin(int inv_bind_acc, const std::vector<int>& joints) const {
    Dictionary skin;
    skin["inverseBindMatrices"] = inv_bind_acc;
    Array j;
    for (int id : joints) j.append(id);
    skin["joints"] = j;
    return skin;
}

Dictionary VFXGLTFExporter::_build_animation_sampler(int input_acc, int output_acc, String interpolation) const {
    Dictionary sampler;
    sampler["input"] = input_acc;
    sampler["output"] = output_acc;
    sampler["interpolation"] = interpolation;
    return sampler;
}

Dictionary VFXGLTFExporter::_build_animation_channel(int sampler, String path, int node) const {
    Dictionary channel;
    Dictionary target;
    target["node"] = node;
    target["path"] = path;
    channel["target"] = target;
    channel["sampler"] = sampler;
    return channel;
}

Dictionary VFXGLTFExporter::_build_animation(const String& name, const Array& samplers, const Array& channels) const {
    Dictionary anim;
    anim["name"] = name;
    anim["samplers"] = samplers;
    anim["channels"] = channels;
    return anim;
}

// === NEW: Coordinate conversion helpers (Godot -> glTF) ===
Vector3 VFXGLTFExporter::_godot_to_gltf_position(const Vector3& p) {
    return Vector3(-p.x, p.y, -p.z);
}

Quaternion VFXGLTFExporter::_godot_to_gltf_rotation(const Quaternion& q) {
    return Quaternion(-q.x, q.y, -q.z, q.w);
}

Transform3D VFXGLTFExporter::_godot_to_gltf_transform(const Transform3D& t) {
    Basis b = t.get_basis();
    Vector3 o = t.get_origin();
    Basis gltf_b;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float m_i = (i == 0 || i == 2) ? -1.0f : 1.0f;
            float m_j = (j == 0 || j == 2) ? -1.0f : 1.0f;
            gltf_b[i][j] = m_i * b[i][j] * m_j;
        }
    }
    Vector3 gltf_o = Vector3(-o.x, o.y, -o.z);
    return Transform3D(gltf_b, gltf_o);
}

void VFXGLTFExporter::_decompose_godot_transform(const Transform3D& t, Vector3& out_pos, Quaternion& out_rot, Vector3& out_scale) {
    out_pos = _godot_to_gltf_position(t.get_origin());
    Basis b = t.get_basis();
    out_rot = _godot_to_gltf_rotation(b.get_rotation_quaternion());
    out_scale = b.get_scale();
}

// === NEW: Node builder with full transform ===
Dictionary VFXGLTFExporter::_build_node_full(const String& name, const Transform3D& transform, const std::vector<int>& children, int mesh_idx, int skin_idx) const {
    Dictionary node;
    if (!name.is_empty()) node["name"] = name;

    Vector3 pos, scale;
    Quaternion rot;
    _decompose_godot_transform(transform, pos, rot, scale);

    if (!pos.is_zero_approx()) {
        Array t_arr;
        t_arr.append(pos.x); t_arr.append(pos.y); t_arr.append(pos.z);
        node["translation"] = t_arr;
    }

    if (!Math::is_zero_approx(rot.x) || !Math::is_zero_approx(rot.y) ||
        !Math::is_zero_approx(rot.z) || !Math::is_equal_approx(rot.w, 1.0f)) {
        Array r_arr;
        r_arr.append(rot.x); r_arr.append(rot.y); r_arr.append(rot.z); r_arr.append(rot.w);
        node["rotation"] = r_arr;
    }

    if (!Math::is_equal_approx(scale.x, 1.0f) || !Math::is_equal_approx(scale.y, 1.0f) || !Math::is_equal_approx(scale.z, 1.0f)) {
        Array s_arr;
        s_arr.append(scale.x); s_arr.append(scale.y); s_arr.append(scale.z);
        node["scale"] = s_arr;
    }

    if (!children.empty()) {
        Array ch;
        for (int c : children) ch.append(c);
        node["children"] = ch;
    }

    if (mesh_idx >= 0) node["mesh"] = mesh_idx;
    if (skin_idx >= 0) node["skin"] = skin_idx;

    return node;
}

// === NEW: Scene export helpers ===
int VFXGLTFExporter::_export_mesh_to_gltf(const Ref<VFXMesh>& mesh, _SceneExportState& state) {
    if (mesh.is_null()) return -1;

    // Get mesh data and convert to glTF space
    PackedVector3Array positions = mesh->get_positions();
    for (int i = 0; i < positions.size(); i++) {
        positions[i] = _godot_to_gltf_position(positions[i]);
    }

    PackedVector3Array normals = mesh->get_normals();
    for (int i = 0; i < normals.size(); i++) {
        normals[i] = _godot_to_gltf_position(normals[i]);
    }

    PackedVector2Array uvs = mesh->get_uvs();
    PackedInt32Array indices = mesh->get_indices();

    if (positions.size() == 0) return -1;

    int pos_offset = _write_vec3_array(positions);
    int norm_offset = _write_vec3_array(normals);
    int uv_offset = _write_vec2_array(uvs);
    int idx_offset = _write_uint16_array(indices);

    int pos_bv = state.bufferViews.size();
    state.bufferViews.append(_build_buffer_view(0, pos_offset, positions.size() * 12, GL_ARRAY_BUFFER));

    int norm_bv = -1;
    if (normals.size() > 0) {
        norm_bv = state.bufferViews.size();
        state.bufferViews.append(_build_buffer_view(0, norm_offset, normals.size() * 12, GL_ARRAY_BUFFER));
    }

    int uv_bv = -1;
    if (uvs.size() > 0) {
        uv_bv = state.bufferViews.size();
        state.bufferViews.append(_build_buffer_view(0, uv_offset, uvs.size() * 8, GL_ARRAY_BUFFER));
    }

    int idx_bv = state.bufferViews.size();
    state.bufferViews.append(_build_buffer_view(0, idx_offset, indices.size() * 2, GL_ELEMENT_ARRAY_BUFFER));

    int pos_acc = state.accessors.size();
    Dictionary pos_accessor = _build_accessor(pos_bv, positions.size(), GL_FLOAT, "VEC3");
    Vector3 pmin(1e30f, 1e30f, 1e30f), pmax(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < positions.size(); i++) {
        pmin.x = fmin(pmin.x, positions[i].x); pmin.y = fmin(pmin.y, positions[i].y); pmin.z = fmin(pmin.z, positions[i].z);
        pmax.x = fmax(pmax.x, positions[i].x); pmax.y = fmax(pmax.y, positions[i].y); pmax.z = fmax(pmax.z, positions[i].z);
    }
    Array amin, amax;
    amin.append(pmin.x); amin.append(pmin.y); amin.append(pmin.z);
    amax.append(pmax.x); amax.append(pmax.y); amax.append(pmax.z);
    pos_accessor["min"] = amin;
    pos_accessor["max"] = amax;
    state.accessors.append(pos_accessor);

    int norm_acc = -1;
    if (norm_bv >= 0) {
        norm_acc = state.accessors.size();
        state.accessors.append(_build_accessor(norm_bv, normals.size(), GL_FLOAT, "VEC3"));
    }

    int uv_acc = -1;
    if (uv_bv >= 0) {
        uv_acc = state.accessors.size();
        state.accessors.append(_build_accessor(uv_bv, uvs.size(), GL_FLOAT, "VEC2"));
    }

    int idx_acc = state.accessors.size();
    state.accessors.append(_build_accessor(idx_bv, indices.size(), GL_UNSIGNED_SHORT, "SCALAR"));

    // Skinning data
    int joints_acc = -1, weights_acc = -1;
    PackedFloat32Array skin_data = mesh->get_skinning_data();
    if (skin_data.size() > 0) {
        PackedInt32Array joints;
        joints.resize(positions.size() * 4);
        PackedFloat32Array weights;
        weights.resize(positions.size() * 4);
        for (int i = 0; i < positions.size(); i++) {
            int base = i * 8;
            for (int j = 0; j < 4; j++) {
                joints[i * 4 + j] = (int)skin_data[base + j];
                weights[i * 4 + j] = skin_data[base + 4 + j];
            }
        }
        int joints_offset = _write_uint16_array(joints);
        int weights_offset = _write_float_array(weights);

        int joints_bv = state.bufferViews.size();
        state.bufferViews.append(_build_buffer_view(0, joints_offset, joints.size() * 2, GL_ARRAY_BUFFER));
        int weights_bv = state.bufferViews.size();
        state.bufferViews.append(_build_buffer_view(0, weights_offset, weights.size() * 4, GL_ARRAY_BUFFER));

        joints_acc = state.accessors.size();
        state.accessors.append(_build_accessor(joints_bv, positions.size(), GL_UNSIGNED_SHORT, "VEC4"));
        weights_acc = state.accessors.size();
        state.accessors.append(_build_accessor(weights_bv, positions.size(), GL_FLOAT, "VEC4"));
    }

    // Build mesh
    Dictionary prim = _build_primitive(pos_acc, norm_acc, uv_acc, idx_acc, joints_acc, weights_acc);
    Dictionary mesh_dict;
    Array prims;
    prims.append(prim);
    mesh_dict["primitives"] = prims;
    int mesh_idx = state.meshes.size();
    state.meshes.append(mesh_dict);

    return mesh_idx;
}

int VFXGLTFExporter::_export_skeleton_to_gltf(const Ref<VFXSkeleton>& skeleton, _SceneExportState& state, std::vector<int>& out_bone_node_indices) {
    int bone_count = skeleton->get_bone_count();
    if (bone_count == 0) return -1;

    out_bone_node_indices.resize(bone_count);

    // First pass: create all bone nodes
    for (int i = 0; i < bone_count; i++) {
        int node_idx = state.nodes.size();
        out_bone_node_indices[i] = node_idx;

        Dictionary bone_node;
        bone_node["name"] = skeleton->get_bone_name(i);

        // Bone local transform (converted to glTF space)
        Vector3 pos = skeleton->get_bone_local_position(i);
        Quaternion rot = skeleton->get_bone_local_rotation(i);
        Vector3 scale = skeleton->get_bone_local_scale(i);

        Vector3 gltf_pos = _godot_to_gltf_position(pos);
        Quaternion gltf_rot = _godot_to_gltf_rotation(rot);

        if (!gltf_pos.is_zero_approx()) {
            Array t_arr;
            t_arr.append(gltf_pos.x); t_arr.append(gltf_pos.y); t_arr.append(gltf_pos.z);
            bone_node["translation"] = t_arr;
        }

        if (!Math::is_zero_approx(gltf_rot.x) || !Math::is_zero_approx(gltf_rot.y) ||
            !Math::is_zero_approx(gltf_rot.z) || !Math::is_equal_approx(gltf_rot.w, 1.0f)) {
            Array r_arr;
            r_arr.append(gltf_rot.x); r_arr.append(gltf_rot.y); r_arr.append(gltf_rot.z); r_arr.append(gltf_rot.w);
            bone_node["rotation"] = r_arr;
        }

        if (!Math::is_equal_approx(scale.x, 1.0f) || !Math::is_equal_approx(scale.y, 1.0f) || !Math::is_equal_approx(scale.z, 1.0f)) {
            Array s_arr;
            s_arr.append(scale.x); s_arr.append(scale.y); s_arr.append(scale.z);
            bone_node["scale"] = s_arr;
        }

        state.nodes.append(bone_node);
    }

    // Second pass: set up bone children arrays
    for (int i = 0; i < bone_count; i++) {
        PackedInt32Array packed_children = skeleton->get_bone_children(i);
        if (packed_children.size() > 0) {
            Array ch;
            for (int j = 0; j < packed_children.size(); j++) {
                int child_bone = packed_children[j];
                ch.append(out_bone_node_indices[child_bone]);
            }
            Dictionary bone_node = state.nodes[out_bone_node_indices[i]];
            bone_node["children"] = ch;
            state.nodes[out_bone_node_indices[i]] = bone_node;
        }
    }

    // Third pass: write inverse bind matrices (in glTF space)
    PackedFloat32Array ibm;
    ibm.resize(bone_count * 16);
    for (int i = 0; i < bone_count; i++) {
        Transform3D gltf_bind = _godot_to_gltf_transform(skeleton->get_bone_bind_pose(i));
        Transform3D ibm_t = gltf_bind.affine_inverse();
        Basis b = ibm_t.get_basis();
        Vector3 t = ibm_t.get_origin();
        int base = i * 16;
        ibm[base + 0] = b[0][0]; ibm[base + 1] = b[0][1]; ibm[base + 2] = b[0][2]; ibm[base + 3] = 0;
        ibm[base + 4] = b[1][0]; ibm[base + 5] = b[1][1]; ibm[base + 6] = b[1][2]; ibm[base + 7] = 0;
        ibm[base + 8] = b[2][0]; ibm[base + 9] = b[2][1]; ibm[base + 10] = b[2][2]; ibm[base + 11] = 0;
        ibm[base + 12] = t.x;    ibm[base + 13] = t.y;    ibm[base + 14] = t.z;    ibm[base + 15] = 1;
    }
    int ibm_offset = _write_mat4_array(ibm);
    int ibm_bv = state.bufferViews.size();
    state.bufferViews.append(_build_buffer_view(0, ibm_offset, bone_count * 64, GL_ARRAY_BUFFER));
    int ibm_acc = state.accessors.size();
    state.accessors.append(_build_accessor(ibm_bv, bone_count, GL_FLOAT, "MAT4"));

    // Create skin
    int skin_idx = state.skins.size();
    state.skins.append(_build_skin(ibm_acc, out_bone_node_indices));

    return skin_idx;
}

void VFXGLTFExporter::_export_animations_to_gltf(const Ref<VFXAnimator>& animator, const Ref<VFXSkeleton>& skeleton, _SceneExportState& state, const std::vector<int>& bone_node_indices, const String& node_name_prefix) {
    if (animator.is_null() || skeleton.is_null()) return;

    int bone_count = skeleton->get_bone_count();
    if (bone_count == 0) return;

    int clip_count = animator->get_clip_count();
    for (int clip_idx = 0; clip_idx < clip_count; clip_idx++) {
        float duration = animator->get_clip_duration(clip_idx);
        float fps = 30.0f;
        int frame_count = (int)ceilf(duration * fps) + 1;

        // Build time samples
        PackedFloat32Array time_samples;
        time_samples.resize(frame_count);
        for (int i = 0; i < frame_count; i++) {
            time_samples[i] = (float)i / fps;
            if (time_samples[i] > duration) time_samples[i] = duration;
        }

        int time_offset = _write_float_array(time_samples);
        int time_bv = state.bufferViews.size();
        state.bufferViews.append(_build_buffer_view(0, time_offset, time_samples.size() * 4, GL_ARRAY_BUFFER));
        int time_acc = state.accessors.size();
        Dictionary time_accessor = _build_accessor(time_bv, time_samples.size(), GL_FLOAT, "SCALAR");
        Array tmin, tmax;
        tmin.append(0.0f); tmax.append(duration);
        time_accessor["min"] = tmin;
        time_accessor["max"] = tmax;
        state.accessors.append(time_accessor);

        int curve_count = animator->get_curve_count(clip_idx);
        Array anim_samplers;
        Array anim_channels;

        for (int c = 0; c < curve_count; c++) {
            int bone_id = animator->get_curve_bone_id(clip_idx, c);
            if (bone_id < 0 || bone_id >= bone_count) continue;

            bool is_rotation = animator->get_curve_is_rotation(clip_idx, c);

            PackedFloat32Array output_data;
            output_data.resize(frame_count * (is_rotation ? 4 : 3));

            for (int f = 0; f < frame_count; f++) {
                float t = time_samples[f];
                if (is_rotation) {
                    Quaternion q = animator->sample_quaternion(clip_idx, c, t);
                    Quaternion gltf_q = _godot_to_gltf_rotation(q);
                    output_data[f * 4 + 0] = gltf_q.x;
                    output_data[f * 4 + 1] = gltf_q.y;
                    output_data[f * 4 + 2] = gltf_q.z;
                    output_data[f * 4 + 3] = gltf_q.w;
                } else {
                    Vector3 v = animator->sample_vector(clip_idx, c, t);
                    Vector3 gltf_v;
                    String curve_name = animator->get_curve_name(clip_idx, c);
                    if (curve_name.contains("scale") || curve_name.contains("Scale")) {
                        gltf_v = v;
                    } else {
                        gltf_v = _godot_to_gltf_position(v);
                    }
                    output_data[f * 3 + 0] = gltf_v.x;
                    output_data[f * 3 + 1] = gltf_v.y;
                    output_data[f * 3 + 2] = gltf_v.z;
                }
            }

            int out_offset = _write_float_array(output_data);
            int out_bv = state.bufferViews.size();
            state.bufferViews.append(_build_buffer_view(0, out_offset, output_data.size() * 4, GL_ARRAY_BUFFER));
            int out_acc = state.accessors.size();
            state.accessors.append(_build_accessor(out_bv, frame_count, GL_FLOAT, is_rotation ? "VEC4" : "VEC3"));

            int sampler_idx = anim_samplers.size();
            anim_samplers.append(_build_animation_sampler(time_acc, out_acc, "LINEAR"));

            String curve_name = animator->get_curve_name(clip_idx, c);
            String path;
            if (curve_name.contains("scale") || curve_name.contains("Scale")) {
                path = "scale";
            } else if (is_rotation) {
                path = "rotation";
            } else {
                path = "translation";
            }

            anim_channels.append(_build_animation_channel(sampler_idx, path, bone_node_indices[bone_id]));
        }

        if (anim_samplers.size() > 0) {
            String anim_name = animator->get_clip_name(clip_idx);
            if (anim_name.is_empty()) anim_name = "animation";
            if (!node_name_prefix.is_empty()) anim_name = node_name_prefix + String("_") + anim_name;
            state.animations.append(_build_animation(anim_name, anim_samplers, anim_channels));
        }
    }
}


int VFXGLTFExporter::_export_scene_node_recursive(const Ref<VFXSceneNode>& node, _SceneExportState& state) {
    if (node.is_null()) return -1;

    // Recursively export children first
    std::vector<int> child_indices;
    Array vfx_children = node->get_children();
    for (int i = 0; i < vfx_children.size(); i++) {
        Ref<VFXSceneNode> child = vfx_children[i];
        int child_idx = _export_scene_node_recursive(child, state);
        if (child_idx >= 0) child_indices.push_back(child_idx);
    }

    // Handle skeleton
    int skin_idx = -1;
    std::vector<int> bone_node_indices;
    bool is_first_skeleton_export = false;
    if (node->has_skeleton()) {
        Ref<VFXSkeleton> skel = node->get_skeleton();
        VFXSkeleton* skel_ptr = skel.ptr();

        auto it = state.skeleton_to_skin.find(skel_ptr);
        if (it != state.skeleton_to_skin.end()) {
            skin_idx = it->second;
            bone_node_indices = state.skeleton_to_bone_nodes[skel_ptr];
        } else {
            skin_idx = _export_skeleton_to_gltf(skel, state, bone_node_indices);
            state.skeleton_to_skin[skel_ptr] = skin_idx;
            state.skeleton_to_bone_nodes[skel_ptr] = bone_node_indices;
            is_first_skeleton_export = true;
        }
    }

    // Handle mesh
    int mesh_idx = -1;
    if (node->has_mesh()) {
        Ref<VFXMesh> m = node->get_mesh();
        VFXMesh* m_ptr = m.ptr();

        auto it = state.mesh_to_index.find(m_ptr);
        if (it != state.mesh_to_index.end()) {
            mesh_idx = it->second;
        } else {
            mesh_idx = _export_mesh_to_gltf(m, state);
            if (mesh_idx >= 0) {
                state.mesh_to_index[m_ptr] = mesh_idx;
            }
        }
    }

    // Handle animations
    if (node->has_animator() && node->has_skeleton()) {
        Ref<VFXAnimator> anim = node->get_animator();
        VFXAnimator* anim_ptr = anim.ptr();

        auto it = state.exported_animators.find(anim_ptr);
        if (it == state.exported_animators.end()) {
            _export_animations_to_gltf(anim, node->get_skeleton(), state, bone_node_indices, node->get_node_name());
            state.exported_animators[anim_ptr] = true;
        }
    }

    // Build glTF node children list
    std::vector<int> all_children = child_indices;

    // Add root bones as children of this node ONLY on first export
    // (prevents double-parenting when skeleton is shared)
    if (is_first_skeleton_export && !bone_node_indices.empty()) {
        Ref<VFXSkeleton> skel = node->get_skeleton();
        for (int bone_id = 0; bone_id < skel->get_bone_count(); bone_id++) {
            if (skel->get_bone_parent(bone_id) < 0) {
                all_children.push_back(bone_node_indices[bone_id]);
            }
        }
    }

    Dictionary gltf_node = _build_node_full(node->get_node_name(), node->get_local_transform(), all_children, mesh_idx, skin_idx);
    int node_idx = state.nodes.size();
    state.nodes.append(gltf_node);
    return node_idx;
}

// === NEW: Main scene export ===
bool VFXGLTFExporter::export_scene_glb(const Ref<VFXScene>& scene, const String& filepath) {
    if (scene.is_null() || scene->get_root().is_null()) {
        UtilityFunctions::print("Scene export failed: no scene or root node");
        return false;
    }

    buffer_data.clear();

    _SceneExportState state;

    // Export the scene tree recursively
    int root_node_idx = _export_scene_node_recursive(scene->get_root(), state);
    if (root_node_idx < 0) {
        UtilityFunctions::print("Scene export failed: could not export root node");
        return false;
    }

    // Build scene
    Array scenes;
    scenes.append(_build_scene(root_node_idx));

    // Buffer
    state.buffers.append(_build_buffer(buffer_data.size()));

    // Assemble document
    Dictionary doc;
    doc["asset"] = _build_asset();
    doc["scene"] = 0;
    doc["scenes"] = scenes;
    doc["nodes"] = state.nodes;
    doc["meshes"] = state.meshes;
    doc["buffers"] = state.buffers;
    doc["bufferViews"] = state.bufferViews;
    doc["accessors"] = state.accessors;
    if (state.skins.size() > 0) doc["skins"] = state.skins;
    if (state.animations.size() > 0) doc["animations"] = state.animations;

    // Write GLB
    PackedByteArray glb = combine_glb(doc, buffer_data);

    Ref<FileAccess> f = FileAccess::open(filepath, FileAccess::WRITE);
    if (f.is_null()) return false;
    f->store_buffer(glb);
    f->close();

    int node_count = state.nodes.size();
    int mesh_count = state.meshes.size();
    int skin_count = state.skins.size();
    int anim_count = state.animations.size();
    UtilityFunctions::print("Exported scene GLB to: ", filepath,
        " (", node_count, " nodes, ", mesh_count, " meshes, ", skin_count, " skins, ", anim_count, " animations, ", buffer_data.size(), " bytes)");
    return true;
}


// === Main export ===
bool VFXGLTFExporter::export_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const String& filepath) {
    if (mesh.is_null()) return false;

    buffer_data.clear();

    Dictionary doc;
    doc["asset"] = _build_asset();

    // Arrays
    Array scenes, nodes, meshes_arr, buffers, bufferViews, accessors, skins;

    // Write mesh data to buffer
    PackedVector3Array positions = mesh->get_positions();
    PackedVector3Array normals = mesh->get_normals();
    PackedVector2Array uvs = mesh->get_uvs();
    PackedInt32Array indices = mesh->get_indices();

    int pos_offset = _write_vec3_array(positions);
    int norm_offset = _write_vec3_array(normals);
    int uv_offset = _write_vec2_array(uvs);
    int idx_offset = _write_uint16_array(indices);

    // Buffer views
    int pos_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, pos_offset, positions.size() * 12, GL_ARRAY_BUFFER));

    int norm_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, norm_offset, normals.size() * 12, GL_ARRAY_BUFFER));

    int uv_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, uv_offset, uvs.size() * 8, GL_ARRAY_BUFFER));

    int idx_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, idx_offset, indices.size() * 2, GL_ELEMENT_ARRAY_BUFFER));

    // Accessors
    int pos_acc = accessors.size();
    Dictionary pos_accessor = _build_accessor(pos_bv, positions.size(), GL_FLOAT, "VEC3");
    // Compute min/max
    Vector3 pmin(1e30f, 1e30f, 1e30f), pmax(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < positions.size(); i++) {
        pmin.x = fmin(pmin.x, positions[i].x); pmin.y = fmin(pmin.y, positions[i].y); pmin.z = fmin(pmin.z, positions[i].z);
        pmax.x = fmax(pmax.x, positions[i].x); pmax.y = fmax(pmax.y, positions[i].y); pmax.z = fmax(pmax.z, positions[i].z);
    }
    Array amin, amax;
    amin.append(pmin.x); amin.append(pmin.y); amin.append(pmin.z);
    amax.append(pmax.x); amax.append(pmax.y); amax.append(pmax.z);
    pos_accessor["min"] = amin;
    pos_accessor["max"] = amax;
    accessors.append(pos_accessor);

    int norm_acc = accessors.size();
    accessors.append(_build_accessor(norm_bv, normals.size(), GL_FLOAT, "VEC3"));

    int uv_acc = accessors.size();
    accessors.append(_build_accessor(uv_bv, uvs.size(), GL_FLOAT, "VEC2"));

    int idx_acc = accessors.size();
    accessors.append(_build_accessor(idx_bv, indices.size(), GL_UNSIGNED_SHORT, "SCALAR"));

    // Skinning data
    int joints_acc = -1, weights_acc = -1;
    PackedFloat32Array skin_data = mesh->get_skinning_data();
    if (skin_data.size() > 0 && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
        // JOINTS_0 (uint16, 4 per vertex)
        PackedInt32Array joints;
        joints.resize(positions.size() * 4);
        PackedFloat32Array weights;
        weights.resize(positions.size() * 4);
        for (int i = 0; i < positions.size(); i++) {
            int base = i * 8;
            for (int j = 0; j < 4; j++) {
                joints[i * 4 + j] = (int)skin_data[base + j];
                weights[i * 4 + j] = skin_data[base + 4 + j];
            }
        }
        int joints_offset = _write_uint16_array(joints);
        int weights_offset = _write_float_array(weights);

        int joints_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, joints_offset, joints.size() * 2, GL_ARRAY_BUFFER));
        int weights_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, weights_offset, weights.size() * 4, GL_ARRAY_BUFFER));

        joints_acc = accessors.size();
        accessors.append(_build_accessor(joints_bv, positions.size(), GL_UNSIGNED_SHORT, "VEC4"));
        weights_acc = accessors.size();
        accessors.append(_build_accessor(weights_bv, positions.size(), GL_FLOAT, "VEC4"));

        // Skeleton nodes
        int bone_count = skeleton->get_bone_count();
        std::vector<int> node_indices(bone_count);
        for (int i = 0; i < bone_count; i++) {
            node_indices[i] = nodes.size();
            PackedInt32Array packed_children = skeleton->get_bone_children(i);
            // Convert PackedInt32Array to std::vector<int>
            std::vector<int> ch;
            for (int j = 0; j < packed_children.size(); j++) ch.push_back(packed_children[j]);
            nodes.append(_build_node_bone(i, ch));
        }

        // Inverse bind matrices
        PackedFloat32Array ibm;
        ibm.resize(bone_count * 16);
        for (int i = 0; i < bone_count; i++) {
            Transform3D ibm_t = skeleton->get_bone_bind_pose(i).affine_inverse();
            Basis b = ibm_t.get_basis();
            Vector3 t = ibm_t.get_origin();
            int base = i * 16;
            ibm[base + 0] = b[0][0]; ibm[base + 1] = b[0][1]; ibm[base + 2] = b[0][2]; ibm[base + 3] = 0;
            ibm[base + 4] = b[1][0]; ibm[base + 5] = b[1][1]; ibm[base + 6] = b[1][2]; ibm[base + 7] = 0;
            ibm[base + 8] = b[2][0]; ibm[base + 9] = b[2][1]; ibm[base + 10] = b[2][2]; ibm[base + 11] = 0;
            ibm[base + 12] = t.x;    ibm[base + 13] = t.y;    ibm[base + 14] = t.z;    ibm[base + 15] = 1;
        }
        int ibm_offset = _write_mat4_array(ibm);
        int ibm_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, ibm_offset, bone_count * 64, GL_ARRAY_BUFFER));
        int ibm_acc = accessors.size();
        accessors.append(_build_accessor(ibm_bv, bone_count, GL_FLOAT, "MAT4"));

        // Skin
        int skin_idx = skins.size();
        skins.append(_build_skin(ibm_acc, node_indices));

        // Mesh node with skin
        int mesh_node = nodes.size();
        nodes.append(_build_node_mesh(0, skin_idx));

        scenes.append(_build_scene(mesh_node));
    } else {
        // No skeleton
        int mesh_node = nodes.size();
        nodes.append(_build_node_mesh(0, -1));
        scenes.append(_build_scene(mesh_node));
    }

    // Mesh
    Dictionary prim = _build_primitive(pos_acc, norm_acc, uv_acc, idx_acc, joints_acc, weights_acc);
    Dictionary mesh_dict;
    Array prims;
    prims.append(prim);
    mesh_dict["primitives"] = prims;
    meshes_arr.append(mesh_dict);

    // Buffer
    buffers.append(_build_buffer(buffer_data.size()));

    // Assemble document
    doc["scene"] = 0;
    doc["scenes"] = scenes;
    doc["nodes"] = nodes;
    doc["meshes"] = meshes_arr;
    doc["buffers"] = buffers;
    doc["bufferViews"] = bufferViews;
    doc["accessors"] = accessors;
    if (skins.size() > 0) doc["skins"] = skins;

    // Write GLB
    PackedByteArray glb = combine_glb(doc, buffer_data);

    Ref<FileAccess> f = FileAccess::open(filepath, FileAccess::WRITE);
    if (f.is_null()) return false;
    f->store_buffer(glb);
    f->close();

    UtilityFunctions::print("Exported GLB to: ", filepath, " (", buffer_data.size(), " bytes)");
    return true;
}

bool VFXGLTFExporter::export_glb_animated(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const Ref<VFXAnimator>& animator, int clip_idx, const String& filepath) {
    if (mesh.is_null() || skeleton.is_null() || animator.is_null()) {
        UtilityFunctions::print("Animated export failed: missing mesh, skeleton, or animator");
        return false;
    }
    if (clip_idx < 0 || clip_idx >= animator->get_clip_count()) {
        UtilityFunctions::print("Animated export failed: invalid clip index ", clip_idx);
        return false;
    }

    buffer_data.clear();

    Dictionary doc;
    doc["asset"] = _build_asset();

    // Arrays
    Array scenes, nodes, meshes_arr, buffers, bufferViews, accessors, skins, animations;

    // Write mesh data to buffer
    PackedVector3Array positions = mesh->get_positions();
    PackedVector3Array normals = mesh->get_normals();
    PackedVector2Array uvs = mesh->get_uvs();
    PackedInt32Array indices = mesh->get_indices();

    int pos_offset = _write_vec3_array(positions);
    int norm_offset = _write_vec3_array(normals);
    int uv_offset = _write_vec2_array(uvs);
    int idx_offset = _write_uint16_array(indices);

    // Buffer views
    int pos_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, pos_offset, positions.size() * 12, GL_ARRAY_BUFFER));

    int norm_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, norm_offset, normals.size() * 12, GL_ARRAY_BUFFER));

    int uv_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, uv_offset, uvs.size() * 8, GL_ARRAY_BUFFER));

    int idx_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, idx_offset, indices.size() * 2, GL_ELEMENT_ARRAY_BUFFER));

    // Accessors
    int pos_acc = accessors.size();
    Dictionary pos_accessor = _build_accessor(pos_bv, positions.size(), GL_FLOAT, "VEC3");
    Vector3 pmin(1e30f, 1e30f, 1e30f), pmax(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < positions.size(); i++) {
        pmin.x = fmin(pmin.x, positions[i].x); pmin.y = fmin(pmin.y, positions[i].y); pmin.z = fmin(pmin.z, positions[i].z);
        pmax.x = fmax(pmax.x, positions[i].x); pmax.y = fmax(pmax.y, positions[i].y); pmax.z = fmax(pmax.z, positions[i].z);
    }
    Array amin, amax;
    amin.append(pmin.x); amin.append(pmin.y); amin.append(pmin.z);
    amax.append(pmax.x); amax.append(pmax.y); amax.append(pmax.z);
    pos_accessor["min"] = amin;
    pos_accessor["max"] = amax;
    accessors.append(pos_accessor);

    int norm_acc = accessors.size();
    accessors.append(_build_accessor(norm_bv, normals.size(), GL_FLOAT, "VEC3"));

    int uv_acc = accessors.size();
    accessors.append(_build_accessor(uv_bv, uvs.size(), GL_FLOAT, "VEC2"));

    int idx_acc = accessors.size();
    accessors.append(_build_accessor(idx_bv, indices.size(), GL_UNSIGNED_SHORT, "SCALAR"));

    // Skinning data
    int joints_acc = -1, weights_acc = -1;
    PackedFloat32Array skin_data = mesh->get_skinning_data();
    int bone_count = skeleton->get_bone_count();
    std::vector<int> node_indices(bone_count);

    if (skin_data.size() > 0 && bone_count > 0) {
        // JOINTS_0 (uint16, 4 per vertex)
        PackedInt32Array joints;
        joints.resize(positions.size() * 4);
        PackedFloat32Array weights;
        weights.resize(positions.size() * 4);
        for (int i = 0; i < positions.size(); i++) {
            int base = i * 8;
            for (int j = 0; j < 4; j++) {
                joints[i * 4 + j] = (int)skin_data[base + j];
                weights[i * 4 + j] = skin_data[base + 4 + j];
            }
        }
        int joints_offset = _write_uint16_array(joints);
        int weights_offset = _write_float_array(weights);

        int joints_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, joints_offset, joints.size() * 2, GL_ARRAY_BUFFER));
        int weights_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, weights_offset, weights.size() * 4, GL_ARRAY_BUFFER));

        joints_acc = accessors.size();
        accessors.append(_build_accessor(joints_bv, positions.size(), GL_UNSIGNED_SHORT, "VEC4"));
        weights_acc = accessors.size();
        accessors.append(_build_accessor(weights_bv, positions.size(), GL_FLOAT, "VEC4"));

        // Skeleton nodes
        for (int i = 0; i < bone_count; i++) {
            node_indices[i] = nodes.size();
            PackedInt32Array packed_children = skeleton->get_bone_children(i);
            std::vector<int> ch;
            for (int j = 0; j < packed_children.size(); j++) ch.push_back(packed_children[j]);
            nodes.append(_build_node_bone(i, ch));
        }

        // Inverse bind matrices
        PackedFloat32Array ibm;
        ibm.resize(bone_count * 16);
        for (int i = 0; i < bone_count; i++) {
            Transform3D ibm_t = skeleton->get_bone_bind_pose(i).affine_inverse();
            Basis b = ibm_t.get_basis();
            Vector3 t = ibm_t.get_origin();
            int base = i * 16;
            ibm[base + 0] = b[0][0]; ibm[base + 1] = b[0][1]; ibm[base + 2] = b[0][2]; ibm[base + 3] = 0;
            ibm[base + 4] = b[1][0]; ibm[base + 5] = b[1][1]; ibm[base + 6] = b[1][2]; ibm[base + 7] = 0;
            ibm[base + 8] = b[2][0]; ibm[base + 9] = b[2][1]; ibm[base + 10] = b[2][2]; ibm[base + 11] = 0;
            ibm[base + 12] = t.x;    ibm[base + 13] = t.y;    ibm[base + 14] = t.z;    ibm[base + 15] = 1;
        }
        int ibm_offset = _write_mat4_array(ibm);
        int ibm_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, ibm_offset, bone_count * 64, GL_ARRAY_BUFFER));
        int ibm_acc = accessors.size();
        accessors.append(_build_accessor(ibm_bv, bone_count, GL_FLOAT, "MAT4"));

        // Skin
        int skin_idx = skins.size();
        skins.append(_build_skin(ibm_acc, node_indices));

        // Mesh node with skin
        int mesh_node = nodes.size();
        nodes.append(_build_node_mesh(0, skin_idx));

        scenes.append(_build_scene(mesh_node));

        // === ANIMATION ===
        float duration = animator->get_clip_duration(clip_idx);
        float fps = 30.0f; // Sample at 30fps
        int frame_count = (int)ceilf(duration * fps) + 1;

        // Build time samples
        PackedFloat32Array time_samples;
        time_samples.resize(frame_count);
        for (int i = 0; i < frame_count; i++) {
            time_samples[i] = (float)i / fps;
            if (time_samples[i] > duration) time_samples[i] = duration;
        }

        // Write time accessor (shared)
        int time_offset = _write_float_array(time_samples);
        int time_bv = bufferViews.size();
        bufferViews.append(_build_buffer_view(0, time_offset, time_samples.size() * 4, GL_ARRAY_BUFFER));
        int time_acc = accessors.size();
        Dictionary time_accessor = _build_accessor(time_bv, time_samples.size(), GL_FLOAT, "SCALAR");
        Array tmin, tmax;
        tmin.append(0.0f); tmax.append(duration);
        time_accessor["min"] = tmin;
        time_accessor["max"] = tmax;
        accessors.append(time_accessor);

        int curve_count = animator->get_curve_count(clip_idx);
        Array anim_samplers;
        Array anim_channels;

        for (int c = 0; c < curve_count; c++) {
            int bone_id = animator->get_curve_bone_id(clip_idx, c);
            if (bone_id < 0 || bone_id >= bone_count) continue;

            bool is_rotation = animator->get_curve_is_rotation(clip_idx, c);

            // Sample the curve at each time point
            PackedFloat32Array output_data;
            output_data.resize(frame_count * (is_rotation ? 4 : 3));

            for (int f = 0; f < frame_count; f++) {
                float t = time_samples[f];
                if (is_rotation) {
                    Quaternion q = animator->sample_quaternion(clip_idx, c, t);
                    output_data[f * 4 + 0] = q.x;
                    output_data[f * 4 + 1] = q.y;
                    output_data[f * 4 + 2] = q.z;
                    output_data[f * 4 + 3] = q.w;
                } else {
                    Vector3 v = animator->sample_vector(clip_idx, c, t);
                    output_data[f * 3 + 0] = v.x;
                    output_data[f * 3 + 1] = v.y;
                    output_data[f * 3 + 2] = v.z;
                }
            }

            // Write output accessor
            int out_offset = _write_float_array(output_data);
            int out_bv = bufferViews.size();
            bufferViews.append(_build_buffer_view(0, out_offset, output_data.size() * 4, GL_ARRAY_BUFFER));
            int out_acc = accessors.size();
            accessors.append(_build_accessor(out_bv, frame_count, GL_FLOAT, is_rotation ? "VEC4" : "VEC3"));

            // Create sampler
            int sampler_idx = anim_samplers.size();
            anim_samplers.append(_build_animation_sampler(time_acc, out_acc, "LINEAR"));

            // Determine path
            String path;
            // Check if this curve is scale by looking at curve name or properties
            String curve_name = animator->get_curve_name(clip_idx, c);
            if (curve_name.contains("scale") || curve_name.contains("Scale")) {
                path = "scale";
            } else if (is_rotation) {
                path = "rotation";
            } else {
                path = "translation";
            }

            // Create channel targeting the bone node
            anim_channels.append(_build_animation_channel(sampler_idx, path, node_indices[bone_id]));
        }

        // Add animation to document
        if (anim_samplers.size() > 0) {
            String anim_name = animator->get_clip_name(clip_idx);
            if (anim_name.is_empty()) anim_name = "animation";
            animations.append(_build_animation(anim_name, anim_samplers, anim_channels));
        }

    } else {
        // No skeleton
        int mesh_node = nodes.size();
        nodes.append(_build_node_mesh(0, -1));
        scenes.append(_build_scene(mesh_node));
    }

    // Mesh
    Dictionary prim = _build_primitive(pos_acc, norm_acc, uv_acc, idx_acc, joints_acc, weights_acc);
    Dictionary mesh_dict;
    Array prims;
    prims.append(prim);
    mesh_dict["primitives"] = prims;
    meshes_arr.append(mesh_dict);

    // Buffer
    buffers.append(_build_buffer(buffer_data.size()));

    // Assemble document
    doc["scene"] = 0;
    doc["scenes"] = scenes;
    doc["nodes"] = nodes;
    doc["meshes"] = meshes_arr;
    doc["buffers"] = buffers;
    doc["bufferViews"] = bufferViews;
    doc["accessors"] = accessors;
    if (skins.size() > 0) doc["skins"] = skins;
    if (animations.size() > 0) doc["animations"] = animations;

    // Write GLB
    PackedByteArray glb = combine_glb(doc, buffer_data);

    Ref<FileAccess> f = FileAccess::open(filepath, FileAccess::WRITE);
    if (f.is_null()) return false;
    f->store_buffer(glb);
    f->close();

    UtilityFunctions::print("Exported animated GLB to: ", filepath, " (", buffer_data.size(), " bytes, ", animations.size(), " animation(s))");
    return true;
}

bool VFXGLTFExporter::export_vat_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkin>& skin, int frame_count, float fps, const String& filepath) {
    if (mesh.is_null() || skin.is_null()) {
        UtilityFunctions::print("VAT export failed: missing mesh or skin");
        return false;
    }
    if (frame_count <= 0 || fps <= 0.0f) {
        UtilityFunctions::print("VAT export failed: invalid frame_count or fps");
        return false;
    }

    buffer_data.clear();

    Dictionary doc;
    doc["asset"] = _build_asset();

    // Arrays
    Array scenes, nodes, meshes_arr, buffers, bufferViews, accessors;

    // Static mesh data (bind pose)
    PackedVector3Array bind_positions = mesh->get_positions();
    PackedVector3Array normals = mesh->get_normals();
    PackedVector2Array uvs = mesh->get_uvs();
    PackedInt32Array indices = mesh->get_indices();

    int pos_offset = _write_vec3_array(bind_positions);
    int norm_offset = _write_vec3_array(normals);
    int uv_offset = _write_vec2_array(uvs);
    int idx_offset = _write_uint16_array(indices);

    // Buffer views
    int pos_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, pos_offset, bind_positions.size() * 12, GL_ARRAY_BUFFER));

    int norm_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, norm_offset, normals.size() * 12, GL_ARRAY_BUFFER));

    int uv_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, uv_offset, uvs.size() * 8, GL_ARRAY_BUFFER));

    int idx_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, idx_offset, indices.size() * 2, GL_ELEMENT_ARRAY_BUFFER));

    // Accessors
    int pos_acc = accessors.size();
    Dictionary pos_accessor = _build_accessor(pos_bv, bind_positions.size(), GL_FLOAT, "VEC3");
    Vector3 pmin(1e30f, 1e30f, 1e30f), pmax(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < bind_positions.size(); i++) {
        pmin.x = fmin(pmin.x, bind_positions[i].x); pmin.y = fmin(pmin.y, bind_positions[i].y); pmin.z = fmin(pmin.z, bind_positions[i].z);
        pmax.x = fmax(pmax.x, bind_positions[i].x); pmax.y = fmax(pmax.y, bind_positions[i].y); pmax.z = fmax(pmax.z, bind_positions[i].z);
    }
    Array amin, amax;
    amin.append(pmin.x); amin.append(pmin.y); amin.append(pmin.z);
    amax.append(pmax.x); amax.append(pmax.y); amax.append(pmax.z);
    pos_accessor["min"] = amin;
    pos_accessor["max"] = amax;
    accessors.append(pos_accessor);

    int norm_acc = accessors.size();
    accessors.append(_build_accessor(norm_bv, normals.size(), GL_FLOAT, "VEC3"));

    int uv_acc = accessors.size();
    accessors.append(_build_accessor(uv_bv, uvs.size(), GL_FLOAT, "VEC2"));

    int idx_acc = accessors.size();
    accessors.append(_build_accessor(idx_bv, indices.size(), GL_UNSIGNED_SHORT, "SCALAR"));

    // === VAT TEXTURES ===
    // Bake vertex animation data
    PackedFloat32Array vat_data = skin->bake_vertex_animation(frame_count, fps);
    int vertex_count = mesh->get_vertex_count();

    // Position texture: RGB32F per vertex per frame
    // Layout: each frame is a row, each vertex is a pixel (or packed)
    int tex_width = vertex_count;
    int tex_height = frame_count;

    // For glTF, we store VAT data as buffer views and use KHR_texture_float extension
    // or we can embed as raw buffer data with custom attributes

    // Write position animation data as a flat buffer
    PackedFloat32Array pos_anim_data;
    pos_anim_data.resize(frame_count * vertex_count * 3);
    PackedFloat32Array norm_anim_data;
    norm_anim_data.resize(frame_count * vertex_count * 3);

    for (int f = 0; f < frame_count; f++) {
        int base = f * vertex_count * 6;
        for (int v = 0; v < vertex_count; v++) {
            int src = base + v * 6;
            int dst = (f * vertex_count + v) * 3;
            pos_anim_data[dst + 0] = vat_data[src + 0];
            pos_anim_data[dst + 1] = vat_data[src + 1];
            pos_anim_data[dst + 2] = vat_data[src + 2];
            norm_anim_data[dst + 0] = vat_data[src + 3];
            norm_anim_data[dst + 1] = vat_data[src + 4];
            norm_anim_data[dst + 2] = vat_data[src + 5];
        }
    }

    int pos_anim_offset = _write_float_array(pos_anim_data);
    int pos_anim_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, pos_anim_offset, pos_anim_data.size() * 4, GL_ARRAY_BUFFER));
    int pos_anim_acc = accessors.size();
    accessors.append(_build_accessor(pos_anim_bv, pos_anim_data.size() / 3, GL_FLOAT, "VEC3"));

    int norm_anim_offset = _write_float_array(norm_anim_data);
    int norm_anim_bv = bufferViews.size();
    bufferViews.append(_build_buffer_view(0, norm_anim_offset, norm_anim_data.size() * 4, GL_ARRAY_BUFFER));
    int norm_anim_acc = accessors.size();
    accessors.append(_build_accessor(norm_anim_bv, norm_anim_data.size() / 3, GL_FLOAT, "VEC3"));

    // Mesh node (no skin, VAT replaces skinning)
    int mesh_node = nodes.size();
    Dictionary mesh_node_dict;
    mesh_node_dict["mesh"] = 0;
    // Add VAT metadata as extras
    Dictionary extras;
    extras["VAT_frame_count"] = frame_count;
    extras["VAT_fps"] = fps;
    extras["VAT_vertex_count"] = vertex_count;
    extras["VAT_position_accessor"] = pos_anim_acc;
    extras["VAT_normal_accessor"] = norm_anim_acc;
    mesh_node_dict["extras"] = extras;
    nodes.append(mesh_node_dict);

    scenes.append(_build_scene(mesh_node));

    // Mesh
    Dictionary prim = _build_primitive(pos_acc, norm_acc, uv_acc, idx_acc, -1, -1);
    Dictionary mesh_dict;
    Array prims;
    prims.append(prim);
    mesh_dict["primitives"] = prims;
    meshes_arr.append(mesh_dict);

    // Buffer
    buffers.append(_build_buffer(buffer_data.size()));

    // Assemble document
    doc["scene"] = 0;
    doc["scenes"] = scenes;
    doc["nodes"] = nodes;
    doc["meshes"] = meshes_arr;
    doc["buffers"] = buffers;
    doc["bufferViews"] = bufferViews;
    doc["accessors"] = accessors;

    // Add extensionsUsed for KHR_texture_float if needed
    Array extensionsUsed;
    extensionsUsed.append("KHR_texture_float");
    doc["extensionsUsed"] = extensionsUsed;

    // Write GLB
    PackedByteArray glb = combine_glb(doc, buffer_data);

    Ref<FileAccess> f = FileAccess::open(filepath, FileAccess::WRITE);
    if (f.is_null()) return false;
    f->store_buffer(glb);
    f->close();

    UtilityFunctions::print("Exported VAT GLB to: ", filepath, 
        " (", buffer_data.size(), " bytes, ", frame_count, " frames, ", vertex_count, " verts)");
    return true;
}

Dictionary VFXGLTFExporter::build_gltf_document(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton) const {
    // Non-export version for inspection
    Dictionary doc;
    doc["asset"] = _build_asset();
    return doc;
}

PackedByteArray VFXGLTFExporter::json_to_bytes(const Dictionary& doc) {
    String json = JSON::stringify(doc, "        ", false, true);
    PackedByteArray bytes;
    CharString utf8 = json.utf8();
    const char* utf8_data = utf8.get_data();
    for (int i = 0; i < utf8.size(); i++) bytes.append(utf8_data[i]);
    return bytes;
}

PackedByteArray VFXGLTFExporter::combine_glb(const Dictionary& json_doc, const PackedByteArray& bin) {
    PackedByteArray json_bytes = json_to_bytes(json_doc);

    // Pad JSON to 4-byte boundary
    while (json_bytes.size() % 4 != 0) json_bytes.append(0x20); // space padding

    // Pad BIN to 4-byte boundary
    PackedByteArray bin_padded = bin;
    while (bin_padded.size() % 4 != 0) bin_padded.append(0);

    uint32_t json_len = json_bytes.size();
    uint32_t bin_len = bin_padded.size();
    uint32_t total_len = 12 + 8 + json_len + 8 + bin_len;

    PackedByteArray glb;
    glb.resize(total_len);
    int idx = 0;

    // Header
    auto write_u32 = [&](uint32_t v) {
        glb[idx++] = v & 0xFF;
        glb[idx++] = (v >> 8) & 0xFF;
        glb[idx++] = (v >> 16) & 0xFF;
        glb[idx++] = (v >> 24) & 0xFF;
    };

    write_u32(GLB_MAGIC);
    write_u32(GLB_VERSION);
    write_u32(total_len);

    // JSON chunk
    write_u32(json_len);
    write_u32(GLB_CHUNK_JSON);
    for (int i = 0; i < json_bytes.size(); i++) glb[idx++] = json_bytes[i];

    // BIN chunk
    write_u32(bin_len);
    write_u32(GLB_CHUNK_BIN);
    for (int i = 0; i < bin_padded.size(); i++) glb[idx++] = bin_padded[i];

    return glb;
}
