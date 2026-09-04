#include "vfx_glb_importer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/vector4i.hpp>
#include <cmath>
#include <cstring>
#include <unordered_set>

using namespace godot;

void VFXGLBImporter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("import_glb", "path"), &VFXGLBImporter::import_glb);
    ClassDB::bind_static_method("VFXGLBImporter", D_METHOD("convert_position", "gltf_pos"), &VFXGLBImporter::convert_position);
    ClassDB::bind_static_method("VFXGLBImporter", D_METHOD("convert_normal", "gltf_normal"), &VFXGLBImporter::convert_normal);
    ClassDB::bind_static_method("VFXGLBImporter", D_METHOD("convert_rotation", "gltf_rot"), &VFXGLBImporter::convert_rotation);
    ClassDB::bind_static_method("VFXGLBImporter", D_METHOD("convert_transform", "gltf_transform"), &VFXGLBImporter::convert_transform);
}

VFXGLBImporter::VFXGLBImporter() {}
VFXGLBImporter::~VFXGLBImporter() {}

Vector3 VFXGLBImporter::convert_position(const Vector3& gltf_pos) {
    return Vector3(-gltf_pos.x, gltf_pos.y, -gltf_pos.z);
}

Vector3 VFXGLBImporter::convert_normal(const Vector3& gltf_normal) {
    Vector3 n = Vector3(-gltf_normal.x, gltf_normal.y, -gltf_normal.z);
    return n.normalized();
}

Quaternion VFXGLBImporter::convert_rotation(const Quaternion& gltf_rot) {
    return Quaternion(-gltf_rot.x, gltf_rot.y, -gltf_rot.z, gltf_rot.w);
}

Transform3D VFXGLBImporter::convert_transform(const Transform3D& gltf_transform) {
    Basis b = gltf_transform.get_basis();
    Vector3 o = gltf_transform.get_origin();
    Basis converted;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float m_i = (i == 0 || i == 2) ? -1.0f : 1.0f;
            float m_j = (j == 0 || j == 2) ? -1.0f : 1.0f;
            converted[i][j] = m_i * b[i][j] * m_j;
        }
    }
    Vector3 o2 = Vector3(-o.x, o.y, -o.z);
    return Transform3D(converted, o2);
}

Vector3 VFXGLBImporter::_gltf_to_godot_v3(const Vector3& v) { return convert_position(v); }
Vector3 VFXGLBImporter::_gltf_to_godot_n(const Vector3& n) { return convert_normal(n); }
Quaternion VFXGLBImporter::_gltf_to_godot_q(const Quaternion& q) { return convert_rotation(q); }
Transform3D VFXGLBImporter::_gltf_to_godot_t(const Transform3D& t) { return convert_transform(t); }
Basis VFXGLBImporter::_gltf_to_godot_b(const Basis& b) {
    return convert_transform(Transform3D(b, Vector3())).get_basis();
}

Transform3D VFXGLBImporter::_get_node_local_transform(const GLBNode& node) const {
    if (node.has_matrix) {
        return _gltf_to_godot_t(node.matrix);
    }
    Transform3D t;
    t.set_origin(_gltf_to_godot_v3(node.translation));
    Basis b(_gltf_to_godot_q(node.rotation));
    b.scale(node.scale);
    t.set_basis(b);
    return t;
}

void VFXGLBImporter::_clear_document() {
    buffers.clear();
    buffer_views.clear();
    accessors.clear();
    nodes.clear();
    meshes.clear();
    skins.clear();
    animations.clear();
    scene_nodes.clear();
}

