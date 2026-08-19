#ifndef VFX_SCENE_H
#define VFX_SCENE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include "vfx_scene_node.h"

using namespace godot;

class VFXScene : public RefCounted {
    GDCLASS(VFXScene, RefCounted)

private:
    Ref<VFXSceneNode> root;
    int node_counter = 0;

    void _import_godot_node(Node* godot_node, VFXSceneNode* parent);
    void _flatten_recursive(const Ref<VFXSceneNode>& node, Array& out) const;

protected:
    static void _bind_methods();

public:
    VFXScene();
    ~VFXScene();

    void create_default_root();
    Ref<VFXSceneNode> get_root() const;

    Ref<VFXSceneNode> create_node(const String& name, int type, const Ref<VFXSceneNode>& parent);
    void delete_node(const Ref<VFXSceneNode>& node);
    void clear();

    bool import_model(const String& filepath, const Ref<VFXSceneNode>& parent = Ref<VFXSceneNode>());

    Ref<VFXSceneNode> find_node_by_name(const String& name) const;
    Array get_all_mesh_nodes() const;
    Array flatten_tree() const;

    int get_unique_id();

    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);
};

#endif
