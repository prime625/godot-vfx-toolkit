#include "vfx_scene_tree_panel.h"

#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <algorithm>

using namespace godot;

namespace {
constexpr float TOOLBAR_HEIGHT = 38.0f;
constexpr float ROW_HEIGHT = 30.0f;
constexpr float VISIBILITY_WIDTH = 34.0f;
}

void VFXSceneTreePanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VFXSceneTreePanel::set_scene);
    ClassDB::bind_method(D_METHOD("get_scene"), &VFXSceneTreePanel::get_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, "VFXScene"), "set_scene", "get_scene");
    ClassDB::bind_method(D_METHOD("mark_dirty"), &VFXSceneTreePanel::mark_dirty);
    ClassDB::bind_method(D_METHOD("force_rebuild"), &VFXSceneTreePanel::force_rebuild);
    ClassDB::bind_method(D_METHOD("delete_selected_nodes"), &VFXSceneTreePanel::delete_selected_nodes);

    ADD_SIGNAL(MethodInfo("node_selected", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("nodes_selected", PropertyInfo(Variant::ARRAY, "nodes")));
    ADD_SIGNAL(MethodInfo("node_activated", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("nodes_reparented"));
    ADD_SIGNAL(MethodInfo("node_renamed", PropertyInfo(Variant::OBJECT, "node"), PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
    ADD_SIGNAL(MethodInfo("node_deleted", PropertyInfo(Variant::OBJECT, "node"), PropertyInfo(Variant::OBJECT, "old_parent")));
    ADD_SIGNAL(MethodInfo("node_reparented", PropertyInfo(Variant::OBJECT, "node"), PropertyInfo(Variant::OBJECT, "old_parent"), PropertyInfo(Variant::OBJECT, "new_parent")));
    ADD_SIGNAL(MethodInfo("node_visibility_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
}

VFXSceneTreePanel::VFXSceneTreePanel() {}
VFXSceneTreePanel::~VFXSceneTreePanel() {}

void VFXSceneTreePanel::_notification(int what) {
    if (what == NOTIFICATION_READY) {
        set_process(true);
        set_mouse_filter(Control::MOUSE_FILTER_STOP);
        set_focus_mode(Control::FOCUS_ALL);
        set_custom_minimum_size(Vector2(190, 180));

        toolbar = memnew(HBoxContainer);
        toolbar->set_custom_minimum_size(Vector2(0, TOOLBAR_HEIGHT));
        toolbar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        add_child(toolbar);

        search_edit = memnew(LineEdit);
        search_edit->set_placeholder("Filter nodes...");
        search_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        search_edit->set_custom_minimum_size(Vector2(70, 32));
        toolbar->add_child(search_edit);
        search_edit->connect("text_changed", callable_mp(this, &VFXSceneTreePanel::_on_search_changed));

        add_btn = memnew(Button);
        add_btn->set_text("+");
        add_btn->set_tooltip_text("Add custom scene node");
        toolbar->add_child(add_btn);
        add_btn->connect("pressed", callable_mp(this, &VFXSceneTreePanel::_on_add_pressed));

        multi_btn = memnew(Button);
        multi_btn->set_text("Multi");
        multi_btn->set_toggle_mode(true);
        multi_btn->set_tooltip_text("Toggle multiple selection");
        toolbar->add_child(multi_btn);
        multi_btn->connect("pressed", callable_mp(this, &VFXSceneTreePanel::_on_multi_pressed));

        reparent_btn = memnew(Button);
        reparent_btn->set_text("Parent");
        reparent_btn->set_toggle_mode(true);
        reparent_btn->set_tooltip_text("Drag selected nodes onto a parent; alternatively arm Parent and tap a destination");
        toolbar->add_child(reparent_btn);
        reparent_btn->connect("pressed", callable_mp(this, &VFXSceneTreePanel::_on_reparent_pressed));

        ui_font = get_theme_default_font();
        dirty = true;
    }
}

void VFXSceneTreePanel::set_scene(const Ref<VFXScene> &p_scene) {
    scene = p_scene;
    selected_ids.clear();
    active_id = -1;
    scroll_offset = 0.0f;
    mark_dirty();
}

Ref<VFXScene> VFXSceneTreePanel::get_scene() const { return scene; }
void VFXSceneTreePanel::mark_dirty() { dirty = true; queue_redraw(); }
void VFXSceneTreePanel::force_rebuild() { dirty = false; _rebuild_rows(); queue_redraw(); }

Ref<VFXSceneNode> VFXSceneTreePanel::_find_node(int id) const {
    if (!scene.is_valid() || id < 0) return Ref<VFXSceneNode>();
    Ref<VFXSceneNode> root = scene->get_root();
    return root.is_valid() ? root->find_node_by_id(id) : Ref<VFXSceneNode>();
}

Ref<VFXSceneNode> VFXSceneTreePanel::_active_node() const { return _find_node(active_id); }

void VFXSceneTreePanel::_append_rows(const Ref<VFXSceneNode> &node, int depth, const String &filter) {
    if (node.is_null()) return;
    bool matches = filter.is_empty() || node->get_node_name().to_lower().contains(filter);
    Array children = node->get_children();
    bool descendant_matches = false;
    if (!filter.is_empty() && !matches) {
        for (int i = 0; i < children.size(); ++i) {
            Ref<VFXSceneNode> child = children[i];
            if (child.is_valid()) {
                if (child->get_node_name().to_lower().contains(filter)) descendant_matches = true;
                Array descendants = child->get_all_descendants();
                for (int j = 0; !descendant_matches && j < descendants.size(); ++j) {
                    Ref<VFXSceneNode> d = descendants[j];
                    if (d.is_valid() && d->get_node_name().to_lower().contains(filter)) descendant_matches = true;
                }
            }
            if (descendant_matches) break;
        }
    }
    if (!matches && !descendant_matches) return;

    Row row;
    row.node = node;
    row.depth = depth;
    row.y = tree_top + rows.size() * row_height - scroll_offset;
    row.has_children = !children.is_empty();
    rows.push_back(row);

    if (!filter.is_empty() || node->get_expanded()) {
        for (int i = 0; i < children.size(); ++i) {
            Ref<VFXSceneNode> child = children[i];
            if (child.is_valid()) _append_rows(child, depth + 1, filter);
        }
    }
}

void VFXSceneTreePanel::_rebuild_rows() {
    rows.clear();
    if (!scene.is_valid()) { content_height = 0.0f; return; }
    Ref<VFXSceneNode> root = scene->get_root();
    if (root.is_null()) { content_height = 0.0f; return; }
    String filter = search_edit ? search_edit->get_text().strip_edges().to_lower() : String();
    _append_rows(root, 0, filter);
    content_height = rows.size() * row_height;
    float visible_height = MAX(0.0f, get_size().y - tree_top);
    scroll_offset = CLAMP(scroll_offset, 0.0f, MAX(0.0f, content_height - visible_height));
    for (size_t i = 0; i < rows.size(); ++i) rows[i].y = tree_top + i * row_height - scroll_offset;
}

void VFXSceneTreePanel::_draw() {
    if (dirty) { dirty = false; _rebuild_rows(); }
    draw_rect(Rect2(0, TOOLBAR_HEIGHT, get_size().x, MAX(0.0f, get_size().y - TOOLBAR_HEIGHT)), Color(0.105f, 0.105f, 0.105f, 1.0f));
    if (ui_font.is_null()) ui_font = get_theme_default_font();
    const int font_size = MAX(11, get_theme_default_font_size());

    for (const Row &row : rows) {
        if (row.y + row_height < tree_top || row.y > get_size().y) continue;
        const int id = row.node->get_node_id();
        Rect2 row_rect(0, row.y, get_size().x, row_height);
        if (selected_ids.count(id)) draw_rect(row_rect, id == active_id ? Color(0.16f, 0.31f, 0.43f) : Color(0.18f, 0.23f, 0.29f));
        if (id == drop_target_id) {
            draw_rect(Rect2(2, row.y + row_height - 3, get_size().x - 4, 2), Color(0.25f, 0.75f, 1.0f));
            draw_rect(row_rect, Color(0.18f, 0.42f, 0.55f, 0.24f));
        }

        float x = 7.0f + row.depth * indent_width;
        if (row.has_children) {
            String arrow = row.node->get_expanded() || !search_edit->get_text().is_empty() ? "▾" : "▸";
            draw_string(ui_font, Vector2(x + 2, row.y + 20), arrow, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0.72f, 0.75f, 0.79f));
        }
        x += 18.0f;
        String glyph = "◇";
        Color type_color = row.node->get_type_color();
        switch (row.node->get_node_type()) {
            case VFXSceneNode::NODE_MESH: glyph = "◈"; break;
            case VFXSceneNode::NODE_ARMATURE: glyph = "♙"; break;
            case VFXSceneNode::NODE_BONE: glyph = "⌁"; break;
            case VFXSceneNode::NODE_LIGHT: glyph = "☼"; break;
            case VFXSceneNode::NODE_CAMERA: glyph = "▣"; break;
            case VFXSceneNode::NODE_CURVE: glyph = "⌇"; break;
            default: break;
        }
        draw_string(ui_font, Vector2(x, row.y + 20), glyph, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size + 1, type_color);
        x += 20.0f;
        Color name_color = row.node->get_visible() ? type_color : type_color.darkened(0.55f);
        draw_string(ui_font, Vector2(x, row.y + 20), row.node->get_node_name(), HORIZONTAL_ALIGNMENT_LEFT, MAX(0.0f, get_size().x - x - VISIBILITY_WIDTH), font_size, name_color);
        String eye = row.node->get_visible() ? "◉" : "○";
        draw_string(ui_font, Vector2(get_size().x - 25, row.y + 20), eye, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0.72f, 0.75f, 0.79f));
        draw_line(Vector2(x, row.y + row_height - 1), Vector2(get_size().x - VISIBILITY_WIDTH, row.y + row_height - 1), Color(0.22f, 0.22f, 0.22f), 1.0f);
    }
    if (rows.empty()) {
        draw_string(ui_font, Vector2(12, tree_top + 24), scene.is_valid() ? "No nodes" : "No scene", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0.58f, 0.58f, 0.58f));
    }
    float viewport_h = MAX(1.0f, get_size().y - tree_top);
    if (content_height > viewport_h) {
        float thumb_h = MAX(28.0f, viewport_h * viewport_h / content_height);
        float thumb_y = tree_top + (viewport_h - thumb_h) * (scroll_offset / MAX(1.0f, content_height - viewport_h));
        draw_rect(Rect2(get_size().x - 4.0f, tree_top, 3.0f, viewport_h), Color(0.20f, 0.20f, 0.20f, 0.7f));
        draw_rect(Rect2(get_size().x - 4.0f, thumb_y, 3.0f, thumb_h), Color(0.55f, 0.58f, 0.62f, 0.85f));
    }
    if (pointer_down && dragging && drop_target_id >= 0) {
        draw_string(ui_font, Vector2(8, get_size().y - 8), "Release to parent", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color(0.35f, 0.82f, 1.0f));
    }
}

int VFXSceneTreePanel::_row_at_y(float y) const {
    if (y < tree_top || y >= get_size().y) return -1;
    int index = int(Math::floor((y - tree_top + scroll_offset) / row_height));
    return index >= 0 && index < (int)rows.size() ? index : -1;
}

int VFXSceneTreePanel::_row_index_for_id(int id) const {
    for (size_t i = 0; i < rows.size(); ++i) if (rows[i].node.is_valid() && rows[i].node->get_node_id() == id) return (int)i;
    return -1;
}

void VFXSceneTreePanel::_select_node(int id, bool toggle) {
    if (id < 0) return;
    if (!multi_select_enabled || !toggle) selected_ids.clear();
    if (multi_select_enabled && toggle && selected_ids.count(id)) selected_ids.erase(id);
    else selected_ids.insert(id);
    if (selected_ids.count(id)) active_id = id;
    _emit_selection();
    queue_redraw();
}

void VFXSceneTreePanel::_clear_selection() {
    selected_ids.clear();
    active_id = -1;
    if (scene.is_valid()) {
        Array all = scene->flatten_tree();
        for (int i = 0; i < all.size(); ++i) { Ref<VFXSceneNode> n = all[i]; if (n.is_valid()) n->set_selected(false); }
    }
    emit_signal("node_selected", Variant());
    emit_signal("nodes_selected", Array());
    queue_redraw();
}

void VFXSceneTreePanel::_emit_selection() {
    Array selected;
    if (scene.is_valid()) {
        Array all = scene->flatten_tree();
        for (int i = 0; i < all.size(); ++i) { Ref<VFXSceneNode> n = all[i]; if (n.is_valid()) n->set_selected(selected_ids.count(n->get_node_id()) != 0); }
        // Keep the active node last, because editor.gd uses the final array item.
        for (int i = 0; i < all.size(); ++i) {
            Ref<VFXSceneNode> n = all[i];
            if (n.is_valid() && selected_ids.count(n->get_node_id()) && n->get_node_id() != active_id) selected.append(n);
        }
        Ref<VFXSceneNode> active = _find_node(active_id);
        if (active.is_valid() && selected_ids.count(active_id)) selected.append(active);
    }
    emit_signal("nodes_selected", selected);
    if (!selected.is_empty()) emit_signal("node_selected", selected[selected.size() - 1]);
}

void VFXSceneTreePanel::_toggle_expanded(const Ref<VFXSceneNode> &node) {
    if (node.is_null() || node->get_child_count() == 0) return;
    node->set_expanded(!node->get_expanded());
    mark_dirty();
}

void VFXSceneTreePanel::_toggle_visibility(const Ref<VFXSceneNode> &node) {
    if (node.is_null()) return;
    node->set_visible(!node->get_visible());
    emit_signal("node_visibility_changed", node);
    queue_redraw();
}

void VFXSceneTreePanel::_begin_rename(int id) {
    Ref<VFXSceneNode> node = _find_node(id);
    int index = _row_index_for_id(id);
    if (node.is_null() || index < 0) return;
    if (rename_edit) { _commit_rename(); }
    renaming_node_id = id;
    rename_original = node->get_node_name();
    rename_edit = memnew(LineEdit);
    rename_edit->set_text(rename_original);
    rename_edit->set_select_all_on_focus(true);
    rename_edit->set_focus_mode(Control::FOCUS_ALL);
    const Row &row = rows[index];
    float x = 45.0f + row.depth * indent_width;
    rename_edit->set_position(Vector2(x, row.y + 1));
    rename_edit->set_size(Vector2(MAX(70.0f, get_size().x - x - VISIBILITY_WIDTH - 4), row_height - 2));
    add_child(rename_edit);
    rename_edit->connect("text_submitted", callable_mp(this, &VFXSceneTreePanel::_commit_rename));
    rename_edit->connect("focus_exited", callable_mp(this, &VFXSceneTreePanel::_commit_rename));
    rename_edit->grab_focus();
    rename_edit->select_all();
}

void VFXSceneTreePanel::_commit_rename() {
    if (!rename_edit) return;
    String new_name = rename_edit->get_text().strip_edges();
    Ref<VFXSceneNode> node = _find_node(renaming_node_id);
    if (node.is_valid() && !new_name.is_empty() && new_name != rename_original) {
        node->set_node_name(new_name);
        emit_signal("node_renamed", node, rename_original, new_name);
    }
    LineEdit *old = rename_edit;
    rename_edit = nullptr;
    renaming_node_id = -1;
    old->queue_free();
    mark_dirty();
    call_deferred("grab_focus");
}

void VFXSceneTreePanel::_cancel_rename() {
    if (!rename_edit) return;
    LineEdit *old = rename_edit;
    rename_edit = nullptr;
    renaming_node_id = -1;
    old->queue_free();
    call_deferred("grab_focus");
    queue_redraw();
}

void VFXSceneTreePanel::_on_search_changed(const String &) { mark_dirty(); }
void VFXSceneTreePanel::_on_add_pressed() {
    if (!scene.is_valid()) return;
    Ref<VFXSceneNode> parent = _active_node();
    if (parent.is_null()) parent = scene->get_root();
    Ref<VFXSceneNode> node = scene->create_node("NewNode", VFXSceneNode::NODE_EMPTY, parent);
    if (node.is_valid()) { node->set_expanded(true); mark_dirty(); force_rebuild(); _select_node(node->get_node_id(), false); _begin_rename(node->get_node_id()); }
}
void VFXSceneTreePanel::_on_multi_pressed() { multi_select_enabled = multi_btn && multi_btn->is_pressed(); if (!multi_select_enabled && selected_ids.size() > 1) { int keep = active_id; selected_ids.clear(); if (keep >= 0) selected_ids.insert(keep); _emit_selection(); } }
void VFXSceneTreePanel::_on_reparent_pressed() { reparent_mode = reparent_btn && reparent_btn->is_pressed(); }

bool VFXSceneTreePanel::_can_reparent(const Ref<VFXSceneNode> &node, const Ref<VFXSceneNode> &new_parent) const {
    if (!scene.is_valid() || node.is_null() || new_parent.is_null() || node == scene->get_root() || node == new_parent) return false;
    return !new_parent->is_descendant_of(node);
}

void VFXSceneTreePanel::_reparent_preserving_world(const Ref<VFXSceneNode> &node, const Ref<VFXSceneNode> &new_parent) {
    if (!_can_reparent(node, new_parent)) return;
    VFXSceneNode *old_parent_ptr = node->get_parent_node();
    Ref<VFXSceneNode> old_parent(old_parent_ptr);
    if (old_parent == new_parent) return;
    Transform3D world = node->get_global_transform();
    if (old_parent.is_valid()) old_parent->remove_child(node);
    new_parent->add_child(node);
    node->set_local_transform(new_parent->get_global_transform().affine_inverse() * world);
    emit_signal("node_reparented", node, old_parent, new_parent);
}

void VFXSceneTreePanel::_perform_drop(int target_id) {
    Ref<VFXSceneNode> destination = _find_node(target_id);
    if (destination.is_null()) return;
    Array moving;
    for (int id : selected_ids) { Ref<VFXSceneNode> n = _find_node(id); if (n.is_valid() && _can_reparent(n, destination)) moving.append(n); }
    Array roots;
    for (int i = 0; i < moving.size(); ++i) {
        Ref<VFXSceneNode> candidate = moving[i]; bool covered = false;
        for (int j = 0; j < moving.size(); ++j) { if (i == j) continue; Ref<VFXSceneNode> other = moving[j]; if (candidate->is_descendant_of(other)) { covered = true; break; } }
        if (!covered) roots.append(candidate);
    }
    bool changed = false;
    for (int i = 0; i < roots.size(); ++i) { Ref<VFXSceneNode> n = roots[i]; if (n->get_parent_node() != destination.ptr()) { _reparent_preserving_world(n, destination); changed = true; } }
    if (changed) { emit_signal("nodes_reparented"); mark_dirty(); force_rebuild(); }
    reparent_mode = false;
    if (reparent_btn) reparent_btn->set_pressed(false);
}

void VFXSceneTreePanel::_delete_selected_nodes() {
    if (!scene.is_valid()) return;
    Array selected;
    for (int id : selected_ids) { Ref<VFXSceneNode> n = _find_node(id); if (n.is_valid() && n != scene->get_root()) selected.append(n); }
    Array roots;
    for (int i = 0; i < selected.size(); ++i) {
        Ref<VFXSceneNode> candidate = selected[i]; bool covered = false;
        for (int j = 0; j < selected.size(); ++j) { if (i == j) continue; Ref<VFXSceneNode> other = selected[j]; if (candidate->is_descendant_of(other)) { covered = true; break; } }
        if (!covered) roots.append(candidate);
    }
    for (int i = 0; i < roots.size(); ++i) { Ref<VFXSceneNode> n = roots[i]; Ref<VFXSceneNode> parent(n->get_parent_node()); if (parent.is_valid()) { emit_signal("node_deleted", n, parent); scene->delete_node(n); } }
    selected_ids.clear(); active_id = -1;
    _clear_selection();
    mark_dirty(); force_rebuild();
}
void VFXSceneTreePanel::delete_selected_nodes() { _delete_selected_nodes(); }

void VFXSceneTreePanel::_set_drop_target_from_position(const Vector2 &position) {
    int index = _row_at_y(position.y);
    drop_target_id = index >= 0 ? rows[index].node->get_node_id() : -1;
    queue_redraw();
}

void VFXSceneTreePanel::_finish_pointer(bool released) {
    if (released && touch_scrolling) { /* swipe was a scroll gesture, not a row click */ }
    else if (released && long_press_rename && pressed_node_id >= 0) { _begin_rename(pressed_node_id); }
    else if (released && dragging && drop_target_id >= 0) _perform_drop(drop_target_id);
    else if (released && pressed_node_id >= 0 && !dragging) {
        int index = _row_index_for_id(pressed_node_id);
        if (index >= 0) {
            const Row &row = rows[index];
            float local_x = press_position.x - (7.0f + row.depth * indent_width);
            if (local_x >= 0.0f && press_position.x >= get_size().x - VISIBILITY_WIDTH) {
                _toggle_visibility(row.node);
            } else if (press_position.x < 32.0f + row.depth * indent_width && row.has_children) {
                _toggle_expanded(row.node);
            } else if (reparent_mode) {
                _perform_drop(pressed_node_id);
            } else if (pressed_was_selected && multi_select_enabled) {
                selected_ids.erase(pressed_node_id);
                if (active_id == pressed_node_id) active_id = selected_ids.empty() ? -1 : *selected_ids.rbegin();
                _emit_selection();
            }
        }
    }
    pointer_down = false; pressed_was_selected = false; dragging = false; long_press_rename = false; touch_drag_armed = false; touch_scrolling = false; pressed_node_id = -1; drop_target_id = -1; pressed_touch_index = -1; press_elapsed = 0.0f;
    queue_redraw();
}

void VFXSceneTreePanel::_gui_input(const Ref<InputEvent> &event) {
    if (event.is_null()) return;
    if (event->is_class("InputEventMouseButton")) {
        Ref<InputEventMouseButton> mb = event;
        if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP && mb->is_pressed()) { scroll_offset = MAX(0.0f, scroll_offset - row_height * 3.0f); mark_dirty(); accept_event(); return; }
        if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN && mb->is_pressed()) { scroll_offset += row_height * 3.0f; mark_dirty(); accept_event(); return; }
        if (mb->get_button_index() == MOUSE_BUTTON_LEFT) {
            if (mb->is_pressed()) {
                if (mb->is_double_click()) { int hit = _row_at_y(mb->get_position().y); if (hit >= 0) { _begin_rename(rows[hit].node->get_node_id()); accept_event(); return; } }
                if (rename_edit) { _commit_rename(); }
                pointer_down = true; dragging = false; press_position = mb->get_position(); pointer_position = press_position; press_elapsed = 0.0f;
                int row = _row_at_y(press_position.y); pressed_node_id = row >= 0 ? rows[row].node->get_node_id() : -1;
                pressed_was_selected = pressed_node_id >= 0 && selected_ids.count(pressed_node_id) != 0;
                if (pressed_node_id >= 0 && !reparent_mode) { if (!multi_select_enabled && !selected_ids.count(pressed_node_id)) { selected_ids.clear(); selected_ids.insert(pressed_node_id); active_id = pressed_node_id; _emit_selection(); } else if (!selected_ids.count(pressed_node_id)) { selected_ids.insert(pressed_node_id); active_id = pressed_node_id; _emit_selection(); } }
                accept_event();
            } else if (pointer_down) { _finish_pointer(true); accept_event(); }
        }
    } else if (event->is_class("InputEventMouseMotion")) {
        Ref<InputEventMouseMotion> mm = event;
        if (pointer_down) { pointer_position = mm->get_position(); if (!dragging && press_position.distance_to(pointer_position) > drag_threshold && pressed_node_id >= 0) dragging = true; if (dragging) _set_drop_target_from_position(pointer_position); accept_event(); }
    } else if (event->is_class("InputEventScreenTouch")) {
        Ref<InputEventScreenTouch> touch = event;
        if (touch->is_pressed()) {
            pointer_down = true; dragging = false; long_press_rename = false; touch_drag_armed = false; touch_scrolling = false; pressed_touch_index = touch->get_index(); press_position = touch->get_position(); pointer_position = press_position; press_elapsed = 0.0f;
            int row = _row_at_y(press_position.y); pressed_node_id = row >= 0 ? rows[row].node->get_node_id() : -1;
            pressed_was_selected = pressed_node_id >= 0 && selected_ids.count(pressed_node_id) != 0;
            if (pressed_node_id >= 0 && !reparent_mode) { if (!multi_select_enabled && !selected_ids.count(pressed_node_id)) { selected_ids.clear(); selected_ids.insert(pressed_node_id); active_id = pressed_node_id; _emit_selection(); } else if (!selected_ids.count(pressed_node_id)) { selected_ids.insert(pressed_node_id); active_id = pressed_node_id; _emit_selection(); } }
            accept_event();
        } else if (pointer_down && touch->get_index() == pressed_touch_index) { _finish_pointer(true); accept_event(); }
    } else if (event->is_class("InputEventScreenDrag")) {
        Ref<InputEventScreenDrag> drag = event;
        if (pointer_down && drag->get_index() == pressed_touch_index) {
            pointer_position = drag->get_position();
            float distance = press_position.distance_to(pointer_position);
            if (!dragging && distance > drag_threshold) {
                if (!touch_drag_armed) {
                    // A quick finger swipe scrolls the hierarchy. Hold briefly
                    // before moving to initiate a node drag, like the desktop drag gesture.
                    touch_scrolling = true;
                    float dy = press_position.y - pointer_position.y;
                    scroll_offset = CLAMP(scroll_offset + dy, 0.0f, MAX(0.0f, content_height - (get_size().y - tree_top)));
                    press_position = pointer_position;
                    mark_dirty();
                } else {
                    dragging = true;
                    long_press_rename = false;
                }
            }
            if (dragging) _set_drop_target_from_position(pointer_position);
            accept_event();
        }
    }
}

void VFXSceneTreePanel::_process(double delta) {
    if (dirty) { dirty = false; _rebuild_rows(); queue_redraw(); }
    if (pointer_down && pressed_touch_index >= 0 && !dragging && pressed_node_id >= 0) {
        press_elapsed += (float)delta;
        if (press_elapsed >= 0.20f) touch_drag_armed = true;
        if (press_elapsed >= 0.70f && !touch_scrolling) long_press_rename = true;
    }
}

