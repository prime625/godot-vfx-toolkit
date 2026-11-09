#include "vfx_glb_importer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/vector4i.hpp>
#include <cmath>
#include <cstring>
#include <functional>
#include <unordered_set>

using namespace godot;

// Local turn constants so the smoothing code below does not depend on
// godot_cpp math_defs include order.
static constexpr float VFX_PI  = 3.14159265358979323846f;
static constexpr float VFX_TAU = 6.28318530717958647692f;

void VFXGLBImporter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("import_glb", "path", "smooth_linear_curves"), &VFXGLBImporter::import_glb, DEFVAL(false));
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
    images.clear();
    samplers.clear();
    textures.clear();
    materials.clear();
    godot_materials.clear();
    decoded_textures.clear();
    scene_nodes.clear();
}

Dictionary VFXGLBImporter::import_glb(const String& path, bool smooth_linear_curves) {
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
    result[KEY_MATERIALS] = Array();
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

    // Build Godot materials (and decode embedded textures) up front so
    // _build_mesh can attach them. Texture problems are non-fatal.
    _build_materials();
    result[KEY_MATERIAL_COUNT] = (int)godot_materials.size();
    Array materials_array;
    for (const Ref<StandardMaterial3D>& m : godot_materials) {
        if (m.is_valid()) materials_array.append(m);
    }
    result[KEY_MATERIALS] = materials_array;

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
        if (skeleton_node_set.count(i)) continue; // skip bones & bone descendant
        vfx_nodes[i].instantiate();
        vfx_nodes[i]->set_node_id(scene->get_unique_id());
        vfx_nodes[i]->set_node_name(nodes[i].name);
        vfx_nodes[i]->set_local_transform(_get_node_local_transform(nodes[i]));
        vfx_nodes[i]->set_node_type(VFXSceneNode::NODE_EMPTY);
    }
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

            // Some exporters store skinned vertices in SCENE space and write
            // IBMs as pure inverse joint globals (inverseBindMatrix = inv(G_joint),
            // without the mesh node transform). Per glTF spec the skinned mesh
            // node's own transform is then ignored. Our pipeline renders the
            // mesh under its node and skins in mesh-node space, so for those
            // files we must bake the inverse of the mesh node's global into
            // the vertices to bring them into mesh-node space.
            Transform3D vertex_space; // identity
            if (nodes[i].skin >= 0 && nodes[i].skin < (int)skins.size()
                    && _skin_uses_scene_space_ibms(skins[nodes[i].skin], i)) {
                int n = i;
                Transform3D owner_global;
                while (n >= 0 && n < (int)nodes.size()) {
                    owner_global = _get_node_local_transform(nodes[n]) * owner_global;
                    n = nodes[n].parent;
                }
                vertex_space = owner_global.affine_inverse();
            }

            Ref<VFXMesh> mesh = _build_mesh(glb_mesh, error, vertex_space);
            if (mesh.is_null()) continue;

            vfx_nodes[i]->set_node_type(VFXSceneNode::NODE_MESH);
            vfx_nodes[i]->set_mesh(mesh);
            mesh_nodes_array.append(vfx_nodes[i]);

            if (!first_mesh.is_valid()) first_mesh = mesh;

            // Skin
            if (nodes[i].skin >= 0 && nodes[i].skin < (int)skins.size()) {
                int skin_idx = nodes[i].skin;

                if (!built_skeletons[skin_idx].is_valid()) {
                    built_skeletons[skin_idx] = _build_skeleton(skins[skin_idx], i, error);
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
                        _build_animations(animator, node_to_bone, smooth_linear_curves);
                        built_animators[skin_idx] = animator;

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
    if (doc.has("images")) {
        if (!_parse_images(doc["images"])) {
            out_error = "Failed to parse images";
            return false;
        }
    }
    if (doc.has("samplers")) {
        if (!_parse_samplers(doc["samplers"])) {
            out_error = "Failed to parse samplers";
            return false;
        }
    }
    if (doc.has("textures")) {
        if (!_parse_textures(doc["textures"])) {
            out_error = "Failed to parse textures";
            return false;
        }
    }
    if (doc.has("materials")) {
        if (!_parse_materials(doc["materials"])) {
            out_error = "Failed to parse materials";
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
            b[0] = Vector3(m[0], m[4], m[8]);
            b[1] = Vector3(m[1], m[5], m[9]);
            b[2] = Vector3(m[2], m[6], m[10]);
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



bool VFXGLBImporter::_parse_images(const Array& img_arr) {
    for (int i = 0; i < img_arr.size(); i++) {
        Dictionary d = img_arr[i];
        GLBImage img;
        img.name = d.get("name", "image_" + String::num_int64(i));
        img.mime_type = d.get("mimeType", "");
        if (d.has("bufferView")) img.buffer_view = d["bufferView"];
        // External URI images are not supported (same policy as buffers).
        images.push_back(img);
    }
    return true;
}

bool VFXGLBImporter::_parse_samplers(const Array& samp_arr) {
    for (int i = 0; i < samp_arr.size(); i++) {
        Dictionary d = samp_arr[i];
        GLBSampler s;
        s.mag_filter = d.get("magFilter", -1);
        s.min_filter = d.get("minFilter", -1);
        s.wrap_s = d.get("wrapS", 10497);
        s.wrap_t = d.get("wrapT", 10497);
        samplers.push_back(s);
    }
    return true;
}

bool VFXGLBImporter::_parse_textures(const Array& tex_arr) {
    for (int i = 0; i < tex_arr.size(); i++) {
        Dictionary d = tex_arr[i];
        GLBTexture t;
        t.sampler = d.get("sampler", -1);
        t.source = d.get("source", -1);
        textures.push_back(t);
    }
    return true;
}

VFXGLBImporter::GLBTextureInfo VFXGLBImporter::_parse_texture_info(const Dictionary& d) {
    GLBTextureInfo info;
    if (d.has("index")) info.index = d["index"];
    if (d.has("texCoord")) info.tex_coord = d["texCoord"];
    return info;
}

bool VFXGLBImporter::_parse_materials(const Array& mat_arr) {
    for (int i = 0; i < mat_arr.size(); i++) {
        Dictionary d = mat_arr[i];
        GLBMaterial m;
        m.name = d.get("name", "material_" + String::num_int64(i));

        if (d.has("pbrMetallicRoughness")) {
            Dictionary pbr = d["pbrMetallicRoughness"];
            if (pbr.has("baseColorFactor")) {
                Array c = pbr["baseColorFactor"];
                if (c.size() >= 4) {
                    m.pbr.base_color_factor = Color(c[0], c[1], c[2], c[3]);
                }
            }
            if (pbr.has("baseColorTexture")) {
                m.pbr.base_color_texture = _parse_texture_info(pbr["baseColorTexture"]);
            }
            m.pbr.metallic_factor = pbr.get("metallicFactor", 1.0);
            m.pbr.roughness_factor = pbr.get("roughnessFactor", 1.0);
            if (pbr.has("metallicRoughnessTexture")) {
                m.pbr.metallic_roughness_texture = _parse_texture_info(pbr["metallicRoughnessTexture"]);
            }
        }

        if (d.has("normalTexture")) m.normal_texture = _parse_texture_info(d["normalTexture"]);
        if (d.has("emissiveTexture")) m.emissive_texture = _parse_texture_info(d["emissiveTexture"]);
        if (d.has("occlusionTexture")) m.occlusion_texture = _parse_texture_info(d["occlusionTexture"]);
        if (d.has("emissiveFactor")) {
            Array e = d["emissiveFactor"];
            if (e.size() >= 3) m.emissive_factor = Color(e[0], e[1], e[2], 1.0);
        }
        m.alpha_mode = d.get("alphaMode", "OPAQUE");
        m.alpha_cutoff = d.get("alphaCutoff", 0.5);
        m.double_sided = d.get("doubleSided", false);

        if (d.has("extensions")) {
            Dictionary ext = d["extensions"];
            if (ext.has("KHR_materials_unlit")) m.unlit = true;
        }

        materials.push_back(m);
    }
    return true;
}

Ref<Image> VFXGLBImporter::_decode_image(int image_idx) {
    if (image_idx < 0 || image_idx >= (int)images.size()) return Ref<Image>();
    const GLBImage& img = images[image_idx];

    if (img.buffer_view < 0 || img.buffer_view >= (int)buffer_views.size()) {
        UtilityFunctions::push_warning("VFXGLBImporter: image '" + img.name + "' has no embedded bufferView (external URI unsupported), skipping");
        return Ref<Image>();
    }
    const GLBBufferView& bv = buffer_views[img.buffer_view];
    if (bv.buffer < 0 || bv.buffer >= (int)buffers.size()) return Ref<Image>();
    const GLBBuffer& buf = buffers[bv.buffer];
    if (bv.byte_offset + bv.byte_length > buf.data.size()) return Ref<Image>();

    PackedByteArray bytes;
    bytes.resize(bv.byte_length);
    memcpy(bytes.ptrw(), buf.data.ptr() + bv.byte_offset, bv.byte_length);

    Ref<Image> image;
    image.instantiate();
    Error err = FAILED;
    if (img.mime_type == "image/png") {
        err = image->load_png_from_buffer(bytes);
    } else if (img.mime_type == "image/jpeg" || img.mime_type == "image/jpg") {
        err = image->load_jpg_from_buffer(bytes);
    } else {
        // Unknown/missing mimeType: sniff PNG first, then JPEG.
        err = image->load_png_from_buffer(bytes);
        if (err != OK) err = image->load_jpg_from_buffer(bytes);
    }
    if (err != OK || image->is_empty()) {
        UtilityFunctions::push_warning("VFXGLBImporter: failed to decode image '" + img.name + "'");
        return Ref<Image>();
    }
    return image;
}

Ref<ImageTexture> VFXGLBImporter::_get_texture(int texture_idx) {
    if (texture_idx < 0 || texture_idx >= (int)textures.size()) return Ref<ImageTexture>();
    if (decoded_textures[texture_idx].is_valid()) return decoded_textures[texture_idx];

    Ref<ImageTexture> tex;
    Ref<Image> img = _decode_image(textures[texture_idx].source);
    if (img.is_valid()) {
        tex.instantiate();
        tex->set_image(img);
    }
    decoded_textures[texture_idx] = tex;
    return tex;
}

void VFXGLBImporter::_build_materials() {
    godot_materials.clear();
    godot_materials.resize(materials.size());
    decoded_textures.clear();
    decoded_textures.resize(textures.size());

    for (int i = 0; i < (int)materials.size(); i++) {
        const GLBMaterial& m = materials[i];
        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_name(m.name);

        // Base color: factor x texture (vertex COLOR_0 multiplies in too).
        mat->set_albedo(m.pbr.base_color_factor);
        if (m.pbr.base_color_texture.index >= 0) {
            Ref<ImageTexture> tex = _get_texture(m.pbr.base_color_texture.index);
            if (tex.is_valid()) {
                mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
                mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
            }
        }

        mat->set_metallic(m.pbr.metallic_factor);
        mat->set_roughness(m.pbr.roughness_factor);

        if (m.normal_texture.index >= 0) {
            Ref<ImageTexture> tex = _get_texture(m.normal_texture.index);
            if (tex.is_valid()) mat->set_texture(BaseMaterial3D::TEXTURE_NORMAL, tex);
        }

        bool has_emissive_tex = false;
        if (m.emissive_texture.index >= 0) {
            Ref<ImageTexture> tex = _get_texture(m.emissive_texture.index);
            if (tex.is_valid()) {
                mat->set_texture(BaseMaterial3D::TEXTURE_EMISSION, tex);
                has_emissive_tex = true;
            }
        }
        if (has_emissive_tex || m.emissive_factor.r > 0.0f || m.emissive_factor.g > 0.0f || m.emissive_factor.b > 0.0f) {
            // With an emissive texture the factor is baked into the texture;
            // without one it is the plain emissive color.
            mat->set_emission(has_emissive_tex ? Color(1, 1, 1) : m.emissive_factor);
        }

        if (m.alpha_mode == "BLEND") {
            mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
        } else if (m.alpha_mode == "MASK") {
            mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
            mat->set_alpha_scissor_threshold(m.alpha_cutoff);
        }

        mat->set_cull_mode(m.double_sided ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
        if (m.unlit) mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);

        godot_materials[i] = mat;
    }
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
        // glTF MAT4 is COLUMN-major; Godot Basis stores ROWs.
        Basis basis;
        basis[0] = Vector3(floats[b+0], floats[b+4], floats[b+8]);
        basis[1] = Vector3(floats[b+1], floats[b+5], floats[b+9]);
        basis[2] = Vector3(floats[b+2], floats[b+6], floats[b+10]);
        Vector3 origin(floats[b+12], floats[b+13], floats[b+14]);
        out[i] = Transform3D(basis, origin);
    }
    return true;
}

Ref<VFXMesh> VFXGLBImporter::_build_mesh(const GLBMesh& glb_mesh, String& out_error, const Transform3D& vertex_space) {
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

        // Material: VFXMesh is a single soup, so the first primitive that
        // references a valid material wins. Multi-material meshes would
        // need per-primitive surface splitting (TODO).
        if (mesh->material.is_null() && prim.material >= 0 && prim.material < (int)godot_materials.size()) {
            mesh->material = godot_materials[prim.material];
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
        // Bake scene-space skinned vertices into mesh-node space when the
        // file uses IBM = inverse(joint global) convention (see import_glb).
        if (vertex_space != Transform3D()) {
            for (int i = 0; i < positions.size(); i++) {
                positions[i] = vertex_space.xform(positions[i]);
            }
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

// Max abs difference between two transforms (basis rows + origin).
static float _transform_diff(const Transform3D& a, const Transform3D& b) {
    float d = 0.0f;
    for (int i = 0; i < 3; i++) {
        d = MAX(d, (a.basis[i] - b.basis[i]).length());
    }
    d = MAX(d, (a.get_origin() - b.get_origin()).length());
    return d;
}

// Two IBM conventions exist in the wild:
//   standard    : IBM_j = inverse(G_joint) * G_meshNode  (vertices in mesh-node space)
//   scene-space : IBM_j = inverse(G_joint)               (vertices in scene space,
//                                                         mesh node transform ignored)
// Decide by comparing inverse(IBM) against joint globals derived from the node
// TRS hierarchy, with and without the mesh node's global transform applied.
bool VFXGLBImporter::_skin_uses_scene_space_ibms(const GLBSkin& glb_skin, int owner_node_idx) {
    int joint_count = glb_skin.joints.size();
    if (joint_count == 0 || glb_skin.inverse_bind_matrices < 0) return false;

    std::vector<Transform3D> ibms;
    String err;
    if (!_read_accessor_mat4(glb_skin.inverse_bind_matrices, ibms, err)) return false;
    if ((int)ibms.size() < joint_count) return false;

    // Godot-space globals for all nodes (memoized, cycle-guarded)
    std::vector<Transform3D> node_global(nodes.size());
    std::vector<int> global_state(nodes.size(), 0);
    std::function<Transform3D(int)> global_of = [&](int idx) -> Transform3D {
        if (idx < 0 || idx >= (int)nodes.size()) return Transform3D();
        if (global_state[idx] == 2) return node_global[idx];
        if (global_state[idx] == 1) return Transform3D(); // cycle guard
        global_state[idx] = 1;
        Transform3D local = _get_node_local_transform(nodes[idx]);
        Transform3D g = (nodes[idx].parent >= 0) ? global_of(nodes[idx].parent) * local : local;
        node_global[idx] = g;
        global_state[idx] = 2;
        return g;
    };

    // The mesh node may be a SIBLING of the joints (not an ancestor), so it
    // cannot be discovered from the joint list â€” the caller must pass it.
    Transform3D g_mesh_inv = global_of(owner_node_idx).affine_inverse();

    double err_standard = 0.0, err_scene = 0.0;
    int checked = 0;
    int step = MAX(1, joint_count / 8);
    for (int i = 0; i < joint_count && checked < 8; i += step, checked++) {
        int ni = glb_skin.joints[i];
        if (ni < 0 || ni >= (int)nodes.size()) continue;
        Transform3D inv_ibm = _gltf_to_godot_t(ibms[i]).affine_inverse();
        Transform3D g_joint = global_of(ni);
        err_scene    += _transform_diff(inv_ibm, g_joint);
        err_standard += _transform_diff(inv_ibm, g_mesh_inv * g_joint);
    }
    if (checked == 0) return false;
    return err_scene < err_standard;
}

Ref<VFXSkeleton> VFXGLBImporter::_build_skeleton(const GLBSkin& glb_skin, int owner_node_idx, String& out_error) {
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

    // Bind poses must be expressed relative to the node that will OWN the
    // skeleton in the VFX scene â€” the mesh node carrying the skin (import_glb
    // attaches the skeleton to that same node, and the exporter parents the
    // bone roots under it).
    //
    // Two IBM conventions exist in the wild (see _skin_uses_scene_space_ibms):
    //   standard    : IBM_j = inverse(G_joint) * G_meshNode
    //                 -> bind (mesh-node-relative) = inverse(IBM_j)
    //   scene-space : IBM_j = inverse(G_joint), vertices in scene space
    //                 -> bind = inverse(G_meshNode) * inverse(IBM_j)
    // Applying the owner inverse unconditionally double-transforms one of the
    // two conventions, which showed up as the mesh floating ~0.6 above the
    // skeleton (pure Y offset) or bones rotating mirrored.
    bool scene_space_ibms = _skin_uses_scene_space_ibms(glb_skin, owner_node_idx);

    Transform3D owner_global;
    {
        int n = owner_node_idx;
        if (n < 0) {
            // Fallback: nearest non-joint ancestor of the skeleton root
            if (glb_skin.skeleton_root >= 0 && glb_skin.skeleton_root < (int)nodes.size()) {
                n = nodes[glb_skin.skeleton_root].parent;
            } else if (joint_count > 0) {
                n = nodes[glb_skin.joints[0]].parent;
            }
        }
        while (n >= 0 && n < (int)nodes.size()) {
            // Parent-first accumulation: global = L(root) * ... * L(node)
            owner_global = _get_node_local_transform(nodes[n]) * owner_global;
            n = nodes[n].parent;
        }
    }
    Transform3D owner_inv = owner_global.affine_inverse();

    std::vector<Transform3D> bind_poses(joint_count);
    for (int i = 0; i < joint_count; i++) {
        Transform3D ibm = _gltf_to_godot_t(ibms[i]);
        bind_poses[i] = scene_space_ibms
            ? owner_inv * ibm.affine_inverse()
            : ibm.affine_inverse();
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


void VFXGLBImporter::_build_animations(Ref<VFXAnimator> animator, const std::vector<int>& node_to_bone, bool smooth_linear_curves) {
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

            const bool is_rotation = (channel.target_path == "rotation");
            const bool is_scale = (channel.target_path == "scale");
            const bool is_translation = (channel.target_path == "translation");
            if (!is_rotation && !is_scale && !is_translation) continue;

            // This importer currently supports glTF STEP and LINEAR samplers.
            // CUBICSPLINE has 3 output elements per key and needs a separate
            // Hermite/tangent path; do not silently interpret it as ordinary keys.
            if (sampler.interpolation == "CUBICSPLINE") {
                UtilityFunctions::push_warning(
                    "VFXGLBImporter: CUBICSPLINE sampler on '" + nodes[channel.target_node].name +
                    "' is not yet supported; skipping this animation channel.");
                continue;
            }

            const String curve_name = nodes[channel.target_node].name + "_" + channel.target_path;
            const int curve_idx = animator->add_curve(clip_idx, curve_name, bone_id, is_rotation, is_scale);

            const int comp_count = is_rotation ? 4 : 3;
            if ((int)values.size() < times.size() * comp_count) {
                UtilityFunctions::push_warning("VFXGLBImporter: animation output is shorter than its input times for '" + curve_name + "'.");
                continue;
            }

            const bool is_step = sampler.interpolation == "STEP";
            // Smooth every sampled track, including rotation: keys are stored
            // as continuous (unwrapped) Euler with auto-tangent Bezier handles.
            // Quaternion slerp playback only applies to INTERP_LINEAR keys, so
            // BEZIER rotation keys automatically use the Euler-backed path in
            // AnimationCurve::sample_quaternion().
            const bool smooth_this_track = smooth_linear_curves && !is_step && times.size() > 1;
            const int interp = is_step ? VFXAnimator::INTERP_STEP
                                       : (smooth_this_track ? VFXAnimator::INTERP_BEZIER : VFXAnimator::INTERP_LINEAR);

            // TEMP DEBUG: verify the importer receives the smoothing option and
            // chooses the expected interpolation for each imported animation track.
            const String interp_name = interp == VFXAnimator::INTERP_BEZIER ? "BEZIER"
                                     : (interp == VFXAnimator::INTERP_STEP ? "STEP" : "LINEAR");
            UtilityFunctions::print(
                String("[GLB DEBUG] Track: ") + curve_name +
                " | glTF sampler: " + sampler.interpolation +
                " | smooth requested: " + (smooth_linear_curves ? "true" : "false") +
                " | smooth applied: " + (smooth_this_track ? "true" : "false") +
                " | keys: " + String::num_int64(times.size()) +
                " | chosen interpolation: " + interp_name +
                " (" + String::num_int64(interp) + ")"
            );

            for (int k = 0; k < times.size(); k++) {
                const float t = times[k];
                const int base = k * comp_count;

                if (is_rotation) {
                    Quaternion q(values[base], values[base + 1], values[base + 2], values[base + 3]);
                    q = _gltf_to_godot_q(q).normalized();
                    animator->add_keyframe_quaternion(clip_idx, curve_idx, t, q, interp);
                } else if (is_scale) {
                    const Vector3 scale(values[base], values[base + 1], values[base + 2]);
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, scale, interp);
                } else {
                    const Vector3 position = _gltf_to_godot_v3(Vector3(values[base], values[base + 1], values[base + 2]));
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, position, interp);
                }
            }

            if (smooth_this_track) {
                const int key_count = animator->get_keyframe_count(clip_idx, curve_idx);

                if (is_rotation) {
                    // Unwrap Euler components so adjacent keys stay continuous.
                    // glTF quaternions often flip sign between keys (q and -q are
                    // the same rotation), which appears as ~2*pi jumps in Euler
                    // space and would collapse the auto tangents to zero.
                    Vector3 prev = animator->get_keyframe_vector_value(clip_idx, curve_idx, 0);
                    for (int key_idx = 1; key_idx < key_count; key_idx++) {
                        Vector3 e = animator->get_keyframe_vector_value(clip_idx, curve_idx, key_idx);
                        for (int a = 0; a < 3; a++) {
                            while (e[a] - prev[a] >  VFX_PI) e[a] -= VFX_TAU;
                            while (e[a] - prev[a] < -VFX_PI) e[a] += VFX_TAU;
                        }
                        // Keeps quat_value in sync (same rotation, since adding
                        // full turns does not change Basis::from_euler(e)).
                        animator->set_keyframe_vector_value(clip_idx, curve_idx, key_idx, e);
                        prev = e;
                    }
                }

                // Automatic handles use the actual key-time spacing, so
                // unevenly sampled GLB tracks remain well behaved.
                for (int key_idx = 0; key_idx < key_count; key_idx++) {
                    animator->set_keyframe_tangent_mode(clip_idx, curve_idx, key_idx, VFXAnimator::TANGENT_AUTO);
                }
                animator->auto_compute_tangents(clip_idx, curve_idx);

                // TEMP DEBUG: read back the stored values after tangent generation.
                const int final_interp = animator->get_keyframe_interp(clip_idx, curve_idx, 0);
                const int final_tangent_mode = animator->get_keyframe_tangent_mode(clip_idx, curve_idx, 0);
                UtilityFunctions::print(
                    String("[GLB DEBUG] Final track: ") + curve_name +
                    " | first key interpolation: " + String::num_int64(final_interp) +
                    " | first key tangent mode: " + String::num_int64(final_tangent_mode) +
                    " (AUTO=" + String::num_int64(VFXAnimator::TANGENT_AUTO) + ")"
                );

                // TEMP DEBUG: inspect actual Euler values and generated handles
                // for the imported Spine rotation curve (especially its X component).
                if (curve_name.find("Spine_rotation") >= 0 && key_count > 1) {
                    const int debug_indices[] = {0, 1, 2, key_count / 4, key_count / 2,
                                                 (key_count * 3) / 4, key_count - 2, key_count - 1};
                    for (int debug_i = 0; debug_i < 8; ++debug_i) {
                        const int key_idx = debug_indices[debug_i];
                        if (key_idx < 0 || key_idx >= key_count) continue;
                        bool already_printed = false;
                        for (int prior = 0; prior < debug_i; ++prior) {
                            if (debug_indices[prior] == key_idx) already_printed = true;
                        }
                        if (already_printed) continue;

                        const Vector3 value = animator->get_keyframe_vector_value(clip_idx, curve_idx, key_idx);
                        const Vector3 in_tan = animator->get_keyframe_vector_in_tangent(clip_idx, curve_idx, key_idx);
                        const Vector3 out_tan = animator->get_keyframe_vector_out_tangent(clip_idx, curve_idx, key_idx);
                        const Vector2 time_tan = animator->get_keyframe_time_tangents(clip_idx, curve_idx, key_idx);
                        UtilityFunctions::print(
                            String("[GLB TANGENT DEBUG] ") + curve_name +
                            " | key: " + String::num_int64(key_idx) +
                            " | time: " + String::num(animator->get_keyframe_time(clip_idx, curve_idx, key_idx), 6) +
                            " | value XYZ: " + String::num(value.x, 6) + "," + String::num(value.y, 6) + "," + String::num(value.z, 6) +
                            " | in XYZ: " + String::num(in_tan.x, 6) + "," + String::num(in_tan.y, 6) + "," + String::num(in_tan.z, 6) +
                            " | out XYZ: " + String::num(out_tan.x, 6) + "," + String::num(out_tan.y, 6) + "," + String::num(out_tan.z, 6) +
                            " | time handles: " + String::num(time_tan.x, 6) + "," + String::num(time_tan.y, 6)
                        );
                    }
                }
            } else {
                const int final_interp = animator->get_keyframe_interp(clip_idx, curve_idx, 0);
                UtilityFunctions::print(
                    String("[GLB DEBUG] Final track: ") + curve_name +
                    " | first key interpolation: " + String::num_int64(final_interp) +
                    " | auto tangents: not requested"
                );
            }
        }
    }
}