Dictionary VFXGLBImporter::import_glb(const String& path) {
    Dictionary result;
    result[KEY_SUCCESS] = false;
    result[KEY_ERROR] = "";
    result[KEY_SCENE] = Variant();
    result[KEY_MESH_NODES] = Array();
    result[KEY_MESH] = Variant();
    result[KEY_SKELETON] = Variant();
    result[KEY_SKIN] = Variant();
    result[KEY_ANIMATOR] = Variant();
    result[KEY_MATERIAL_COUNT] = 0;
    result[KEY_ANIMATION_COUNT] = 0;
    result[KEY_NODE_COUNT] = 0;
    result[KEY_MESH_COUNT] = 0;

    Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
    if (f.is_null()) {
        result[KEY_ERROR] = "Failed to open file: " + path;
        return result;
    }

    PackedByteArray data = f->get_buffer(f->get_length());
    f->close();

    String error;
    if (!_parse_glb(data, error)) {
        result[KEY_ERROR] = error;
        return result;
    }

    result[KEY_NODE_COUNT] = (int)nodes.size();
    result[KEY_MESH_COUNT] = (int)meshes.size();
    result[KEY_ANIMATION_COUNT] = (int)animations.size();

    // Create VFXScene and build node hierarchy
    Ref<VFXScene> scene;
    scene.instantiate();
    scene->create_default_root();

    // Collect all joint node indices so we can skip creating scene nodes for bones.
    // Bones live inside VFXSkeleton only; they should NOT appear as VFXSceneNode items.
    std::unordered_set<int> joint_node_set;
    for (const GLBSkin& skin : skins) {
        for (int joint_idx : skin.joints) {
            if (joint_idx >= 0 && joint_idx < (int)nodes.size()) {
                joint_node_set.insert(joint_idx);
            }
        }
    }

    // Also mark descendants of joints as skeleton-related (end bones, leaf tips, etc.)
    // UNLESS they carry a mesh (e.g. a weapon parented to a hand bone).
    std::unordered_set<int> skeleton_node_set = joint_node_set;
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (joint_node_set.count(i)) continue;
        int p = nodes[i].parent;
        bool has_joint_ancestor = false;
        while (p >= 0 && p < (int)nodes.size()) {
            if (joint_node_set.count(p)) {
                has_joint_ancestor = true;
                break;
            }
            p = nodes[p].parent;
        }
        if (has_joint_ancestor && nodes[i].mesh < 0) {
            skeleton_node_set.insert(i);
        }
    }

    // Helper: find nearest non-skeleton ancestor for a given node index
    auto find_non_skeleton_parent = [&](int node_idx) -> int {
        int p = nodes[node_idx].parent;
        while (p >= 0 && p < (int)nodes.size() && skeleton_node_set.count(p)) {
            p = nodes[p].parent;
        }
        return p;
    };

    // Create a VFXSceneNode for every NON-SKELETON glTF node only
    std::vector<Ref<VFXSceneNode>> vfx_nodes(nodes.size());
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (skeleton_node_set.count(i)) continue; // skip bones & bone descendants
        vfx_nodes[i].instantiate();
        vfx_nodes[i]->set_node_name(nodes[i].name);
        vfx_nodes[i]->set_local_transform(_get_node_local_transform(nodes[i]));
        vfx_nodes[i]->set_node_type(VFXSceneNode::NODE_EMPTY);
	// Mark armature roots: non-skeleton nodes that are ancestors of skeletons
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (!skeleton_node_set.count(i)) continue;
        int p = nodes[i].parent;
        while (p >= 0 && p < (int)nodes.size()) {
            if (!skeleton_node_set.count(p)) {
               if (vfx_nodes[p].is_valid()) {
                   vfx_nodes[p]->set_node_type(VFXSceneNode::NODE_ARMATURE);
               }
               break;
            }
            p = nodes[p].parent;
        }
    }


    // Build hierarchy: parent -> child links, skipping over skeleton parents
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (skeleton_node_set.count(i)) continue; // skip bones & bone descendants
        int parent = find_non_skeleton_parent(i);
        if (parent >= 0 && parent < (int)nodes.size() && vfx_nodes[parent].is_valid()) {
            vfx_nodes[parent]->add_child(vfx_nodes[i]);
        }
    }

    // Attach root-level non-skeleton scene nodes to the scene root
    for (int scene_node_idx : scene_nodes) {
        if (scene_node_idx >= 0 && scene_node_idx < (int)nodes.size()) {
            if (skeleton_node_set.count(scene_node_idx)) continue;
            int parent = find_non_skeleton_parent(scene_node_idx);
            if (parent < 0) {
                bool already_child = false;
                for (int j = 0; j < scene->get_root()->get_child_count(); j++) {
                    if (scene->get_root()->get_child(j) == vfx_nodes[scene_node_idx]) {
                        already_child = true;
                        break;
                    }
                }
                if (!already_child && vfx_nodes[scene_node_idx].is_valid()) {
                    scene->get_root()->add_child(vfx_nodes[scene_node_idx]);
                }
            }
        }
    }

    // Ensure ALL orphan non-skeleton nodes are attached (not just scene_nodes)
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (skeleton_node_set.count(i)) continue;
        int parent = find_non_skeleton_parent(i);
        if (parent < 0) {
            bool already_child = false;
            for (int j = 0; j < scene->get_root()->get_child_count(); j++) {
                if (scene->get_root()->get_child(j) == vfx_nodes[i]) {
                    already_child = true;
                    break;
                }
            }
            if (!already_child && vfx_nodes[i].is_valid()) {
                scene->get_root()->add_child(vfx_nodes[i]);
            }
        }
    }

    // Build meshes, skeletons, skins, animators per node
    std::vector<Ref<VFXSkeleton>> built_skeletons(skins.size());
    std::vector<Ref<VFXAnimator>> built_animators(skins.size());
    Array mesh_nodes_array;

    Ref<VFXMesh> first_mesh;
    Ref<VFXSkeleton> first_skeleton;
    Ref<VFXSkin> first_skin;
    Ref<VFXAnimator> first_animator;

    for (int i = 0; i < (int)nodes.size(); i++) {
        if (!vfx_nodes[i].is_valid()) continue; // skeleton node, no scene representation
        if (nodes[i].mesh >= 0 && nodes[i].mesh < (int)meshes.size()) {
            const GLBMesh& glb_mesh = meshes[nodes[i].mesh];
            Ref<VFXMesh> mesh = _build_mesh(glb_mesh, error);
            if (mesh.is_null()) continue;

            vfx_nodes[i]->set_node_type(VFXSceneNode::NODE_MESH);
            vfx_nodes[i]->set_mesh(mesh);
            mesh_nodes_array.append(vfx_nodes[i]);

            if (!first_mesh.is_valid()) first_mesh = mesh;

            // Skin
            if (nodes[i].skin >= 0 && nodes[i].skin < (int)skins.size()) {
                int skin_idx = nodes[i].skin;

                if (!built_skeletons[skin_idx].is_valid()) {
                    built_skeletons[skin_idx] = _build_skeleton(skins[skin_idx], error);
                }

                if (built_skeletons[skin_idx].is_valid()) {
                    Ref<VFXSkin> vfx_skin;
                    vfx_skin.instantiate();
                    vfx_skin->set_mesh(mesh);
                    vfx_skin->set_skeleton(built_skeletons[skin_idx]);
                    vfx_nodes[i]->set_skeleton(built_skeletons[skin_idx]);
                    vfx_nodes[i]->set_skin(vfx_skin);

                    if (!first_skeleton.is_valid()) first_skeleton = built_skeletons[skin_idx];
                    if (!first_skin.is_valid()) first_skin = vfx_skin;

                    // Build animator once per skin
                    if (!built_animators[skin_idx].is_valid()) {
                        Ref<VFXAnimator> animator;
                        animator.instantiate();
                        std::vector<int> node_to_bone(nodes.size(), -1);
                        for (int j = 0; j < skins[skin_idx].joints.size(); j++) {
                            int joint_node = skins[skin_idx].joints[j];
                            if (joint_node >= 0 && joint_node < (int)nodes.size()) {
                                node_to_bone[joint_node] = j;
                            }
                        }
                        _build_animations(animator, node_to_bone);
                        if (animator->get_clip_count() > 0) {
                            built_animators[skin_idx] = animator;
                        }
                    }
                    if (built_animators[skin_idx].is_valid()) {
                        vfx_nodes[i]->set_animator(built_animators[skin_idx]);
                        if (!first_animator.is_valid()) first_animator = built_animators[skin_idx];
                    }
                }
            }
        }
    }

    result[KEY_SCENE] = scene;
    result[KEY_MESH_NODES] = mesh_nodes_array;
    if (first_mesh.is_valid()) result[KEY_MESH] = first_mesh;
    if (first_skeleton.is_valid()) result[KEY_SKELETON] = first_skeleton;
    if (first_skin.is_valid()) result[KEY_SKIN] = first_skin;
    if (first_animator.is_valid()) result[KEY_ANIMATOR] = first_animator;
    result[KEY_SUCCESS] = true;
    return result;
}

