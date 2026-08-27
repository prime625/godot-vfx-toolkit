#ifndef VFX_GLTF_EXPORTER_H
#define VFX_GLTF_EXPORTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include <unordered_map>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"

using namespace godot;

// Forward declarations
class VFXScene;
class VFXSceneNode;

// glTF 2.0 exporter (binary .glb and JSON .gltf)
// Supports: mesh, skin, skeleton, animation, VAT (Vertex Animation Texture)
// NEW: Full scene tree export

class VFXGLTFExporter : public RefCounted {
    GDCLASS(VFXGLTFExporter, RefCounted)

private:
    // glTF JSON structure builders
    Dictionary _build_asset() const;
    Dictionary _build_scene(int node_idx) const;
    Dictionary _build_node_mesh(int mesh_idx, int skin_idx = -1) const;
    Dictionary _build_node_bone(int bone_idx, const std::vector<int>& children) const;
    Dictionary _build_buffer(int byte_length) const;
    Dictionary _build_buffer_view(int buffer, int offset, int length, int target = 34962) const;
    Dictionary _build_accessor(int view, int count, int component_type, String type, bool normalized = false) const;
    Dictionary _build_primitive(int pos_acc, int norm_acc, int uv_acc, int indices_acc, int joints_acc = -1, int weights_acc = -1) const;
    Dictionary _build_mesh(int prim_idx) const;
    Dictionary _build_skin(int inv_bind_acc, const std::vector<int>& joints) const;
    Dictionary _build_animation_sampler(int input_acc, int output_acc, String interpolation = "LINEAR") const;
    Dictionary _build_animation_channel(int sampler, String path, int node) const;
    Dictionary _build_animation(const String& name, const Array& samplers, const Array& channels) const;

    // NEW: Node builder with full transform
    Dictionary _build_node_full(const String& name, const Transform3D& transform, const std::vector<int>& children, int mesh_idx = -1, int skin_idx = -1) const;

    // NEW: Coordinate conversion helpers (Godot -> glTF)
    static Vector3 _godot_to_gltf_position(const Vector3& p);
    static Quaternion _godot_to_gltf_rotation(const Quaternion& q);
    static Transform3D _godot_to_gltf_transform(const Transform3D& t);
    static void _decompose_godot_transform(const Transform3D& t, Vector3& out_pos, Quaternion& out_rot, Vector3& out_scale);

    // Binary buffer management
    PackedByteArray buffer_data;
    int _pad_to_4();
    int _write_float_array(const PackedFloat32Array& data);
    int _write_uint16_array(const PackedInt32Array& data);
    int _write_vec3_array(const PackedVector3Array& data);
    int _write_vec2_array(const PackedVector2Array& data);
    int _write_mat4_array(const PackedFloat32Array& data);

    // Helpers
    PackedFloat32Array _pack_vec3_to_floats(const PackedVector3Array& arr) const;
    PackedFloat32Array _pack_vec2_to_floats(const PackedVector2Array& arr) const;

    // NEW: Scene export state and helpers
    struct _SceneExportState {
        Array nodes;
        Array meshes;
        Array skins;
        Array animations;
        Array bufferViews;
        Array accessors;
        Array buffers;
        std::unordered_map<VFXSkeleton*, int> skeleton_to_skin;
        std::unordered_map<VFXSkeleton*, std::vector<int>> skeleton_to_bone_nodes;
        std::unordered_map<VFXAnimator*, bool> exported_animators;
        std::unordered_map<VFXMesh*, int> mesh_to_index;
    };

    int _export_mesh_to_gltf(const Ref<VFXMesh>& mesh, _SceneExportState& state);
    int _export_skeleton_to_gltf(const Ref<VFXSkeleton>& skeleton, _SceneExportState& state, std::vector<int>& out_bone_node_indices);
    void _export_animations_to_gltf(const Ref<VFXAnimator>& animator, const Ref<VFXSkeleton>& skeleton, _SceneExportState& state, const std::vector<int>& bone_node_indices, const String& node_name_prefix);
    int _export_scene_node_recursive(const Ref<VFXSceneNode>& node, _SceneExportState& state);

protected:
    static void _bind_methods();

public:
    VFXGLTFExporter();
    ~VFXGLTFExporter();

    // === EXPORT ===
    bool export_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const String& filepath);
    bool export_glb_animated(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const Ref<VFXAnimator>& animator, int clip_idx, const String& filepath);
    bool export_vat_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkin>& skin, int frame_count, float fps, const String& filepath);

    // NEW: Export full VFXScene tree as .glb
    bool export_scene_glb(const Ref<VFXScene>& scene, const String& filepath);

    // === JSON BUILDERS ===
    Dictionary build_gltf_document(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton) const;

    // === UTILS ===
    static PackedByteArray json_to_bytes(const Dictionary& doc);
    static PackedByteArray combine_glb(const Dictionary& json, const PackedByteArray& bin);
};

#endif
