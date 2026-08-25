#include "vfx_scene_tree_panel.h"
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>

using namespace godot;

void VFXSceneTreePanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VFXSceneTreePanel::set_scene);
    ClassDB::bind_method(D_METHOD("get_scene"), &VFXSceneTreePanel::get_scene);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, "VFXScene"), "set_scene", "get_scene");

    ClassDB::bind_method(D_METHOD("mark_dirty"), &VFXSceneTreePanel::mark_dirty);
    ClassDB::bind_method(D_METHOD("force_rebuild"), &VFXSceneTreePanel::force_rebuild);

    ADD_SIGNAL(MethodInfo("node_selected", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("node_activated", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    ADD_SIGNAL(MethodInfo("nodes_reparented"));
    ADD_SIGNAL(MethodInfo("node_visibility_changed",
    PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "VFXSceneNode")));
    
}

VFXSceneTreePanel::VFXSceneTreePanel() {}
VFXSceneTreePanel::~VFXSceneTreePanel() {}

void VFXSceneTreePanel::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {
        set_custom_minimum_size(Vector2(180, 200));

        // --- Toolbar ---
        HBoxContainer* toolbar = memnew(HBoxContainer);
        add_child(toolbar);

        search_edit = memnew(LineEdit);
        search_edit->set_placeholder("Filter nodes...");
        search_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        toolbar->add_child(search_edit);
        search_edit->connect("text_changed", callable_mp(this, &VFXSceneTreePanel::_on_search_text_changed));

        add_btn = memnew(Button);
        add_btn->set_text("+");
        add_btn->set_tooltip_text("Add node");
        toolbar->add_child(add_btn);
        add_btn->connect("pressed", callable_mp(this, &VFXSceneTreePanel::_on_add_pressed));

        // --- Tree ---
        tree = memnew(Tree);
        tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
        tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
        tree->set_hide_root(false);
        tree->set_select_mode(Tree::SELECT_MULTI);
        tree->set_allow_reselect(true);
        tree->set_allow_rmb_select(true);
        add_child(tree);

        // Columns: [Name | Visibility button]
        tree->set_columns(2);
        tree->set_column_title(0, "Node");
        tree->set_column_expand(0, true);
        tree->set_column_expand(1, false);
        tree->set_column_custom_minimum_width(1, 22);
        tree->set_column_title_alignment(0, HorizontalAlignment::HORIZONTAL_ALIGNMENT_LEFT);

        // Signals
        tree->connect("item_selected", callable_mp(this, &VFXSceneTreePanel::_on_tree_item_selected));
        tree->connect("item_edited", callable_mp(this, &VFXSceneTreePanel::_on_tree_item_edited));
        tree->connect("button_clicked", callable_mp(this, &VFXSceneTreePanel::_on_tree_button_clicked));
        tree->connect("item_collapsed", callable_mp(this, &VFXSceneTreePanel::_on_tree_item_collapsed));
        tree->connect("item_activated", callable_mp(this, &VFXSceneTreePanel::_on_tree_item_activated));
        tree->connect("nothing_selected", callable_mp(this, &VFXSceneTreePanel::_on_tree_nothing_selected));
        tree->connect("item_mouse_selected", callable_mp(this, &VFXSceneTreePanel::_on_tree_item_mouse_selected));

        // Load icons (fallback to empty if not found)
        ResourceLoader* rl = ResourceLoader::get_singleton();
        icon_mesh = rl->load("res://addons/vfx_toolkit/icons/mesh.svg");
        icon_armature = rl->load("res://addons/vfx_toolkit/icons/armature.svg");
        icon_bone = rl->load("res://addons/vfx_toolkit/icons/bone.svg");
        icon_light = rl->load("res://addons/vfx_toolkit/icons/light.svg");
        icon_camera = rl->load("res://addons/vfx_toolkit/icons/camera.svg");
        icon_curve = rl->load("res://addons/vfx_toolkit/icons/curve.svg");
        icon_empty = rl->load("res://addons/vfx_toolkit/icons/empty.svg");
        icon_visible = rl->load("res://addons/vfx_toolkit/icons/visible.svg");
        icon_hidden = rl->load("res://addons/vfx_toolkit/icons/hidden.svg");

        mark_dirty();
    }

    if (p_what == NOTIFICATION_PROCESS) {
        if (tree_dirty && scene.is_valid()) {
            tree_dirty = false;
            _build_tree();
        }
    }
}

void VFXSceneTreePanel::set_scene(const Ref<VFXScene>& p_scene) {
    scene = p_scene;
    mark_dirty();
}

Ref<VFXScene> VFXSceneTreePanel::get_scene() const {
    return scene;
}

