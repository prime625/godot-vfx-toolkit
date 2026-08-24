#ifndef VFX_SCENE_H
#define VFX_SCENE_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include "vfx_scene_node.h"

using namespace godot;

class VFXScene : public RefCounted {
    GDCLASS(VFXScene, RefCounted)

private:
    Ref<VFXSceneNode> root;
    int node_counter = 0;

    void _import_godot_node(Node* godot_node, VFXSceneNode* parent);
    Ref<VFXSceneNode> _clone_node_recursive(const Ref<VFXSceneNode>& source) const;
    void _reassign_ids_recursive(const Ref<VFXSceneNode>& node);
    bool _name_exists_recursive(const Ref<VFXSceneNode>& node, const String& name) const;

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

    // Legacy Godot ResourceLoader import (kept for compat)
    bool import_model(const String& filepath, const Ref<VFXSceneNode>& parent = Ref<VFXSceneNode>());

    // NEW: Custom GLB import using VFXGLBImporter -- merges into this scene
    bool import_glb_model(const String& filepath,
                          const Ref<VFXSceneNode>& parent = Ref<VFXSceneNode>(),
                          const Transform3D& transform = Transform3D());

    // NEW: Merge another VFXScene into this one (Blender-like Append)
    bool merge_scene(const Ref<VFXScene>& other,
                     const Ref<VFXSceneNode>& parent = Ref<VFXSceneNode>(),
                     const Transform3D& transform = Transform3D());

    // NEW: Ensure a node name is unique in the entire scene (appends .001, .002, etc.)
    void ensure_unique_name(String& name) const;

    // NEW: Total node count in the scene
    int get_node_count() const;

    Ref<VFXSceneNode> find_node_by_name(const String& name) const;
    Array get_all_mesh_nodes() const;
    Array flatten_tree() const;

    int get_unique_id();

    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);
};

#endif