bool VFXGLBImporter::_parse_glb(const PackedByteArray& data, String& out_error) {
    _clear_document();

    if (data.size() < 12) {
        out_error = "File too small for GLB header";
        return false;
    }

    uint32_t magic = *reinterpret_cast<const uint32_t*>(data.ptr());
    uint32_t version = *reinterpret_cast<const uint32_t*>(data.ptr() + 4);
    uint32_t length = *reinterpret_cast<const uint32_t*>(data.ptr() + 8);

    if (magic != 0x46546C67) {
        out_error = "Not a valid GLB file (bad magic)";
        return false;
    }
    if (version != 2) {
        out_error = "Unsupported GLB version: " + String::num_int64(version);
        return false;
    }
    if ((int)length != data.size()) {
        out_error = "GLB length mismatch";
        return false;
    }

    int offset = 12;
    PackedByteArray json_chunk;
    PackedByteArray bin_chunk;

    while (offset < data.size()) {
        if (offset + 8 > data.size()) {
            out_error = "Truncated chunk header";
            return false;
        }
        uint32_t chunk_len = *reinterpret_cast<const uint32_t*>(data.ptr() + offset);
        uint32_t chunk_type = *reinterpret_cast<const uint32_t*>(data.ptr() + offset + 4);
        offset += 8;

        if (offset + (int)chunk_len > data.size()) {
            out_error = "Chunk exceeds file bounds";
            return false;
        }

        if (chunk_type == 0x4E4F534A) {
            json_chunk.resize(chunk_len);
            memcpy(json_chunk.ptrw(), data.ptr() + offset, chunk_len);
        } else if (chunk_type == 0x004E4942) {
            bin_chunk.resize(chunk_len);
            memcpy(bin_chunk.ptrw(), data.ptr() + offset, chunk_len);
        }
        offset += chunk_len;
    }

    if (json_chunk.is_empty()) {
        out_error = "No JSON chunk found";
        return false;
    }

    String json_text;
    json_text.parse_utf8((const char*)json_chunk.ptr(), json_chunk.size());
    return _parse_json(json_text, bin_chunk, out_error);
}

