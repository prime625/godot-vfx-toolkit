#include "vfx_gltf_exporter.h"
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
    // TODO: Add animation samplers/channels to the GLB export
    // For now, fall back to static export
    UtilityFunctions::print("Animated export TODO - exporting static mesh");
    return export_glb(mesh, skeleton, filepath);
}

bool VFXGLTFExporter::export_vat_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkin>& skin, int frame_count, float fps, const String& filepath) {
    // TODO: Export mesh + VAT textures
    UtilityFunctions::print("VAT export TODO");
    return false;
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
