#ifndef VFX_GLTF_EXPORTER_H
#define VFX_GLTF_EXPORTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"

using namespace godot;

// glTF 2.0 exporter (binary .glb and JSON .gltf)
// Supports: mesh, skin, skeleton, animation, VAT (Vertex Animation Texture)

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

    // Binary buffer management
    PackedByteArray buffer_data;
    int _pad_to_4();
    int _write_float_array(const PackedFloat32Array& data);
    int _write_uint16_array(const PackedInt32Array& data);
    int _write_vec3_array(const PackedVector3Array& data);
    int _write_vec2_array(const PackedVector2Array& data);
    int _write_mat4_array(const PackedFloat32Array& data);  // 4x4 matrices

    // Helpers
    PackedFloat32Array _pack_vec3_to_floats(const PackedVector3Array& arr) const;
    PackedFloat32Array _pack_vec2_to_floats(const PackedVector2Array& arr) const;

protected:
    static void _bind_methods();

public:
    VFXGLTFExporter();
    ~VFXGLTFExporter();

    // === EXPORT ===
    // Export mesh + skin + skeleton as .glb
    bool export_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const String& filepath);

    // Export mesh + skin + skeleton + animation as .glb
    bool export_glb_animated(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton, const Ref<VFXAnimator>& animator, int clip_idx, const String& filepath);

    // Export VAT data as separate textures + glb mesh
    // VAT format: position texture (RGB32F) + normal texture (RGB32F)
    bool export_vat_glb(const Ref<VFXMesh>& mesh, const Ref<VFXSkin>& skin, int frame_count, float fps, const String& filepath);

    // === JSON BUILDERS ===
    Dictionary build_gltf_document(const Ref<VFXMesh>& mesh, const Ref<VFXSkeleton>& skeleton) const;

    // === UTILS ===
    static PackedByteArray json_to_bytes(const Dictionary& doc);
    static PackedByteArray combine_glb(const Dictionary& json, const PackedByteArray& bin);
};

#endif