bool VFXGLBImporter::_parse_json(const String& json_text, const PackedByteArray& bin_chunk, String& out_error) {
    Ref<JSON> json;
    json.instantiate();
    Error err = json->parse(json_text);
    if (err != OK) {
        out_error = "JSON parse error: " + json->get_error_message() + " at line " + String::num_int64(json->get_error_line());
        return false;
    }

    Dictionary doc = json->get_data();
    if (!doc.has("asset")) {
        out_error = "Missing asset in glTF";
        return false;
    }

    Dictionary asset = doc["asset"];
    String version = asset.get("version", "2.0");
    if (!version.begins_with("2.")) {
        out_error = "Unsupported glTF version: " + version;
        return false;
    }

    if (doc.has("scene")) {
        int scene_idx = doc["scene"];
        if (doc.has("scenes")) {
            Array scenes = doc["scenes"];
            if (scene_idx >= 0 && scene_idx < scenes.size()) {
                Dictionary scene = scenes[scene_idx];
                if (scene.has("nodes")) {
                    Array nodes_arr = scene["nodes"];
                    for (int i = 0; i < nodes_arr.size(); i++) {
                        scene_nodes.push_back(nodes_arr[i]);
                    }
                }
            }
        }
    }

    if (doc.has("nodes")) {
        if (!_parse_nodes(doc["nodes"])) {
            out_error = "Failed to parse nodes";
            return false;
        }
    }
    if (doc.has("meshes")) {
        if (!_parse_meshes(doc["meshes"])) {
            out_error = "Failed to parse meshes";
            return false;
        }
    }
    if (doc.has("skins")) {
        if (!_parse_skins(doc["skins"])) {
            out_error = "Failed to parse skins";
            return false;
        }
    }
    if (doc.has("animations")) {
        if (!_parse_animations(doc["animations"])) {
            out_error = "Failed to parse animations";
            return false;
        }
    }
    if (doc.has("accessors")) {
        if (!_parse_accessors(doc["accessors"])) {
            out_error = "Failed to parse accessors";
            return false;
        }
    }
    if (doc.has("bufferViews")) {
        if (!_parse_buffer_views(doc["bufferViews"])) {
            out_error = "Failed to parse buffer views";
            return false;
        }
    }
    if (doc.has("buffers")) {
        if (!_parse_buffers(doc["buffers"], bin_chunk)) {
            out_error = "Failed to parse buffers";
            return false;
        }
    }

    return true;
}

bool VFXGLBImporter::_parse_nodes(const Array& nodes_arr) {
    for (int i = 0; i < nodes_arr.size(); i++) {
        Dictionary n = nodes_arr[i];
        GLBNode node;
        node.name = n.get("name", "node_" + String::num_int64(i));
        if (n.has("mesh")) node.mesh = n["mesh"];
        if (n.has("skin")) node.skin = n["skin"];
        if (n.has("children")) {
            Array c = n["children"];
            for (int j = 0; j < c.size(); j++) {
                node.children.push_back(c[j]);
            }
        }
        if (n.has("translation")) {
            Array t = n["translation"];
            node.translation = Vector3(t[0], t[1], t[2]);
        }
        if (n.has("rotation")) {
            Array r = n["rotation"];
            node.rotation = Quaternion(r[0], r[1], r[2], r[3]);
        }
        if (n.has("scale")) {
            Array s = n["scale"];
            node.scale = Vector3(s[0], s[1], s[2]);
        }
        if (n.has("matrix")) {
            Array m = n["matrix"];
            Basis b;
            b[0] = Vector3(m[0], m[1], m[2]);
            b[1] = Vector3(m[4], m[5], m[6]);
            b[2] = Vector3(m[8], m[9], m[10]);
            node.matrix = Transform3D(b, Vector3(m[12], m[13], m[14]));
            node.has_matrix = true;
        }
        nodes.push_back(node);
    }
    for (int i = 0; i < (int)nodes.size(); i++) {
        for (int child : nodes[i].children) {
            if (child >= 0 && child < (int)nodes.size()) {
                nodes[child].parent = i;
            }
        }
    }
    return true;
}

bool VFXGLBImporter::_parse_meshes(const Array& meshes_arr) {
    for (int i = 0; i < meshes_arr.size(); i++) {
        Dictionary m = meshes_arr[i];
        GLBMesh mesh;
        mesh.name = m.get("name", "mesh_" + String::num_int64(i));
        if (m.has("primitives")) {
            Array prims = m["primitives"];
            for (int j = 0; j < prims.size(); j++) {
                Dictionary p = prims[j];
                GLBPrimitive prim;
                if (p.has("attributes")) prim.attributes = p["attributes"];
                if (p.has("indices")) prim.indices = p["indices"];
                if (p.has("mode")) prim.mode = p["mode"];
                if (p.has("material")) prim.material = p["material"];
                mesh.primitives.push_back(prim);
            }
        }
        meshes.push_back(mesh);
    }
    return true;
}

bool VFXGLBImporter::_parse_skins(const Array& skins_arr) {
    for (int i = 0; i < skins_arr.size(); i++) {
        Dictionary s = skins_arr[i];
        GLBSkin skin;
        if (s.has("inverseBindMatrices")) skin.inverse_bind_matrices = s["inverseBindMatrices"];
        if (s.has("joints")) {
            Array j = s["joints"];
            for (int k = 0; k < j.size(); k++) skin.joints.push_back(j[k]);
        }
        if (s.has("skeleton")) skin.skeleton_root = s["skeleton"];
        skins.push_back(skin);
    }
    return true;
}

bool VFXGLBImporter::_parse_animations(const Array& anims_arr) {
    for (int i = 0; i < anims_arr.size(); i++) {
        Dictionary a = anims_arr[i];
        GLBAnimation anim;
        anim.name = a.get("name", "anim_" + String::num_int64(i));
        if (a.has("samplers")) {
            Array samps = a["samplers"];
            for (int j = 0; j < samps.size(); j++) {
                Dictionary s = samps[j];
                GLBAnimationSampler sampler;
                if (s.has("input")) sampler.input = s["input"];
                if (s.has("output")) sampler.output = s["output"];
                if (s.has("interpolation")) sampler.interpolation = s["interpolation"];
                anim.samplers.push_back(sampler);
            }
        }
        if (a.has("channels")) {
            Array chans = a["channels"];
            for (int j = 0; j < chans.size(); j++) {
                Dictionary c = chans[j];
                GLBAnimationChannel channel;
                if (c.has("sampler")) channel.sampler = c["sampler"];
                if (c.has("target")) {
                    Dictionary t = c["target"];
                    if (t.has("node")) channel.target_node = t["node"];
                    if (t.has("path")) channel.target_path = t["path"];
                }
                anim.channels.push_back(channel);
            }
        }
        animations.push_back(anim);
    }
    return true;
}

