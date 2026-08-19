#ifndef VFX_SCENE_NODE_H
#define VFX_SCENE_NODE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/array.hpp>
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
        NODE_CAMERA = 2,
        NODE_LIGHT = 3,
        NODE_ARMATURE = 4,
        NODE_CURVE = 5
    };

private:
    String node_name = "Node";
    String icon_id = "empty";
    Color icon_color = Color(1, 1, 1, 1);
    NodeType type = NODE_EMPTY;
    Transform3D local_transform;
    bool visible = true;
    bool selected = false;
    bool locked = false;

    // Typed payload
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;
    Ref<VFXSkin> skin;
    Ref<VFXAnimator> animator;

    // Hierarchy (raw pointer to parent avoids circular RefCounted refs)
    VFXSceneNode* parent = nullptr;
    std::vector<Ref<VFXSceneNode>> children;

protected:
    static void _bind_methods();

public:
    VFXSceneNode();
    ~VFXSceneNode();

    // Identity
    void set_node_name(const String& p_name);
    String get_node_name() const;

    void set_icon_id(const String& p_id);
    String get_icon_id() const;

    void set_icon_color(const Color& p_color);
    Color get_icon_color() const;

    void set_node_type(int p_type);
    int get_node_type() const;

    // Transform
    void set_transform(const Transform3D& t);
    Transform3D get_transform() const;
    Transform3D get_global_transform() const;

    // State
    void set_visible(bool v);
    bool is_visible() const;
    void set_selected(bool s);
    bool is_selected() const;
    void set_locked(bool l);
    bool is_locked() const;

    // Payload
    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;
    void set_skeleton(const Ref<VFXSkeleton>& p_sk);
    Ref<VFXSkeleton> get_skeleton() const;
    void set_skin(const Ref<VFXSkin>& p_skin);
    Ref<VFXSkin> get_skin() const;
    void set_animator(const Ref<VFXAnimator>& p_anim);
    Ref<VFXAnimator> get_animator() const;

    // Hierarchy
    void add_child(const Ref<VFXSceneNode>& child);
    void remove_child(const Ref<VFXSceneNode>& child);
    void remove_child_by_index(int idx);
    int get_child_count() const;
    Ref<VFXSceneNode> get_child(int idx) const;
    VFXSceneNode* get_parent_node() const; // named to avoid clash with Object::get_parent()

    void clear_children();
    bool is_ancestor_of(const VFXSceneNode* node) const;
    Ref<VFXSceneNode> find_child_by_name(const String& name) const;
    // BEFORE: void get_all_descendants(Array& out) const;
    Array get_all_descendants() const;
    
    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);
};

VARIANT_ENUM_CAST(VFXSceneNode::NodeType);

#endif
