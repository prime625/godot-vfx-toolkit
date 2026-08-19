#ifndef VFX_SCENE_NODE_H
#define VFX_SCENE_NODE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>

#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"

using namespace godot;

class VFXSceneNode : public RefCounted {
    GDCLASS(VFXSceneNode, RefCounted)

public:
    enum NodeType {
        NODE_EMPTY = 0,
        NODE_MESH = 1,
        NODE_ARMATURE = 2,
        NODE_BONE = 3,
        NODE_LIGHT = 4,
        NODE_CAMERA = 5,
        NODE_CURVE = 6
    };

private:
    int node_id = -1;
    String node_name = "Node";
    NodeType node_type = NODE_EMPTY;

    VFXSceneNode* parent_node = nullptr;
    std::vector<Ref<VFXSceneNode>> children;

    Transform3D local_transform;
    bool node_visible = true;
    bool node_expanded = true;
    bool node_selected = false;

    // Components
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;
    Ref<VFXSkin> skin;
    Ref<VFXAnimator> animator;

    int bone_parent_idx = -1;

    void _unparent();
    Ref<VFXSceneNode> _find_node_by_id_recursive(int p_id) const;
    void _find_node_by_name_recursive(const String& p_name, Ref<VFXSceneNode>& r_result) const;
    void _get_all_descendants_recursive(Array& r_out) const;

protected:
    static void _bind_methods();

public:
    VFXSceneNode();
    ~VFXSceneNode();

    // Identity
    void set_node_id(int p_id);
    int get_node_id() const;
    void set_node_name(const String& p_name);
    String get_node_name() const;
    void set_node_type(int p_type);
    int get_node_type() const;

    // Hierarchy
    void set_parent_node(VFXSceneNode* p_parent);
    VFXSceneNode* get_parent_node() const;
    void add_child(const Ref<VFXSceneNode>& p_child);
    void remove_child(const Ref<VFXSceneNode>& p_child);
    void remove_child_by_id(int p_child_id);
    void clear_children();
    int get_child_count() const;
    Ref<VFXSceneNode> get_child(int p_index) const;
    Array get_children() const;
    bool has_child(const Ref<VFXSceneNode>& p_child) const;
    bool is_ancestor_of(const Ref<VFXSceneNode>& p_node) const;
    bool is_descendant_of(const Ref<VFXSceneNode>& p_node) const;

    // Traversal
    Ref<VFXSceneNode> find_node_by_id(int p_id) const;
    Ref<VFXSceneNode> find_node_by_name(const String& p_name) const;
    Ref<VFXSceneNode> find_child_by_name(const String& p_name) const;
    Array get_all_descendants() const;

    // Transform
    void set_local_transform(const Transform3D& p_transform);
    Transform3D get_local_transform() const;
    void set_transform(const Transform3D& p_transform);
    Transform3D get_transform() const;
    void set_local_position(const Vector3& p_pos);
    Vector3 get_local_position() const;
    void set_local_rotation(const Quaternion& p_rot);
    Quaternion get_local_rotation() const;
    void set_local_scale(const Vector3& p_scale);
    Vector3 get_local_scale() const;

    // State
    void set_visible(bool p_visible);
    bool get_visible() const;
    void set_expanded(bool p_expanded);
    bool get_expanded() const;
    void set_selected(bool p_selected);
    bool get_selected() const;

    // Components
    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;
    void set_skeleton(const Ref<VFXSkeleton>& p_skeleton);
    Ref<VFXSkeleton> get_skeleton() const;
    void set_skin(const Ref<VFXSkin>& p_skin);
    Ref<VFXSkin> get_skin() const;
    void set_animator(const Ref<VFXAnimator>& p_animator);
    Ref<VFXAnimator> get_animator() const;

    // Bone
    void set_bone_parent_index(int p_idx);
    int get_bone_parent_index() const;

    // Utility
    bool has_mesh() const;
    bool has_skeleton() const;
    bool has_skin() const;
    bool has_animator() const;
    Color get_type_color() const;
    String get_type_icon_hint() const;

    // Serialization
    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& p_data);
};

VARIANT_ENUM_CAST(VFXSceneNode::NodeType);

#endif