bool VFXGLBImporter::_parse_accessors(const Array& acc_arr) {
    for (int i = 0; i < acc_arr.size(); i++) {
        Dictionary a = acc_arr[i];
        GLBAccessor acc;
        acc.buffer_view = a.get("bufferView", -1);
        acc.byte_offset = a.get("byteOffset", 0);
        acc.component_type = a["componentType"];
        acc.count = a["count"];
        acc.type = a["type"];
        acc.normalized = a.get("normalized", false);
        if (a.has("min")) {
            Array m = a["min"];
            for (int j = 0; j < m.size(); j++) acc.min.append(m[j]);
        }
        if (a.has("max")) {
            Array m = a["max"];
            for (int j = 0; j < m.size(); j++) acc.max.append(m[j]);
        }
        accessors.push_back(acc);
    }
    return true;
}

bool VFXGLBImporter::_parse_buffer_views(const Array& bv_arr) {
    for (int i = 0; i < bv_arr.size(); i++) {
        Dictionary b = bv_arr[i];
        GLBBufferView bv;
        bv.buffer = b["buffer"];
        bv.byte_offset = b.get("byteOffset", 0);
        bv.byte_length = b["byteLength"];
        bv.byte_stride = b.get("byteStride", 0);
        bv.target = b.get("target", 0);
        buffer_views.push_back(bv);
    }
    return true;
}

bool VFXGLBImporter::_parse_buffers(const Array& buf_arr, const PackedByteArray& bin_chunk) {
    for (int i = 0; i < buf_arr.size(); i++) {
        Dictionary b = buf_arr[i];
        GLBBuffer buf;
        if (b.has("uri")) {
            // External / data URI not supported
        } else if (i == 0 && !bin_chunk.is_empty()) {
            buf.data = bin_chunk;
        }
        buffers.push_back(buf);
    }
    return true;
}


int VFXGLBImporter::_accessor_component_count(const String& type) const {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT2") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 1;
}

int VFXGLBImporter::_accessor_component_size(int component_type) const {
    switch (component_type) {
        case 5120: return 1;
        case 5121: return 1;
        case 5122: return 2;
        case 5123: return 2;
        case 5125: return 4;
        case 5126: return 4;
        default: return 1;
    }
}

PackedByteArray VFXGLBImporter::_read_accessor_raw(int accessor_idx, String& out_error) {
    PackedByteArray empty;
    if (accessor_idx < 0 || accessor_idx >= (int)accessors.size()) {
        out_error = "Invalid accessor index";
        return empty;
    }
    const GLBAccessor& acc = accessors[accessor_idx];
    if (acc.buffer_view < 0 || acc.buffer_view >= (int)buffer_views.size()) {
        out_error = "Invalid buffer view index";
        return empty;
    }
    const GLBBufferView& bv = buffer_views[acc.buffer_view];
    if (bv.buffer < 0 || bv.buffer >= (int)buffers.size()) {
        out_error = "Invalid buffer index";
        return empty;
    }
    const GLBBuffer& buf = buffers[bv.buffer];

    int comp_size = _accessor_component_size(acc.component_type);
    int comp_count = _accessor_component_count(acc.type);
    int element_size = comp_size * comp_count;
    int stride = bv.byte_stride > 0 ? bv.byte_stride : element_size;

    int total_bytes = acc.count * element_size;
    PackedByteArray result;
    result.resize(total_bytes);

    int src_offset = bv.byte_offset + acc.byte_offset;
    int dst_offset = 0;

    for (int i = 0; i < acc.count; i++) {
        if (src_offset + element_size > buf.data.size()) {
            out_error = "Accessor read out of bounds";
            return empty;
        }
        memcpy(result.ptrw() + dst_offset, buf.data.ptr() + src_offset, element_size);
        src_offset += stride;
        dst_offset += element_size;
    }

    return result;
}