void VFXSceneTreePanel::mark_dirty() {
    tree_dirty = true;
}

void VFXSceneTreePanel::force_rebuild() {
    tree_dirty = false;
    _build_tree();
}

void VFXSceneTreePanel::_clear_tree_maps() {
    node_to_item.clear();
    item_to_node.clear();
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

void VFXSceneTreePanel::_build_tree() {
    if (!tree || !scene.is_valid()) return;
    syncing = true;

    // Save selection state using get_next_selected iteration
    PackedInt32Array old_selection;
    TreeItem* sel = tree->get_selected();
    while (sel) {
        auto itf = item_to_node.find(sel);
        if (itf != item_to_node.end()) old_selection.append(itf->second);
        sel = tree->get_next_selected(sel);
    }

    tree->clear();
    _clear_tree_maps();

    Ref<VFXSceneNode> root = scene->get_root();
    if (root.is_null()) {
        syncing = false;
        return;
    }

    TreeItem* root_item = tree->create_item();
    root_item->set_text(0, root->get_node_name());
    root_item->set_icon(0, _get_icon_for_type(root->get_node_type()));
    root_item->set_metadata(0, root->get_node_id());
    root_item->set_editable(0, true);
    root_item->set_selectable(0, true);
    root_item->set_collapsed(!root->get_expanded());

    // Visibility button
    root_item->add_button(1, root->get_visible() ? icon_visible : icon_hidden,
                          root->get_visible() ? 0 : 1, false,
                          root->get_visible() ? "Hide" : "Show");

    node_to_item[root->get_node_id()] = root_item;
    item_to_node[root_item] = root->get_node_id();

    _build_tree_recursive(root_item, root);

    // Restore selection
    for (int i = 0; i < old_selection.size(); i++) {
        TreeItem* it = _get_item_for_node(old_selection[i]);
        if (it) it->select(0);
    }

    syncing = false;
}

void VFXSceneTreePanel::_build_tree_recursive(TreeItem* parent_item, const Ref<VFXSceneNode>& node) {
    Array children = node->get_children();
    String filter = search_edit ? search_edit->get_text().to_lower() : String();

    for (int i = 0; i < children.size(); i++) {
        Ref<VFXSceneNode> child = children[i];
        if (child.is_null()) continue;

        // Filter: if name doesn't match, skip unless a descendant matches
        bool name_matches = filter.is_empty() || child->get_node_name().to_lower().contains(filter);
        bool descendant_matches = false;
        if (!name_matches) {
            Array desc = child->get_all_descendants();
            for (int d = 0; d < desc.size(); d++) {
                Ref<VFXSceneNode> dd = desc[d];
                if (dd.is_valid() && dd->get_node_name().to_lower().contains(filter)) {
                    descendant_matches = true;
                    break;
                }
            }
        }

        if (!filter.is_empty() && !name_matches && !descendant_matches) continue;

        TreeItem* item = tree->create_item(parent_item);
        item->set_text(0, child->get_node_name());
        item->set_icon(0, _get_icon_for_type(child->get_node_type()));
       	item->set_metadata(0, child);  // Store the node object directly
        item->set_editable(0, true);
        item->set_selectable(0, true);
        item->set_collapsed(!child->get_expanded());

        // Color tint by type
        Color type_color = child->get_type_color();
        item->set_custom_color(0, type_color);

        // Visibility button
        item->add_button(1, child->get_visible() ? icon_visible : icon_hidden,
                         child->get_visible() ? 0 : 1, false,
                         child->get_visible() ? "Hide" : "Show");

        node_to_item[child->get_node_id()] = item;
        item_to_node[item] = child->get_node_id();

        _build_tree_recursive(item, child);
    }
}

Ref<VFXSceneNode> VFXSceneTreePanel::_get_node_for_item(TreeItem* item) {
    if (!item) return Ref<VFXSceneNode>();
    
    // Read node directly from metadata
    Variant meta = item->get_metadata(0);
    if (meta.get_type() == Variant::OBJECT) {
        Ref<VFXSceneNode> node = meta;
        if (node.is_valid()) return node;
    }
    
    // Fallback to ID map
    if (!scene.is_valid()) return Ref<VFXSceneNode>();
    auto it = item_to_node.find(item);
    if (it == item_to_node.end()) return Ref<VFXSceneNode>();
    Ref<VFXSceneNode> root = scene->get_root();
    if (root.is_null()) return Ref<VFXSceneNode>();
    return root->find_node_by_id(it->second);
}


TreeItem* VFXSceneTreePanel::_get_item_for_node(int node_id) const {
    auto it = node_to_item.find(node_id);
    if (it == node_to_item.end()) return nullptr;
    return it->second;
}

// --- Signal handlers ---

void VFXSceneTreePanel::_on_tree_item_selected() {
    if (syncing) return;
    _sync_selection_to_scene();
}

void VFXSceneTreePanel::_on_tree_nothing_selected() {
    if (syncing) return;
    if (scene.is_valid()) {
        Array all = scene->flatten_tree();
        for (int i = 0; i < all.size(); i++) {
            Ref<VFXSceneNode> n = all[i];
            if (n.is_valid()) n->set_selected(false);
        }
    }
    emit_signal("node_selected", Variant());
}

void VFXSceneTreePanel::_sync_selection_to_scene() {
    if (!scene.is_valid()) return;

    Array all = scene->flatten_tree();
    for (int i = 0; i < all.size(); i++) {
        Ref<VFXSceneNode> n = all[i];
        if (n.is_valid()) n->set_selected(false);
    }

    // Iterate multi-selection via get_next_selected
    TreeItem* sel = tree->get_selected();
    while (sel) {
        Ref<VFXSceneNode> n = _get_node_for_item(sel);
        if (n.is_valid()) {
            n->set_selected(true);
            emit_signal("node_selected", n);
        }
        sel = tree->get_next_selected(sel);
    }
}

void VFXSceneTreePanel::_on_tree_item_edited() {
    TreeItem* item = tree->get_edited();
    if (!item) return;
    Ref<VFXSceneNode> node = _get_node_for_item(item);
    if (node.is_valid()) {
        node->set_node_name(item->get_text(0));
    }
}

void VFXSceneTreePanel::_on_tree_button_clicked(TreeItem* item, int column, int id, int mouse_button_idx) {
    if (column != 1) return;
    Ref<VFXSceneNode> node = _get_node_for_item(item);
    if (node.is_null()) return;

    bool new_vis = !node->get_visible();
    node->set_visible(new_vis);

    // Update button
    item->erase_button(column, 0);
    item->add_button(column, new_vis ? icon_visible : icon_hidden, new_vis ? 0 : 1, false, new_vis ? "Hide" : "Show");
    emit_signal("node_visibility_changed", node);
}

void VFXSceneTreePanel::_on_tree_item_collapsed(TreeItem* item) {
    if (syncing) return;
    Ref<VFXSceneNode> node = _get_node_for_item(item);
    if (node.is_valid()) {
        node->set_expanded(!item->is_collapsed());
    }
}

void VFXSceneTreePanel::_on_tree_item_activated() {
    TreeItem* item = tree->get_selected();
    Ref<VFXSceneNode> node = _get_node_for_item(item);
    if (node.is_valid()) emit_signal("node_activated", node);
}

void VFXSceneTreePanel::_on_search_text_changed(const String& text) {
    mark_dirty();
}

void VFXSceneTreePanel::_on_add_pressed() {
    if (!scene.is_valid()) return;
    Ref<VFXSceneNode> parent;
    TreeItem* sel = tree->get_selected();
    if (sel) parent = _get_node_for_item(sel);
    Ref<VFXSceneNode> new_node = scene->create_node("NewNode", VFXSceneNode::NODE_EMPTY, parent);
    if (new_node.is_valid()) {
        new_node->set_expanded(true);
        mark_dirty();
    }
}

void VFXSceneTreePanel::_on_tree_item_mouse_selected(const Vector2& mouse_pos, int mouse_button_idx) {
    if (mouse_button_idx == MOUSE_BUTTON_RIGHT) {
        TreeItem* item = tree->get_item_at_position(mouse_pos);
        if (item) {
            tree->deselect_all();
            item->select(0);
            _sync_selection_to_scene();
        }
    }
}

// --- Drag & Drop Reparenting ---

void VFXSceneTreePanel::_reparent_item(TreeItem* item, TreeItem* new_parent_item) {
    if (!scene.is_valid() || !item || !new_parent_item) return;
    if (item == new_parent_item) return;

    Ref<VFXSceneNode> node = _get_node_for_item(item);
    Ref<VFXSceneNode> new_parent = _get_node_for_item(new_parent_item);
    if (node.is_null() || new_parent.is_null()) return;

    // Prevent cycles
    if (new_parent->is_descendant_of(node)) return;

    // Do the reparent in scene
    Ref<VFXSceneNode> old_parent = node->get_parent_node();
    if (old_parent.is_valid()) {
        old_parent->remove_child(node);
    }
    new_parent->add_child(node);
    node->set_parent_node(new_parent.ptr());

    emit_signal("nodes_reparented");
    mark_dirty();
}
