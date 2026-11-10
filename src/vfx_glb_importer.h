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
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
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
    static inline const char* KEY_MATERIALS = "materials";
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
    struct GLBImage {
        String name;
        String mime_type;
        String uri;
        int buffer_view = -1;
    };
    struct GLBSampler {
        int mag_filter = -1, min_filter = -1;
        int wrap_s = 10497, wrap_t = 10497; // glTF default: REPEAT
    };
    struct GLBTexture {
        int sampler = -1, source = -1;
    };
    struct GLBTextureInfo {
        int index = -1;
        int tex_coord = 0;
    };
    struct GLBPBRMetallicRoughness {
        Color base_color_factor = Color(1, 1, 1, 1);
        GLBTextureInfo base_color_texture;
        float metallic_factor = 1.0f;
        float roughness_factor = 1.0f;
        GLBTextureInfo metallic_roughness_texture;
    };
    struct GLBMaterial {
        String name;
        GLBPBRMetallicRoughness pbr;
        GLBTextureInfo normal_texture;
        GLBTextureInfo emissive_texture;
        GLBTextureInfo occlusion_texture;
        Color emissive_factor = Color(0, 0, 0, 1);
        String alpha_mode = "OPAQUE";
        float alpha_cutoff = 0.5f;
        bool double_sided = false;
        bool unlit = false;
    };

    String source_path;
    std::vector<GLBBuffer> buffers;
    std::vector<GLBBufferView> buffer_views;
    std::vector<GLBAccessor> accessors;
    std::vector<GLBNode> nodes;
    std::vector<GLBMesh> meshes;
    std::vector<GLBSkin> skins;
    std::vector<GLBAnimation> animations;
    std::vector<GLBImage> images;
    std::vector<GLBSampler> samplers;
    std::vector<GLBTexture> textures;
    std::vector<GLBMaterial> materials;
    std::vector<Ref<StandardMaterial3D>> godot_materials;
    std::vector<Ref<ImageTexture>> decoded_textures;
    std::vector<int> scene_nodes;
    Transform3D _get_node_world_transform(int node_idx, const std::vector<Ref<VFXSceneNode>>& vfx_nodes) const;

    bool _parse_glb(const PackedByteArray& data, String& out_error);
    bool _parse_json(const String& json_text, const PackedByteArray& bin_chunk, String& out_error);
    bool _parse_nodes(const Array& nodes_arr);
    bool _parse_meshes(const Array& meshes_arr);
    bool _parse_skins(const Array& skins_arr);
    bool _parse_animations(const Array& anims_arr);
    bool _parse_accessors(const Array& acc_arr);
    bool _parse_buffer_views(const Array& bv_arr);
    bool _parse_buffers(const Array& buf_arr, const PackedByteArray& bin_chunk);
    bool _parse_images(const Array& img_arr);
    bool _parse_samplers(const Array& samp_arr);
    bool _parse_textures(const Array& tex_arr);
    bool _parse_materials(const Array& mat_arr);
    static GLBTextureInfo _parse_texture_info(const Dictionary& d);
    void _build_materials();
    Ref<Image> _decode_image(int image_idx);
    Ref<ImageTexture> _get_texture(int texture_idx);

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

    Ref<VFXMesh> _build_mesh(const GLBMesh& glb_mesh, String& out_error, const Transform3D& vertex_space = Transform3D());
    Ref<VFXSkeleton> _build_skeleton(const GLBSkin& glb_skin, int owner_node_idx, String& out_error);
    bool _skin_uses_scene_space_ibms(const GLBSkin& glb_skin, int owner_node_idx);
    void _build_animations(Ref<VFXAnimator> animator, const std::vector<int>& node_to_bone, bool smooth_linear_curves);
    void _clear_document();

protected:
    static void _bind_methods();

public:
    VFXGLBImporter();
    ~VFXGLBImporter();
    Dictionary import_glb(const String& path, bool smooth_linear_curves = false);
    static Vector3 convert_position(const Vector3& gltf_pos);
    static Vector3 convert_normal(const Vector3& gltf_normal);
    static Quaternion convert_rotation(const Quaternion& gltf_rot);
    static Transform3D convert_transform(const Transform3D& gltf_transform);
};

#endif