bool VFXGLBImporter::_read_accessor_floats(int accessor_idx, PackedFloat32Array& out, String& out_error) {
    out.clear();
    if (accessor_idx < 0) return true;
    PackedByteArray raw = _read_accessor_raw(accessor_idx, out_error);
    if (raw.is_empty() && !out_error.is_empty()) return false;

    const GLBAccessor& acc = accessors[accessor_idx];
    int comp_count = _accessor_component_count(acc.type);
    int total_elements = acc.count * comp_count;

    out.resize(total_elements);

    if (acc.component_type == 5126) {
        memcpy(out.ptrw(), raw.ptr(), total_elements * 4);
    } else if (acc.component_type == 5121) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < total_elements; i++) {
            out[i] = acc.normalized ? (p[i] / 255.0f) : (float)p[i];
        }
    } else if (acc.component_type == 5123) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < total_elements; i++) {
            uint16_t v = p[i*2] | (p[i*2+1] << 8);
            out[i] = acc.normalized ? (v / 65535.0f) : (float)v;
        }
    } else if (acc.component_type == 5125) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < total_elements; i++) {
            uint32_t v = p[i*4] | (p[i*4+1] << 8) | (p[i*4+2] << 16) | (p[i*4+3] << 24);
            out[i] = (float)v;
        }
    } else if (acc.component_type == 5120) {
        const int8_t* p = reinterpret_cast<const int8_t*>(raw.ptr());
        for (int i = 0; i < total_elements; i++) {
            out[i] = acc.normalized ? (MAX(p[i], (int8_t)-127) / 127.0f) : (float)p[i];
        }
    } else if (acc.component_type == 5122) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < total_elements; i++) {
            int16_t v = (int16_t)(p[i*2] | (p[i*2+1] << 8));
            out[i] = acc.normalized ? (MAX(v, (int16_t)-32767) / 32767.0f) : (float)v;
        }
    } else {
        out_error = "Unsupported component type: " + String::num_int64(acc.component_type);
        return false;
    }

    return true;
}

bool VFXGLBImporter::_read_accessor_vec3(int accessor_idx, PackedVector3Array& out, String& out_error) {
    out.clear();
    PackedFloat32Array floats;
    if (!_read_accessor_floats(accessor_idx, floats, out_error)) return false;
    const GLBAccessor& acc = accessors[accessor_idx];
    if (_accessor_component_count(acc.type) != 3) {
        out_error = "Accessor is not VEC3";
        return false;
    }
    out.resize(acc.count);
    for (int i = 0; i < acc.count; i++) {
        out[i] = Vector3(floats[i*3], floats[i*3+1], floats[i*3+2]);
    }
    return true;
}

bool VFXGLBImporter::_read_accessor_vec2(int accessor_idx, PackedVector2Array& out, String& out_error) {
    out.clear();
    PackedFloat32Array floats;
    if (!_read_accessor_floats(accessor_idx, floats, out_error)) return false;
    const GLBAccessor& acc = accessors[accessor_idx];
    if (_accessor_component_count(acc.type) != 2) {
        out_error = "Accessor is not VEC2";
        return false;
    }
    out.resize(acc.count);
    for (int i = 0; i < acc.count; i++) {
        out[i] = Vector2(floats[i*2], floats[i*2+1]);
    }
    return true;
}

bool VFXGLBImporter::_read_accessor_indices(int accessor_idx, PackedInt32Array& out, String& out_error) {
    out.clear();
    if (accessor_idx < 0) return true;
    PackedByteArray raw = _read_accessor_raw(accessor_idx, out_error);
    if (raw.is_empty() && !out_error.is_empty()) return false;

    const GLBAccessor& acc = accessors[accessor_idx];
    out.resize(acc.count);

    if (acc.component_type == 5123) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < acc.count; i++) {
            out[i] = p[i*2] | (p[i*2+1] << 8);
        }
    } else if (acc.component_type == 5125) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < acc.count; i++) {
            out[i] = p[i*4] | (p[i*4+1] << 8) | (p[i*4+2] << 16) | (p[i*4+3] << 24);
        }
    } else if (acc.component_type == 5121) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < acc.count; i++) out[i] = p[i];
    } else {
        out_error = "Unsupported index component type";
        return false;
    }
    return true;
}

bool VFXGLBImporter::_read_accessor_vec4i(int accessor_idx, std::vector<Vector4i>& out, String& out_error) {
    out.clear();
    if (accessor_idx < 0) return true;
    PackedByteArray raw = _read_accessor_raw(accessor_idx, out_error);
    if (raw.is_empty() && !out_error.is_empty()) return false;

    const GLBAccessor& acc = accessors[accessor_idx];
    if (_accessor_component_count(acc.type) != 4) {
        out_error = "Accessor is not VEC4";
        return false;
    }
    out.resize(acc.count);

    if (acc.component_type == 5121) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < acc.count; i++) {
            out[i] = Vector4i(p[i*4], p[i*4+1], p[i*4+2], p[i*4+3]);
        }
    } else if (acc.component_type == 5123) {
        const uint8_t* p = raw.ptr();
        for (int i = 0; i < acc.count; i++) {
            out[i] = Vector4i(
                p[i*8] | (p[i*8+1] << 8),
                p[i*8+2] | (p[i*8+3] << 8),
                p[i*8+4] | (p[i*8+5] << 8),
                p[i*8+6] | (p[i*8+7] << 8)
            );
        }
    } else {
        out_error = "Unsupported joints component type";
        return false;
    }
    return true;
}

bool VFXGLBImporter::_read_accessor_vec4f(int accessor_idx, std::vector<Vector4>& out, String& out_error) {
    out.clear();
    PackedFloat32Array floats;
    if (!_read_accessor_floats(accessor_idx, floats, out_error)) return false;
    const GLBAccessor& acc = accessors[accessor_idx];
    if (_accessor_component_count(acc.type) != 4) {
        out_error = "Accessor is not VEC4";
        return false;
    }
    out.resize(acc.count);
    for (int i = 0; i < acc.count; i++) {
        out[i] = Vector4(floats[i*4], floats[i*4+1], floats[i*4+2], floats[i*4+3]);
    }
    return true;
}

