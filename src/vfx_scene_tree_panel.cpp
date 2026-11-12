#include "vfx_scene_tree_panel.h"

#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>

using namespace godot;

namespace {
enum ContextAction { ACTION_RENAME = 1, ACTION_DELETE = 2, ACTION_ADD_CHILD = 3, ACTION_COLLAPSE = 4, ACTION_EXPAND = 5 };
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
    ADD_SIGNAL(MethodInfo("node_visibility_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("node_renamed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode"), PropertyInfo(Variant::STRING, "old_name"), PropertyInfo(Variant::STRING, "new_name")));
    ADD_SIGNAL(MethodInfo("node_deleted", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode"), PropertyInfo(Variant::OBJECT, "old_parent", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("node_reparented", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode"), PropertyInfo(Variant::OBJECT, "old_parent", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode"), PropertyInfo(Variant::OBJECT, "new_parent", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("nodes_reparented"));
}

VFXSceneTreePanel::VFXSceneTreePanel() {}
VFXSceneTreePanel::~VFXSceneTreePanel() {}

void VFXSceneTreePanel::_notification(int what) {
    if (what == NOTIFICATION_READY) {
        set_clip_contents(true);
        set_focus_mode(FOCUS_ALL);
        set_process(true);
        set_custom_minimum_size(Vector2(180, 200));

        toolbar = memnew(HBoxContainer);
        toolbar->set_anchors_and_offsets_preset(Control::PRESET_TOP_WIDE);
        toolbar->set_offset(SIDE_BOTTOM, 34.0);
        toolbar->set_custom_minimum_size(Vector2(0, 34));
        add_child(toolbar);

        search_edit = memnew(LineEdit);
        search_edit->set_placeholder("Filter Nodes");
        search_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        toolbar->add_child(search_edit);
        search_edit->connect("text_changed", callable_mp(this, &VFXSceneTreePanel::_on_search_changed));

        add_btn = memnew(Button);
        add_btn->set_text("+");
        add_btn->set_tooltip_text("Add New Container");
        toolbar->add_child(add_btn);
        add_btn->connect("pressed", callable_mp(this, &VFXSceneTreePanel::_on_add_pressed));

        rename_edit = memnew(LineEdit);
        rename_edit->set_visible(false);
        rename_edit->set_z_index(10);
        add_child(rename_edit);
        rename_edit->connect("text_submitted", callable_mp(this, &VFXSceneTreePanel::_on_rename_finished));
        rename_edit->connect("focus_exited", callable_mp(this, &VFXSceneTreePanel::_on_rename_focus_exited));

        context_menu = memnew(PopupMenu);
        add_child(context_menu);
        context_menu->add_item("Add Child Container", ACTION_ADD_CHILD);
        context_menu->add_separator();
        context_menu->add_item("Rename", ACTION_RENAME);
        context_menu->add_item("Expand", ACTION_EXPAND);
        context_menu->add_item("Collapse", ACTION_COLLAPSE);
        context_menu->add_separator();
        context_menu->add_item("Delete", ACTION_DELETE);
        context_menu->connect("id_pressed", callable_mp(this, &VFXSceneTreePanel::_on_context_id_pressed));

        _load_icons();
        _rebuild_rows();
    }
    if (what == NOTIFICATION_PROCESS && pointer_down && !long_press_fired && pressed_id >= 0) {
        const uint64_t now = Time::get_singleton()->get_ticks_msec();
        if (now - press_time_msec >= 520 && press_position.distance_to(last_pointer_position) < 12.0f) {
            long_press_fired = true;
            _show_context_menu(pressed_id, last_pointer_position);
        }
    }
}

void VFXSceneTreePanel::_load_icons() {
    ResourceLoader *loader = ResourceLoader::get_singleton();
    icon_mesh = loader->load("res://addons/vfx_toolkit/icons/mesh.svg");
    icon_armature = loader->load("res://addons/vfx_toolkit/icons/armature.svg");
    icon_bone = loader->load("res://addons/vfx_toolkit/icons/bone.svg");
    icon_light = loader->load("res://addons/vfx_toolkit/icons/light.svg");
    icon_camera = loader->load("res://addons/vfx_toolkit/icons/camera.svg");
    icon_curve = loader->load("res://addons/vfx_toolkit/icons/curve.svg");
    icon_empty = loader->load("res://addons/vfx_toolkit/icons/empty.svg");
    icon_visible = loader->load("res://addons/vfx_toolkit/icons/visible.svg");
    icon_hidden = loader->load("res://addons/vfx_toolkit/icons/hidden.svg");
}

Ref<Texture2D> VFXSceneTreePanel::_get_icon_for_type(int type) const {
    switch (type) {
        case VFXSceneNode::NODE_MESH: return icon_mesh;
        case VFXSceneNode::NODE_ARMATURE: return icon_armature;
        case VFXSceneNode::NODE_BONE: return icon_bone;
        case VFXSceneNode::NODE_LIGHT: return icon_light;
        case VFXSceneNode::NODE_CAMERA: return icon_camera;
        case VFXSceneNode::NODE_CURVE: return icon_curve;
        default: return icon_empty;
    }
}

void VFXSceneTreePanel::set_scene(const Ref<VFXScene> &p_scene) {
    scene = p_scene;
    selected_ids.clear(); active_id = -1; anchor_id = -1;
    force_rebuild();
}
Ref<VFXScene> VFXSceneTreePanel::get_scene() const { return scene; }
void VFXSceneTreePanel::mark_dirty() { queue_redraw(); }
void VFXSceneTreePanel::force_rebuild() { _rebuild_rows(); queue_redraw(); }

bool VFXSceneTreePanel::_subtree_matches(const Ref<VFXSceneNode> &node, const String &filter) const {
    if (node.is_null()) return false;
    if (node->get_node_name().to_lower().contains(filter)) return true;
    Array children = node->get_children();
    for (int i = 0; i < children.size(); ++i) {
        Ref<VFXSceneNode> child = children[i];
        if (_subtree_matches(child, filter)) return true;
    }
    return false;
}

void VFXSceneTreePanel::_append_rows(const Ref<VFXSceneNode> &node, int depth, const String &filter) {
    if (node.is_null()) return;
    const bool matches = filter.is_empty() || _subtree_matches(node, filter);
    if (!matches) return;
    Row row; row.node = node; row.depth = depth; row.has_children = node->get_child_count() > 0;
    rows.push_back(row);
    const bool force_open = !filter.is_empty();
    if (node->get_expanded() || force_open) {
        Array children = node->get_children();
        for (int i = 0; i < children.size(); ++i) _append_rows(children[i], depth + 1, filter);
    }
}

void VFXSceneTreePanel::_rebuild_rows() {
    rows.clear(); row_y_by_id.clear(); content_height = 0.0f;
    if (!scene.is_valid() || scene->get_root().is_null()) { queue_redraw(); return; }
    const String filter = search_edit ? search_edit->get_text().strip_edges().to_lower() : String();
    _append_rows(scene->get_root(), 0, filter);
    const float top = _tree_top();
    for (size_t i = 0; i < rows.size(); ++i) {
        rows[i].y = top + float(i) * row_height;
        row_y_by_id[rows[i].node->get_node_id()] = rows[i].y;
    }
    content_height = float(rows.size()) * row_height;
    const float viewport = MAX(0.0f, get_size().y - top);
    scroll_offset = CLAMP(scroll_offset, 0.0f, MAX(0.0f, content_height - viewport));
    queue_redraw();
}

float VFXSceneTreePanel::_tree_top() const { return toolbar ? MAX(36.0f, toolbar->get_size().y + 2.0f) : 36.0f; }
Ref<VFXSceneNode> VFXSceneTreePanel::_find_node(int id) const {
    if (!scene.is_valid() || scene->get_root().is_null()) return Ref<VFXSceneNode>();
    return scene->get_root()->find_node_by_id(id);
}
int VFXSceneTreePanel::_row_id_at(float y) const {
    const float local_y = y + scroll_offset;
    for (const Row &row : rows) if (local_y >= row.y && local_y < row.y + row_height) return row.node->get_node_id();
    return -1;
}
Ref<VFXSceneNode> VFXSceneTreePanel::_row_node_at(float y) const { return _find_node(_row_id_at(y)); }

void VFXSceneTreePanel::_draw_row(const Row &row) {
    const float y = row.y - scroll_offset;
    if (y + row_height < _tree_top() || y > get_size().y) return;
    const int id = row.node->get_node_id();
    const float left = 7.0f + row.depth * indent_width;
    const float right = get_size().x - 7.0f;
    if (selected_ids.count(id)) draw_rect(Rect2(2, y, get_size().x - 4, row_height), Color("#3d526d"));
    else if (id == drop_target_id) draw_rect(Rect2(2, y, get_size().x - 4, row_height), Color("#405d43"));

    if (row.has_children) {
        const float cx = left + 6.0f;
        const float cy = y + row_height * 0.5f;
        PackedVector2Array tri;
        if (row.node->get_expanded() || (search_edit && !search_edit->get_text().is_empty())) {
            tri.push_back(Vector2(cx - 3, cy - 2)); tri.push_back(Vector2(cx + 3, cy - 2)); tri.push_back(Vector2(cx, cy + 2));
        } else {
            tri.push_back(Vector2(cx - 2, cy - 4)); tri.push_back(Vector2(cx - 2, cy + 4)); tri.push_back(Vector2(cx + 3, cy));
        }
        draw_colored_polygon(tri, Color("#b9c0ca"));
    }

    Ref<Texture2D> type_icon = _get_icon_for_type(row.node->get_node_type());
    const float icon_x = left + 16.0f;
    if (type_icon.is_valid()) draw_texture_rect(type_icon, Rect2(icon_x, y + 5.0f, 17.0f, 17.0f), false);
    Ref<Font> font = get_theme_default_font();
    if (font.is_valid()) {
        Color text_color = row.node->get_type_color();
        if (selected_ids.count(id)) text_color = Color(1, 1, 1);
        draw_string(font, Vector2(icon_x + 23.0f, y + 18.5f), row.node->get_node_name(), HORIZONTAL_ALIGNMENT_LEFT, right - icon_x - 48.0f, 14, text_color);
    }
    Ref<Texture2D> vis = row.node->get_visible() ? icon_visible : icon_hidden;
    if (vis.is_valid()) draw_texture_rect(vis, Rect2(right - 18.0f, y + 6.0f, 16.0f, 16.0f), false);
}

void VFXSceneTreePanel::_draw_tree() {
    draw_rect(Rect2(0, _tree_top(), get_size().x, get_size().y - _tree_top()), Color("#202124"));
    for (const Row &row : rows) _draw_row(row);
    const float viewport = MAX(1.0f, get_size().y - _tree_top());
    if (content_height > viewport) {
        const float thumb_h = MAX(24.0f, viewport * viewport / content_height);
        const float thumb_y = _tree_top() + (viewport - thumb_h) * scroll_offset / MAX(1.0f, content_height - viewport);
        draw_rect(Rect2(get_size().x - 5.0f, thumb_y, 3.0f, thumb_h), Color(0.65f, 0.68f, 0.72f, 0.65f), true);
    }
}
void VFXSceneTreePanel::_draw() { _draw_tree(); }

void VFXSceneTreePanel::_clear_selection() {
    selected_ids.clear(); active_id = -1; anchor_id = -1;
    _sync_selection_to_scene();
}
void VFXSceneTreePanel::_select_node(int id, bool additive, bool range) {
    if (id < 0) { _clear_selection(); return; }
    if (range && anchor_id >= 0) {
        selected_ids.clear();
        bool in_range = false;
        for (const Row &row : rows) {
            const int rid = row.node->get_node_id();
            if (rid == anchor_id || rid == id) { if (!in_range) in_range = true; else { selected_ids.insert(rid); break; } }
            if (in_range) selected_ids.insert(rid);
        }
        selected_ids.insert(anchor_id); selected_ids.insert(id);
    } else if (additive) {
        if (selected_ids.count(id)) selected_ids.erase(id); else selected_ids.insert(id);
        anchor_id = id;
    } else {
        selected_ids.clear(); selected_ids.insert(id); anchor_id = id;
    }
    active_id = id;
    _sync_selection_to_scene();
    queue_redraw();
}
void VFXSceneTreePanel::_sync_selection_to_scene() {
    if (!scene.is_valid()) return;
    Array all = scene->flatten_tree();
    for (int i = 0; i < all.size(); ++i) { Ref<VFXSceneNode> n = all[i]; if (n.is_valid()) n->set_selected(selected_ids.count(n->get_node_id()) != 0); }
    Array selected;
    for (const Row &row : rows) if (selected_ids.count(row.node->get_node_id())) selected.append(row.node);
    Ref<VFXSceneNode> active = _find_node(active_id);
    if (active.is_valid()) { int idx = selected.find(active); if (idx >= 0 && idx != selected.size() - 1) { selected.remove_at(idx); selected.append(active); } }
    emit_signal("nodes_selected", selected);
    Variant active_variant;
    if (active.is_valid()) active_variant = active.ptr();
    emit_signal("node_selected", active_variant);
}
void VFXSceneTreePanel::_ensure_active_visible() {
    auto it = row_y_by_id.find(active_id); if (it == row_y_by_id.end()) return;
    const float top = _tree_top(); const float bottom = get_size().y;
    if (it->second - scroll_offset < top) scroll_offset = it->second - top;
    else if (it->second + row_height - scroll_offset > bottom) scroll_offset = it->second + row_height - bottom;
    _rebuild_rows();
}

void VFXSceneTreePanel::_begin_rename(int id) {
    Ref<VFXSceneNode> node = _find_node(id); if (node.is_null() || !rename_edit) return;
    rename_id = id;
    const float y = row_y_by_id.count(id) ? row_y_by_id[id] - scroll_offset : _tree_top();
    const Row *found = nullptr; for (const Row &r : rows) if (r.node->get_node_id() == id) { found = &r; break; }
    const float x = found ? 28.0f + found->depth * indent_width : 28.0f;
    rename_edit->set_position(Vector2(x, y + 2.0f));
    rename_edit->set_size(Vector2(MAX(80.0f, get_size().x - x - 28.0f), row_height - 4.0f));
    rename_edit->set_text(node->get_node_name()); rename_edit->set_visible(true); rename_edit->grab_focus(); rename_edit->select_all();
}
void VFXSceneTreePanel::_finish_rename() {
    if (rename_id < 0 || !rename_edit || !rename_edit->is_visible()) return;
    Ref<VFXSceneNode> node = _find_node(rename_id);
    if (node.is_valid()) {
        String old_name = node->get_node_name(); String new_name = rename_edit->get_text().strip_edges();
        if (!new_name.is_empty() && new_name != old_name) { node->set_node_name(new_name); emit_signal("node_renamed", node, old_name, new_name); }
    }
    rename_id = -1; rename_edit->set_visible(false); _rebuild_rows();
}
void VFXSceneTreePanel::_on_rename_finished(const String &text) { _finish_rename(); }
void VFXSceneTreePanel::_on_rename_focus_exited() { _finish_rename(); }

void VFXSceneTreePanel::_show_context_menu(int id, const Vector2 &screen_position) {
    Ref<VFXSceneNode> node = _find_node(id); if (node.is_null() || !context_menu) return;
    context_id = id;
    context_menu->set_item_text(3, node->get_expanded() ? "Collapse" : "Expand");
    context_menu->set_item_id(3, node->get_expanded() ? ACTION_COLLAPSE : ACTION_EXPAND);
    context_menu->set_position(Vector2i(get_global_transform().xform(screen_position)));
    context_menu->popup();
}
void VFXSceneTreePanel::_on_context_id_pressed(int action) {
    Ref<VFXSceneNode> node = _find_node(context_id); if (node.is_null()) return;
    if (action == ACTION_RENAME) _begin_rename(context_id);
    else if (action == ACTION_DELETE) { _select_node(context_id, true, false); _delete_selected(); }
    else if (action == ACTION_ADD_CHILD) {
        Ref<VFXSceneNode> added = scene->create_node("New Container", VFXSceneNode::NODE_EMPTY, node);
        if (added.is_valid()) { _rebuild_rows(); _select_node(added->get_node_id(), false, false); _begin_rename(added->get_node_id()); }
    } else if (action == ACTION_EXPAND || action == ACTION_COLLAPSE) { node->set_expanded(action == ACTION_EXPAND); _rebuild_rows(); }
}
void VFXSceneTreePanel::_delete_selected() {
    if (!scene.is_valid()) return;
    std::vector<Ref<VFXSceneNode>> victims;
    for (int id : selected_ids) { Ref<VFXSceneNode> n = _find_node(id); if (n.is_valid() && n != scene->get_root()) victims.push_back(n); }
    // Delete descendants only through their selected ancestor to avoid stale parent references.
    for (const Ref<VFXSceneNode> &n : victims) {
        bool covered = false; VFXSceneNode *p = n->get_parent_node();
        while (p) { if (selected_ids.count(p->get_node_id())) { covered = true; break; } p = p->get_parent_node(); }
        if (!covered) { Ref<VFXSceneNode> old_parent = n->get_parent_node(); scene->delete_node(n); emit_signal("node_deleted", n, old_parent); }
    }
    selected_ids.clear(); active_id = -1; anchor_id = -1; _sync_selection_to_scene(); _rebuild_rows();
}
void VFXSceneTreePanel::delete_selected_nodes() { _delete_selected(); }

void VFXSceneTreePanel::_reparent_node(int node_id, int parent_id) {
    if (!scene.is_valid() || node_id == parent_id) return;
    Ref<VFXSceneNode> node = _find_node(node_id), new_parent = _find_node(parent_id);
    if (node.is_null() || new_parent.is_null() || node == scene->get_root() || new_parent->is_descendant_of(node)) return;
    Ref<VFXSceneNode> old_parent = node->get_parent_node(); if (old_parent.is_null() || old_parent == new_parent) return;
    const Transform3D world_before = node->get_global_transform();
    const Transform3D parent_world = new_parent->get_global_transform();
    old_parent->remove_child(node);
    new_parent->add_child(node);
    node->set_parent_node(new_parent.ptr());
    node->set_local_transform(parent_world.affine_inverse() * world_before);
    emit_signal("node_reparented", node, old_parent, new_parent);
    emit_signal("nodes_reparented");
    _rebuild_rows();
}

void VFXSceneTreePanel::_on_search_changed(const String &text) { scroll_offset = 0.0f; _rebuild_rows(); }
void VFXSceneTreePanel::_on_add_pressed() {
    if (!scene.is_valid()) return;
    Ref<VFXSceneNode> parent = _find_node(active_id);
    if (parent.is_null()) parent = scene->get_root();
    Ref<VFXSceneNode> node = scene->create_node("New Container", VFXSceneNode::NODE_EMPTY, parent);
    if (node.is_valid()) { parent->set_expanded(true); _rebuild_rows(); _select_node(node->get_node_id(), false, false); _begin_rename(node->get_node_id()); }
}

void VFXSceneTreePanel::_gui_input(const Ref<InputEvent> &event) {
    if (event.is_null()) return;
    Ref<InputEventMouseButton> mb = event;
    if (mb.is_valid()) {
        const int button = mb->get_button_index(); const Vector2 pos = mb->get_position();
        if (button == MOUSE_BUTTON_WHEEL_UP && mb->is_pressed()) { scroll_offset = MAX(0.0f, scroll_offset - row_height * 3.0f); queue_redraw(); accept_event(); return; }
        if (button == MOUSE_BUTTON_WHEEL_DOWN && mb->is_pressed()) { scroll_offset = MIN(MAX(0.0f, content_height - (get_size().y - _tree_top())), scroll_offset + row_height * 3.0f); queue_redraw(); accept_event(); return; }
        if (button == MOUSE_BUTTON_LEFT && mb->is_pressed()) {
            grab_focus(); pointer_down = true; dragging = false; touch_scrolling = false; long_press_fired = false;
            press_position = pos; last_pointer_position = pos; press_time_msec = Time::get_singleton()->get_ticks_msec();
            pressed_id = _row_id_at(pos.y); drop_target_id = -1;
            if (pressed_id >= 0) {
                Ref<VFXSceneNode> node = _find_node(pressed_id);
                const Row *row = nullptr; for (const Row &r : rows) if (r.node->get_node_id() == pressed_id) { row = &r; break; }
                const float indent = row ? 7.0f + row->depth * indent_width : 7.0f;
                if (node.is_valid() && node->get_child_count() > 0 && pos.x >= indent && pos.x < indent + 14.0f) {
                    node->set_expanded(!node->get_expanded()); _rebuild_rows(); pressed_id = -1;
                } else if (pos.x >= get_size().x - 27.0f) {
                    node->set_visible(!node->get_visible()); emit_signal("node_visibility_changed", node); queue_redraw(); pressed_id = -1;
                } else {
                    Input *input = Input::get_singleton();
                    const bool additive = input->is_key_pressed(KEY_CTRL) || input->is_key_pressed(KEY_META);
                    const bool range = input->is_key_pressed(KEY_SHIFT);
                    _select_node(pressed_id, additive, range);
                }
            } else if (pos.y >= _tree_top()) _clear_selection();
            accept_event(); return;
        }
        if (button == MOUSE_BUTTON_LEFT && !mb->is_pressed()) {
            if (pointer_down && dragging && drop_target_id >= 0 && pressed_id >= 0) _reparent_node(pressed_id, drop_target_id);
            else if (pointer_down && !long_press_fired && pressed_id >= 0) {
                const uint64_t now = Time::get_singleton()->get_ticks_msec();
                if (last_tap_id == pressed_id && now - last_tap_time_msec < 360) _begin_rename(pressed_id);
                last_tap_id = pressed_id; last_tap_time_msec = now;
            }
            pointer_down = false; dragging = false; pressed_id = -1; drop_target_id = -1; queue_redraw(); accept_event(); return;
        }
        if (button == MOUSE_BUTTON_RIGHT && mb->is_pressed()) { const int id = _row_id_at(pos.y); if (id >= 0) { _select_node(id, false, false); _show_context_menu(id, pos); } accept_event(); return; }
    }
    Ref<InputEventMouseMotion> mm = event;
    if (mm.is_valid()) {
        last_pointer_position = mm->get_position();
        if (pointer_down && pressed_id >= 0 && press_position.distance_to(last_pointer_position) > 7.0f) {
            dragging = true; if (long_press_fired && context_menu) context_menu->hide(); drop_target_id = _row_id_at(last_pointer_position.y);
            if (drop_target_id == pressed_id) drop_target_id = -1;
            queue_redraw(); accept_event();
        }
    }
    Ref<InputEventScreenTouch> touch = event;
    if (touch.is_valid()) {
        Vector2 pos = touch->get_position();
        if (touch->is_pressed()) {
            pointer_down = true; touch_scrolling = false; dragging = false; long_press_fired = false;
            press_position = pos; last_pointer_position = pos; press_time_msec = Time::get_singleton()->get_ticks_msec(); pressed_id = _row_id_at(pos.y);
            if (pressed_id >= 0) _select_node(pressed_id, false, false);
        } else {
            if (pointer_down && dragging && drop_target_id >= 0 && pressed_id >= 0) _reparent_node(pressed_id, drop_target_id);
            else if (pointer_down && long_press_fired) { /* context menu already shown */ }
            else if (pointer_down && pressed_id >= 0) { const uint64_t now = Time::get_singleton()->get_ticks_msec(); if (last_tap_id == pressed_id && now - last_tap_time_msec < 360) _begin_rename(pressed_id); last_tap_id = pressed_id; last_tap_time_msec = now; }
            pointer_down = false; dragging = false; pressed_id = -1; drop_target_id = -1;
        }
        accept_event(); return;
    }
    Ref<InputEventScreenDrag> drag = event;
    if (drag.is_valid()) {
        const Vector2 pos = drag->get_position(); const Vector2 delta = drag->get_relative(); last_pointer_position = pos;
        if (pointer_down && press_position.distance_to(pos) > 10.0f) {
            // A vertical gesture that starts in the row body scrolls; a horizontal/held gesture can reparent.
            if (long_press_fired) { dragging = true; touch_scrolling = false; if (context_menu) context_menu->hide(); }
            else if (!dragging && ABS(delta.y) > ABS(delta.x) * 1.25f && press_position.distance_to(pos) > 18.0f) touch_scrolling = true;
            if (touch_scrolling) { scroll_offset = CLAMP(scroll_offset - delta.y, 0.0f, MAX(0.0f, content_height - (get_size().y - _tree_top()))); queue_redraw(); }
            else { dragging = true; drop_target_id = _row_id_at(pos.y); if (drop_target_id == pressed_id) drop_target_id = -1; queue_redraw(); }
        }
        accept_event(); return;
    }
    Ref<InputEventKey> key = event;
    if (key.is_valid()) {
        if (!key->is_pressed() || key->is_echo()) return;
        if (key->get_keycode() == KEY_DELETE || key->get_keycode() == KEY_BACKSPACE) { _delete_selected(); accept_event(); }
        else if (key->get_keycode() == KEY_F2 && active_id >= 0) { _begin_rename(active_id); accept_event(); }
        else if (key->get_keycode() == KEY_LEFT && active_id >= 0) { Ref<VFXSceneNode> n = _find_node(active_id); if (n.is_valid()) { n->set_expanded(false); _rebuild_rows(); } accept_event(); }
        else if (key->get_keycode() == KEY_RIGHT && active_id >= 0) { Ref<VFXSceneNode> n = _find_node(active_id); if (n.is_valid()) { n->set_expanded(true); _rebuild_rows(); } accept_event(); }
    }
}
