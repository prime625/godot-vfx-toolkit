#ifndef VFX_SCENE_TREE_PANEL_H
#define VFX_SCENE_TREE_PANEL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <map>

#include "vfx_scene.h"

using namespace godot;

class VFXSceneTreePanel : public VBoxContainer {
    GDCLASS(VFXSceneTreePanel, VBoxContainer)

private:
    Tree* tree = nullptr;
    LineEdit* search_edit = nullptr;
    Button* add_btn = nullptr;

    Ref<VFXScene> scene;

    // Map: VFXSceneNode id -> TreeItem*
    std::map<int, TreeItem*> node_to_item;
    // Map: TreeItem* -> VFXSceneNode id
    std::map<TreeItem*, int> item_to_node;

    bool syncing = false; // prevent signal loops
    bool tree_dirty = true;

    // Icon references (set these in _ready or load them)
    Ref<Texture2D> icon_empty;
    Ref<Texture2D> icon_mesh;
    Ref<Texture2D> icon_armature;
    Ref<Texture2D> icon_bone;
    Ref<Texture2D> icon_light;
    Ref<Texture2D> icon_camera;
    Ref<Texture2D> icon_curve;
    Ref<Texture2D> icon_visible;
    Ref<Texture2D> icon_hidden;

    void _build_tree();
    void _build_tree_recursive(TreeItem* parent_item, const Ref<VFXSceneNode>& node);
    void _clear_tree_maps();

    Ref<Texture2D> _get_icon_for_type(int type) const;

    void _on_tree_item_selected();
    void _on_tree_item_edited();
    void _on_tree_button_clicked(TreeItem* item, int column, int id, int mouse_button_idx);
    void _on_tree_item_collapsed(TreeItem* item);
    void _on_tree_item_activated();
    void _on_tree_nothing_selected();
    void _on_search_text_changed(const String& text);
    void _on_add_pressed();
    void _on_tree_item_mouse_selected(const Vector2& mouse_pos, int mouse_button_idx);

    void _sync_selection_to_scene();
    void _sync_expansion_to_scene();
    void _reparent_item(TreeItem* item, TreeItem* new_parent);

    Ref<VFXSceneNode> _get_node_for_item(TreeItem* item);  // was: ... const;
    TreeItem* _get_item_for_node(int node_id) const;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    VFXSceneTreePanel();
    ~VFXSceneTreePanel();

    void set_scene(const Ref<VFXScene>& p_scene);
    Ref<VFXScene> get_scene() const;

    void mark_dirty();
    void force_rebuild();
};

#endif