bool VFXGLBImporter::_read_accessor_mat4(int accessor_idx, std::vector<Transform3D>& out, String& out_error) {
    out.clear();
    PackedFloat32Array floats;
    if (!_read_accessor_floats(accessor_idx, floats, out_error)) return false;
    const GLBAccessor& acc = accessors[accessor_idx];
    if (_accessor_component_count(acc.type) != 16) {
        out_error = "Accessor is not MAT4";
        return false;
    }
    out.resize(acc.count);
    for (int i = 0; i < acc.count; i++) {
        int b = i * 16;
        Basis basis;
        basis[0] = Vector3(floats[b+0], floats[b+1], floats[b+2]);
        basis[1] = Vector3(floats[b+4], floats[b+5], floats[b+6]);
        basis[2] = Vector3(floats[b+8], floats[b+9], floats[b+10]);
        Vector3 origin(floats[b+12], floats[b+13], floats[b+14]);
        out[i] = Transform3D(basis, origin);
    }
    return true;
}

Ref<VFXMesh> VFXGLBImporter::_build_mesh(const GLBMesh& glb_mesh, String& out_error) {
    Ref<VFXMesh> mesh;
    mesh.instantiate();

    for (const GLBPrimitive& prim : glb_mesh.primitives) {
        if (prim.mode != 4) continue;

        int pos_acc = -1, norm_acc = -1, uv_acc = -1, col_acc = -1;
        int joints_acc = -1, weights_acc = -1;

        Array attr_keys = prim.attributes.keys();
        for (int i = 0; i < attr_keys.size(); i++) {
            String key = attr_keys[i];
            int idx = prim.attributes[key];
            if (key == "POSITION") pos_acc = idx;
            else if (key == "NORMAL") norm_acc = idx;
            else if (key == "TEXCOORD_0") uv_acc = idx;
            else if (key == "COLOR_0") col_acc = idx;
            else if (key == "JOINTS_0") joints_acc = idx;
            else if (key == "WEIGHTS_0") weights_acc = idx;
        }

        if (pos_acc < 0) {
            out_error = "Primitive missing POSITION attribute";
            return Ref<VFXMesh>();
        }

        PackedVector3Array positions;
        if (!_read_accessor_vec3(pos_acc, positions, out_error)) {
            return Ref<VFXMesh>();
        }

        PackedVector3Array normals;
        if (norm_acc >= 0) {
            if (!_read_accessor_vec3(norm_acc, normals, out_error)) return Ref<VFXMesh>();
        }

        PackedVector2Array uvs;
        if (uv_acc >= 0) {
            if (!_read_accessor_vec2(uv_acc, uvs, out_error)) return Ref<VFXMesh>();
        }

        PackedInt32Array indices;
        if (prim.indices >= 0) {
            if (!_read_accessor_indices(prim.indices, indices, out_error)) return Ref<VFXMesh>();
        }

        std::vector<Vector4> colors;
        if (col_acc >= 0) {
            std::vector<Vector4> cols;
            if (!_read_accessor_vec4f(col_acc, cols, out_error)) return Ref<VFXMesh>();
            colors = cols;
        }

        std::vector<Vector4i> joints;
        std::vector<Vector4> weights;
        if (joints_acc >= 0) {
            if (!_read_accessor_vec4i(joints_acc, joints, out_error)) return Ref<VFXMesh>();
        }
        if (weights_acc >= 0) {
            if (!_read_accessor_vec4f(weights_acc, weights, out_error)) return Ref<VFXMesh>();
        }

        for (int i = 0; i < positions.size(); i++) {
            positions[i] = _gltf_to_godot_v3(positions[i]);
        }
        for (int i = 0; i < normals.size(); i++) {
            normals[i] = _gltf_to_godot_n(normals[i]);
        }
        for (int i = 0; i < uvs.size(); i++) {
            uvs[i].y = 1.0f - uvs[i].y;
        }

        int vert_base = mesh->get_vertex_count();
        for (int i = 0; i < positions.size(); i++) {
            Vector2 uv = (i < uvs.size()) ? uvs[i] : Vector2();
            Color col = Color(1,1,1,1);
            if (i < (int)colors.size()) {
                col = Color(colors[i].x, colors[i].y, colors[i].z, colors[i].w);
            }
            mesh->add_vertex(positions[i], uv, col);
        }

        if (!joints.empty() && !weights.empty()) {
            for (int i = 0; i < positions.size() && i < (int)joints.size() && i < (int)weights.size(); i++) {
                mesh->set_vertex_bones(vert_base + i, joints[i].x, joints[i].y, joints[i].z, joints[i].w);
                mesh->set_vertex_weights(vert_base + i, weights[i].x, weights[i].y, weights[i].z, weights[i].w);
            }
        }

        if (indices.is_empty()) {
            for (int i = 0; i < positions.size(); i += 3) {
                if (i + 2 < positions.size()) {
                    mesh->add_triangle(vert_base + i, vert_base + i + 1, vert_base + i + 2);
                }
            }
        } else {
            for (int i = 0; i < indices.size(); i += 3) {
                if (i + 2 < indices.size()) {
                    mesh->add_triangle(vert_base + indices[i], vert_base + indices[i+1], vert_base + indices[i+2]);
                }
            }
        }
    }

    mesh->recalculate_normals();
    mesh->recalculate_bounds();
    return mesh;
}

