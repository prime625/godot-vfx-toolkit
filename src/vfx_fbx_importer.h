#ifndef VFX_FBX_IMPORTER_H
#define VFX_FBX_IMPORTER_H

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
#include <memory>

#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_scene.h"
#include "vfx_scene_node.h"

using namespace godot;

// ============================================================================
// FBX Binary Document Parser
// ============================================================================

namespace vfx_fbx {

// Forward declarations
struct FBXRecord;
struct FBXProperty;

// Property types in FBX binary format
enum class FBXPropType {
    UNKNOWN,
    INT16,      // 'Y'
    BOOL,       // 'C'
    INT32,      // 'I'
    FLOAT,      // 'F'
    DOUBLE,     // 'D'
    INT64,      // 'L'
    ARRAY_FLOAT, // 'f'
    ARRAY_DOUBLE, // 'd'
    ARRAY_INT64,  // 'l'
    ARRAY_INT32,  // 'i'
    ARRAY_BOOL,   // 'b'
    STRING,     // 'S'
    RAW,        // 'R'
};

struct FBXProperty {
    FBXPropType type = FBXPropType::UNKNOWN;
    
    // Scalar storage
    int64_t int_val = 0;
    double double_val = 0.0;
    bool bool_val = false;
    
    // Array / string / raw storage
    std::vector<uint8_t> data;
    
    // Array metadata
    uint32_t array_count = 0;
    uint32_t array_encoding = 0; // 0 = raw, 1 = deflate
    uint32_t array_compressed_len = 0;
    
    bool is_array() const {
        return type == FBXPropType::ARRAY_FLOAT ||
               type == FBXPropType::ARRAY_DOUBLE ||
               type == FBXPropType::ARRAY_INT64 ||
               type == FBXPropType::ARRAY_INT32 ||
               type == FBXPropType::ARRAY_BOOL;
    }
    
    String as_string() const;
    int64_t as_int() const;
    double as_double() const;
    float as_float() const;
    bool as_bool() const;
    
    PackedFloat32Array as_float_array() const;
    PackedInt32Array as_int_array() const;
    PackedVector3Array as_vec3_array() const;
    PackedVector2Array as_vec2_array() const;
};

struct FBXRecord {
    String name;
    std::vector<FBXProperty> properties;
    std::vector<std::unique_ptr<FBXRecord>> children;
    
    const FBXRecord* find_child(const String& child_name) const;
    const FBXProperty* find_property(int index) const;
    
    // Find first child with given name
    const FBXRecord* child(const String& n) const {
        return find_child(n);
    }
};

struct FBXDocument {
    uint32_t version = 0;
    std::unique_ptr<FBXRecord> root;
    
    // Parsed objects
    std::unordered_map<int64_t, const FBXRecord*> objects_by_id;
    std::unordered_map<int64_t, std::vector<int64_t>> connections; // parent -> children
    std::unordered_map<int64_t, int64_t> parent_of; // child -> parent
    
    // Object type lookups
    std::vector<int64_t> geometry_ids;
    std::vector<int64_t> model_ids;
    std::vector<int64_t> limb_node_ids;
    std::vector<int64_t> mesh_model_ids;
    std::vector<int64_t> skin_ids;
    std::vector<int64_t> cluster_ids;
    std::vector<int64_t> anim_stack_ids;
    std::vector<int64_t> anim_layer_ids;
    std::vector<int64_t> anim_curve_node_ids;
    std::vector<int64_t> anim_curve_ids;
    std::vector<int64_t> pose_ids;
    
    float unit_scale = 1.0f; // cm to meters, typically 0.01
    
    bool parse(const PackedByteArray& data, String& out_error);
    void build_connection_graph();
    void classify_objects();
    
private:
    bool _parse_header(const uint8_t* data, int size, String& out_error);
    bool _parse_record(const uint8_t* data, int size, int& offset, FBXRecord* parent, 
                       uint32_t record_version, String& out_error);
    bool _parse_property(const uint8_t* data, int size, int& offset, FBXProperty& prop, 
                         uint32_t record_version, String& out_error);
    bool _decompress_deflate(const uint8_t* src, int src_len, uint8_t* dst, int dst_len, String& out_error);
};

} // namespace vfx_fbx

// ============================================================================
// VFX FBX Importer
// ============================================================================

class VFXFBXImporter : public RefCounted {
    GDCLASS(VFXFBXImporter, RefCounted)

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
    std::unique_ptr<vfx_fbx::FBXDocument> doc;
    
    // Per-object cached data
    std::unordered_map<int64_t, Ref<VFXMesh>> built_meshes;
    std::unordered_map<int64_t, Ref<VFXSkeleton>> built_skeletons;
    std::unordered_map<int64_t, Ref<VFXAnimator>> built_animators;
    std::unordered_map<int64_t, Ref<VFXSkin>> built_skins;
    std::unordered_map<int64_t, Ref<VFXSceneNode>> built_nodes;
    
    // Bone mapping: FBX cluster/node ID -> VFXSkeleton bone index
    std::unordered_map<int64_t, int> cluster_to_bone;
    std::unordered_map<int64_t, int> model_to_bone;
    
    // Coordinate conversion
    static Vector3 _fbx_to_godot_pos(const Vector3& p);
    static Vector3 _fbx_to_godot_n(const Vector3& n);
    static Quaternion _fbx_to_godot_q(const Quaternion& q);
    static Transform3D _fbx_to_godot_transform(const Transform3D& t);
    
    // Extraction
    Ref<VFXMesh> _build_mesh(int64_t geom_id, String& out_error);
    Ref<VFXSkeleton> _build_skeleton(int64_t skin_id, String& out_error);
    void _build_animations(Ref<VFXAnimator> animator, const Ref<VFXSkeleton>& skeleton, String& out_error);
    
    // Helpers
    Transform3D _get_model_transform(int64_t model_id) const;
    String _get_object_name(int64_t obj_id) const;
    String _get_object_type(int64_t obj_id) const;
    const vfx_fbx::FBXRecord* _get_object_record(int64_t obj_id) const;
    std::vector<int64_t> _get_children_of_type(int64_t parent_id, const String& type) const;
    int64_t _get_first_child_of_type(int64_t parent_id, const String& type) const;
    
    void _clear_state();

protected:
    static void _bind_methods();

public:
    VFXFBXImporter();
    ~VFXFBXImporter();
    
    Dictionary import_fbx(const String& path);
    
    // Static converters (mirrors GLB importer API)
    static Vector3 convert_position(const Vector3& fbx_pos);
    static Vector3 convert_normal(const Vector3& fbx_normal);
    static Quaternion convert_rotation(const Quaternion& fbx_rot);
    static Transform3D convert_transform(const Transform3D& fbx_transform);
};

#endif