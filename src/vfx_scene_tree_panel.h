#ifndef VFX_SCENE_TREE_PANEL_H
#define VFX_SCENE_TREE_PANEL_H

#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <vector>
#include <set>

#include "vfx_scene.h"

using namespace godot;

// A fully custom-drawn hierarchy view. It deliberately does not use Godot's
// Tree control, so mouse and touch share the same hit-testing and drag logic.
class VFXSceneTreePanel : public VBoxContainer {
    GDCLASS(VFXSceneTreePanel, VBoxContainer)

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
    Button *multi_btn = nullptr;
    Button *reparent_btn = nullptr;

    Ref<VFXScene> scene;
    std::vector<Row> rows;
    std::set<int> selected_ids;
    int active_id = -1;

    bool syncing = false;
    bool dirty = true;
    bool multi_select_enabled = false;
    bool reparent_mode = false;
    bool pointer_down = false;
    bool pressed_was_selected = false;
    bool dragging = false;
    bool long_press_rename = false;
    bool touch_drag_armed = false;
    bool touch_scrolling = false;
    int pressed_touch_index = -1;
    int pressed_node_id = -1;
    int drop_target_id = -1;
    Vector2 press_position;
    Vector2 pointer_position;
    float press_elapsed = 0.0f;
    float scroll_offset = 0.0f;
    float content_height = 0.0f;
    float row_height = 30.0f;
    float indent_width = 17.0f;
    float tree_top = 42.0f;
    float drag_threshold = 9.0f;

    LineEdit *rename_edit = nullptr;
    int renaming_node_id = -1;
    String rename_original;

    Ref<Font> ui_font;

    void _rebuild_rows();
    void _append_rows(const Ref<VFXSceneNode> &node, int depth, const String &filter);
    void _draw() override;
    void _gui_input(const Ref<InputEvent> &event) override;
    void _process(double delta) override;

    int _row_at_y(float y) const;
    int _row_index_for_id(int id) const;
    Ref<VFXSceneNode> _find_node(int id) const;
    Ref<VFXSceneNode> _active_node() const;
    void _select_node(int id, bool toggle);
    void _emit_selection();
    void _clear_selection();
    void _toggle_expanded(const Ref<VFXSceneNode> &node);
    void _toggle_visibility(const Ref<VFXSceneNode> &node);
    void _begin_rename(int id);
    void _commit_rename();
    void _cancel_rename();
    void _on_search_changed(const String &text);
    void _on_add_pressed();
    void _on_multi_pressed();
    void _on_reparent_pressed();
    void _delete_selected_nodes();
    bool _can_reparent(const Ref<VFXSceneNode> &node, const Ref<VFXSceneNode> &new_parent) const;
    void _perform_drop(int target_id);
    void _reparent_preserving_world(const Ref<VFXSceneNode> &node, const Ref<VFXSceneNode> &new_parent);
    void _finish_pointer(bool released);
    void _set_drop_target_from_position(const Vector2 &position);

protected:
    static void _bind_methods();
    void _notification(int what);

public:
    VFXSceneTreePanel();
    ~VFXSceneTreePanel();

    void set_scene(const Ref<VFXScene> &p_scene);
    Ref<VFXScene> get_scene() const;
    void mark_dirty();
    void force_rebuild();
    void delete_selected_nodes();
};

#endif