Ref<VFXSkeleton> VFXGLBImporter::_build_skeleton(const GLBSkin& glb_skin, String& out_error) {
    Ref<VFXSkeleton> skeleton;
    skeleton.instantiate();

    int joint_count = glb_skin.joints.size();
    if (joint_count == 0) {
        out_error = "Skin has no joints";
        return Ref<VFXSkeleton>();
    }

    std::vector<Transform3D> ibms;
    if (glb_skin.inverse_bind_matrices >= 0) {
        if (!_read_accessor_mat4(glb_skin.inverse_bind_matrices, ibms, out_error)) {
            return Ref<VFXSkeleton>();
        }
    }
    if ((int)ibms.size() < joint_count) {
        ibms.resize(joint_count);
        for (int i = 0; i < joint_count; i++) ibms[i] = Transform3D();
    }

    for (int i = 0; i < joint_count; i++) {
        int node_idx = glb_skin.joints[i];
        String name = (node_idx >= 0 && node_idx < (int)nodes.size()) ? nodes[node_idx].name : ("bone_" + String::num_int64(i));
        int parent_bone = -1;
        if (node_idx >= 0 && node_idx < (int)nodes.size()) {
            int parent_node = nodes[node_idx].parent;
            for (int j = 0; j < joint_count; j++) {
                if (glb_skin.joints[j] == parent_node) {
                    parent_bone = j;
                    break;
                }
            }
        }
        skeleton->add_bone(name, parent_bone);
    }

    std::vector<Transform3D> bind_poses(joint_count);
    for (int i = 0; i < joint_count; i++) {
        Transform3D ibm = _gltf_to_godot_t(ibms[i]);
        bind_poses[i] = ibm.affine_inverse();
    }

    for (int i = 0; i < joint_count; i++) {
        skeleton->set_bone_bind_pose(i, bind_poses[i]);

        int parent = skeleton->get_bone_parent(i);
        Transform3D local;
        if (parent >= 0) {
            Transform3D parent_inv = bind_poses[parent].affine_inverse();
            local = parent_inv * bind_poses[i];
        } else {
            local = bind_poses[i];
        }

        skeleton->set_bone_local_position(i, local.get_origin());
        skeleton->set_bone_local_rotation(i, local.get_basis().get_rotation_quaternion());
        skeleton->set_bone_local_scale(i, local.get_basis().get_scale());
    }

    skeleton->update_transforms();
    return skeleton;
}

void VFXGLBImporter::_build_animations(Ref<VFXAnimator> animator, const std::vector<int>& node_to_bone) {
    for (const GLBAnimation& anim : animations) {
        float duration = 0.0f;
        for (const GLBAnimationSampler& sampler : anim.samplers) {
            if (sampler.input >= 0 && sampler.input < (int)accessors.size()) {
                PackedFloat32Array times;
                String err;
                if (_read_accessor_floats(sampler.input, times, err) && !times.is_empty()) {
                    duration = MAX(duration, times[times.size() - 1]);
                }
            }
        }
        if (duration <= 0.0f) duration = 1.0f;

        int clip_idx = animator->create_clip(anim.name, duration, 30.0f);

        for (const GLBAnimationChannel& channel : anim.channels) {
            if (channel.target_node < 0 || channel.target_node >= (int)nodes.size()) continue;
            if (channel.sampler < 0 || channel.sampler >= (int)anim.samplers.size()) continue;

            int bone_id = node_to_bone[channel.target_node];
            if (bone_id < 0) continue;

            const GLBAnimationSampler& sampler = anim.samplers[channel.sampler];
            if (sampler.input < 0 || sampler.output < 0) continue;

            PackedFloat32Array times;
            PackedFloat32Array values;
            String err;
            if (!_read_accessor_floats(sampler.input, times, err)) continue;
            if (!_read_accessor_floats(sampler.output, values, err)) continue;

            bool is_rotation = (channel.target_path == "rotation");
            bool is_scale = (channel.target_path == "scale");
            bool is_translation = (channel.target_path == "translation");
            if (!is_rotation && !is_scale && !is_translation) continue;

            String curve_name = nodes[channel.target_node].name + "_" + channel.target_path;
            int curve_idx = animator->add_curve(clip_idx, curve_name, bone_id, is_rotation, is_scale);

            int comp_count = is_rotation ? 4 : 3;
            if ((int)values.size() < times.size() * comp_count) continue;

            int interp = VFXAnimator::INTERP_LINEAR;
            if (sampler.interpolation == "STEP") interp = VFXAnimator::INTERP_STEP;

            for (int k = 0; k < times.size(); k++) {
                float t = times[k];
                int base = k * comp_count;

                if (is_rotation) {
                    Quaternion q(values[base], values[base+1], values[base+2], values[base+3]);
                    q = _gltf_to_godot_q(q);
                    animator->add_keyframe_quaternion(clip_idx, curve_idx, t, q, interp);
                } else if (is_scale) {
                    Vector3 s(values[base], values[base+1], values[base+2]);
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, s, interp);
                } else {
                    Vector3 p(values[base], values[base+1], values[base+2]);
                    p = _gltf_to_godot_v3(p);
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, p, interp);
                }
            }
        }
    }
}