#ifndef VFX_SCENE_TREE_PANEL_H
#define VFX_SCENE_TREE_PANEL_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <map>
#include <set>
#include <vector>

#include "vfx_scene.h"

using namespace godot;

// A fully custom-drawn scene dock. No Godot Tree/TreeItem is used.
class VFXSceneTreePanel : public Control {
    GDCLASS(VFXSceneTreePanel, Control)

    struct Row {
        Ref<VFXSceneNode> node;
        int depth = 0;
        float y = 0.0f;
        bool has_children = false;
    };

private:
    HBoxContainer *toolbar = nullptr;
    LineEdit *search_edit = nullptr;
    Button *add_btn = nullptr;
    LineEdit *rename_edit = nullptr;
    PopupMenu *context_menu = nullptr;

    Ref<VFXScene> scene;
    Ref<Texture2D> icon_empty, icon_mesh, icon_armature, icon_bone;
    Ref<Texture2D> icon_light, icon_camera, icon_curve, icon_visible, icon_hidden;

    std::vector<Row> rows;
    std::set<int> selected_ids;
    std::map<int, float> row_y_by_id;
    int active_id = -1;
    int anchor_id = -1;
    int context_id = -1;
    int rename_id = -1;

    float row_height = 27.0f;
    float indent_width = 18.0f;
    float scroll_offset = 0.0f;
    float content_height = 0.0f;
    bool syncing = false;

    bool pointer_down = false;
    bool dragging = false;
    bool touch_scrolling = false;
    Vector2 press_position;
    Vector2 last_pointer_position;
    int pressed_id = -1;
    int drop_target_id = -1;
    int hover_id = -1;
    uint64_t press_time_msec = 0;
    uint64_t last_tap_time_msec = 0;
    int last_tap_id = -1;
    bool long_press_fired = false;

    void _load_icons();
    Ref<Texture2D> _get_icon_for_type(int type) const;
    void _rebuild_rows();
    void _append_rows(const Ref<VFXSceneNode> &node, int depth, const String &filter);
    bool _subtree_matches(const Ref<VFXSceneNode> &node, const String &filter) const;
    Ref<VFXSceneNode> _find_node(int id) const;
    Ref<VFXSceneNode> _row_node_at(float y) const;
    int _row_id_at(float y) const;
    float _tree_top() const;
    void _ensure_active_visible();
    void _sync_selection_to_scene();
    void _select_node(int id, bool additive, bool range);
    void _clear_selection();
    void _draw_row(const Row &row);
    void _draw_tree();
    void _begin_rename(int id);
    void _position_rename_editor();
    void _finish_rename();
    void _show_context_menu(int id, const Vector2 &screen_position, bool preserve_selection = false);
    void _delete_selected();
    void _reparent_node(int node_id, int parent_id);
    void _on_search_changed(const String &text);
    void _on_add_pressed();
    void _on_rename_finished(const String &text);
    void _on_rename_focus_exited();
    void _on_context_id_pressed(int id);

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    // godot-cpp registers these virtuals through GDCLASS, so they must be public.
    void _draw() override;
    void _gui_input(const Ref<InputEvent> &event) override;

    VFXSceneTreePanel();
    ~VFXSceneTreePanel();

    void set_scene(const Ref<VFXScene> &p_scene);
    Ref<VFXScene> get_scene() const;
    void mark_dirty();
    void force_rebuild();
    void delete_selected_nodes();
};

#endif
