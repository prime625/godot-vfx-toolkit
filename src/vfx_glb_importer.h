#ifndef VFX_GLB_IMPORTER_H
#define VFX_GLB_IMPORTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <vector>
#include <unordered_map>

#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_scene.h"
#include "vfx_scene_node.h"

using namespace godot;

class VFXGLBImporter : public RefCounted {
    GDCLASS(VFXGLBImporter, RefCounted)

public:
    static inline const char* KEY_SUCCESS = "success";
    static inline const char* KEY_ERROR = "error";
    static inline const char* KEY_SCENE = "scene";
    static inline const char* KEY_MESH_NODES = "mesh_nodes";
    static inline const char* KEY_MESH = "mesh";
    static inline const char* KEY_SKELETON = "skeleton";
    static inline const char* KEY_SKIN = "skin";
    static inline const char* KEY_ANIMATOR = "animator";
    static inline const char* KEY_MATERIAL_COUNT = "material_count";
    static inline const char* KEY_ANIMATION_COUNT = "animation_count";
    static inline const char* KEY_NODE_COUNT = "node_count";
    static inline const char* KEY_MESH_COUNT = "mesh_count";

private:
    struct GLBBuffer { PackedByteArray data; };
    struct GLBBufferView {
        int buffer = 0, byte_offset = 0, byte_length = 0, byte_stride = 0, target = 0;
    };
    struct GLBAccessor {
        int buffer_view = -1, byte_offset = 0, component_type = 0, count = 0;
        String type;
        PackedFloat32Array min, max;
        bool normalized = false;
    };
    struct GLBNode {
        String name;
        int mesh = -1, skin = -1, parent = -1;
        Vector<int> children;
        Vector3 translation, scale = Vector3(1,1,1);
        Quaternion rotation;
        Transform3D matrix;
        bool has_matrix = false;
    };
    struct GLBPrimitive {
        Dictionary attributes;
        int indices = -1, mode = 4, material = -1;
    };
    struct GLBMesh {
        String name;
        std::vector<GLBPrimitive> primitives;
    };
    struct GLBSkin {
        int inverse_bind_matrices = -1, skeleton_root = -1;
        Vector<int> joints;
    };
    struct GLBAnimationSampler {
        int input = -1, output = -1;
        String interpolation = "LINEAR";
    };
    struct GLBAnimationChannel {
        int sampler = -1, target_node = -1;
        String target_path;
    };
    struct GLBAnimation {
        String name;
        std::vector<GLBAnimationSampler> samplers;
        std::vector<GLBAnimationChannel> channels;
    };

    std::vector<GLBBuffer> buffers;
    std::vector<GLBBufferView> buffer_views;
    std::vector<GLBAccessor> accessors;
    std::vector<GLBNode> nodes;
    std::vector<GLBMesh> meshes;
    std::vector<GLBSkin> skins;
    std::vector<GLBAnimation> animations;
    std::vector<int> scene_nodes;

    bool _parse_glb(const PackedByteArray& data, String& out_error);
    bool _parse_json(const String& json_text, const PackedByteArray& bin_chunk, String& out_error);
    bool _parse_nodes(const Array& nodes_arr);
    bool _parse_meshes(const Array& meshes_arr);
    bool _parse_skins(const Array& skins_arr);
    bool _parse_animations(const Array& anims_arr);
    bool _parse_accessors(const Array& acc_arr);
    bool _parse_buffer_views(const Array& bv_arr);
    bool _parse_buffers(const Array& buf_arr, const PackedByteArray& bin_chunk);

    int _accessor_component_count(const String& type) const;
    int _accessor_component_size(int component_type) const;
    PackedByteArray _read_accessor_raw(int accessor_idx, String& out_error);
    bool _read_accessor_floats(int accessor_idx, PackedFloat32Array& out, String& out_error);
    bool _read_accessor_vec3(int accessor_idx, PackedVector3Array& out, String& out_error);
    bool _read_accessor_vec2(int accessor_idx, PackedVector2Array& out, String& out_error);
    bool _read_accessor_indices(int accessor_idx, PackedInt32Array& out, String& out_error);
    bool _read_accessor_vec4i(int accessor_idx, std::vector<Vector4i>& out, String& out_error);
    bool _read_accessor_vec4f(int accessor_idx, std::vector<Vector4>& out, String& out_error);
    bool _read_accessor_mat4(int accessor_idx, std::vector<Transform3D>& out, String& out_error);

    static Vector3 _gltf_to_godot_v3(const Vector3& v);
    static Vector3 _gltf_to_godot_n(const Vector3& n);
    static Quaternion _gltf_to_godot_q(const Quaternion& q);
    static Transform3D _gltf_to_godot_t(const Transform3D& t);
    static Basis _gltf_to_godot_b(const Basis& b);
    Transform3D _get_node_local_transform(const GLBNode& node) const;

    Ref<VFXMesh> _build_mesh(const GLBMesh& glb_mesh, String& out_error);
    Ref<VFXSkeleton> _build_skeleton(const GLBSkin& glb_skin, String& out_error);
    void _build_animations(Ref<VFXAnimator> animator, const std::vector<int>& node_to_bone);
    void _clear_document();

protected:
    static void _bind_methods();

public:
    VFXGLBImporter();
    ~VFXGLBImporter();
    Dictionary import_glb(const String& path);
    static Vector3 convert_position(const Vector3& gltf_pos);
    static Vector3 convert_normal(const Vector3& gltf_normal);
    static Quaternion convert_rotation(const Quaternion& gltf_rot);
    static Transform3D convert_transform(const Transform3D& gltf_transform);
};

#endif
