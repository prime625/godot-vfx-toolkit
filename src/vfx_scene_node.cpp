#include "vfx_scene_node.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void VFXSceneNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_node_name", "name"), &VFXSceneNode::set_node_name);
    ClassDB::bind_method(D_METHOD("get_node_name"), &VFXSceneNode::get_node_name);
    ClassDB::bind_method(D_METHOD("set_icon_id", "id"), &VFXSceneNode::set_icon_id);
    ClassDB::bind_method(D_METHOD("get_icon_id"), &VFXSceneNode::get_icon_id);
    ClassDB::bind_method(D_METHOD("set_icon_color", "color"), &VFXSceneNode::set_icon_color);
    ClassDB::bind_method(D_METHOD("get_icon_color"), &VFXSceneNode::get_icon_color);
    ClassDB::bind_method(D_METHOD("set_node_type", "type"), &VFXSceneNode::set_node_type);
    ClassDB::bind_method(D_METHOD("get_node_type"), &VFXSceneNode::get_node_type);

    ClassDB::bind_method(D_METHOD("set_transform", "transform"), &VFXSceneNode::set_transform);
    ClassDB::bind_method(D_METHOD("get_transform"), &VFXSceneNode::get_transform);
    ClassDB::bind_method(D_METHOD("get_global_transform"), &VFXSceneNode::get_global_transform);

    ClassDB::bind_method(D_METHOD("set_visible", "visible"), &VFXSceneNode::set_visible);
    ClassDB::bind_method(D_METHOD("is_visible"), &VFXSceneNode::is_visible);
    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &VFXSceneNode::set_selected);
    ClassDB::bind_method(D_METHOD("is_selected"), &VFXSceneNode::is_selected);
    ClassDB::bind_method(D_METHOD("set_locked", "locked"), &VFXSceneNode::set_locked);
    ClassDB::bind_method(D_METHOD("is_locked"), &VFXSceneNode::is_locked);

    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXSceneNode::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXSceneNode::get_mesh);
    ClassDB::bind_method(D_METHOD("set_skeleton", "skeleton"), &VFXSceneNode::set_skeleton);
    ClassDB::bind_method(D_METHOD("get_skeleton"), &VFXSceneNode::get_skeleton);
    ClassDB::bind_method(D_METHOD("set_skin", "skin"), &VFXSceneNode::set_skin);
    ClassDB::bind_method(D_METHOD("get_skin"), &VFXSceneNode::get_skin);
    ClassDB::bind_method(D_METHOD("set_animator", "animator"), &VFXSceneNode::set_animator);
    ClassDB::bind_method(D_METHOD("get_animator"), &VFXSceneNode::get_animator);

    ClassDB::bind_method(D_METHOD("add_child", "child"), &VFXSceneNode::add_child);
    ClassDB::bind_method(D_METHOD("remove_child", "child"), &VFXSceneNode::remove_child);
    ClassDB::bind_method(D_METHOD("remove_child_by_index", "idx"), &VFXSceneNode::remove_child_by_index);
    ClassDB::bind_method(D_METHOD("get_child_count"), &VFXSceneNode::get_child_count);
    ClassDB::bind_method(D_METHOD("get_child", "idx"), &VFXSceneNode::get_child);
    ClassDB::bind_method(D_METHOD("get_parent_node"), &VFXSceneNode::get_parent_node);
    ClassDB::bind_method(D_METHOD("clear_children"), &VFXSceneNode::clear_children);
    ClassDB::bind_method(D_METHOD("find_child_by_name", "name"), &VFXSceneNode::find_child_by_name);
    ClassDB::bind_method(D_METHOD("get_all_descendants"), &VFXSceneNode::get_all_descendants);

    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_EMPTY", NODE_EMPTY);
    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_MESH", NODE_MESH);
    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_CAMERA", NODE_CAMERA);
    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_LIGHT", NODE_LIGHT);
    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_ARMATURE", NODE_ARMATURE);
    ClassDB::bind_integer_constant(get_class_static(), "", "NODE_CURVE", NODE_CURVE);
}

VFXSceneNode::VFXSceneNode() {}
VFXSceneNode::~VFXSceneNode() { clear_children(); }

void VFXSceneNode::set_node_name(const String& p_name) { node_name = p_name; }
String VFXSceneNode::get_node_name() const { return node_name; }

void VFXSceneNode::set_icon_id(const String& p_id) { icon_id = p_id; }
String VFXSceneNode::get_icon_id() const { return icon_id; }

void VFXSceneNode::set_icon_color(const Color& p_color) { icon_color = p_color; }
Color VFXSceneNode::get_icon_color() const { return icon_color; }

void VFXSceneNode::set_node_type(int p_type) {
    type = (NodeType)p_type;
    // Auto-assign sensible icon if user hasn't set a custom one
    if (icon_id == "empty" || icon_id.is_empty()) {
        switch (type) {
            case NODE_MESH: icon_id = "mesh"; break;
            case NODE_CAMERA: icon_id = "camera"; break;
            case NODE_LIGHT: icon_id = "light"; break;
            case NODE_ARMATURE: icon_id = "armature"; break;
            case NODE_CURVE: icon_id = "curve"; break;
            default: icon_id = "empty"; break;
        }
    }
}
int VFXSceneNode::get_node_type() const { return (int)type; }

void VFXSceneNode::set_transform(const Transform3D& t) { local_transform = t; }
Transform3D VFXSceneNode::get_transform() const { return local_transform; }
Transform3D VFXSceneNode::get_global_transform() const {
    if (parent) return parent->get_global_transform() * local_transform;
    return local_transform;
}

void VFXSceneNode::set_visible(bool v) { visible = v; }
bool VFXSceneNode::is_visible() const { return visible; }
void VFXSceneNode::set_selected(bool s) { selected = s; }
bool VFXSceneNode::is_selected() const { return selected; }
void VFXSceneNode::set_locked(bool l) { locked = l; }
bool VFXSceneNode::is_locked() const { return locked; }

void VFXSceneNode::set_mesh(const Ref<VFXMesh>& p_mesh) { mesh = p_mesh; }
Ref<VFXMesh> VFXSceneNode::get_mesh() const { return mesh; }
void VFXSceneNode::set_skeleton(const Ref<VFXSkeleton>& p_sk) { skeleton = p_sk; }
Ref<VFXSkeleton> VFXSceneNode::get_skeleton() const { return skeleton; }
void VFXSceneNode::set_skin(const Ref<VFXSkin>& p_skin) { skin = p_skin; }
Ref<VFXSkin> VFXSceneNode::get_skin() const { return skin; }
void VFXSceneNode::set_animator(const Ref<VFXAnimator>& p_anim) { animator = p_anim; }
Ref<VFXAnimator> VFXSceneNode::get_animator() const { return animator; }

void VFXSceneNode::add_child(const Ref<VFXSceneNode>& child) {
    if (child.is_null() || child->parent == this) return;
    // Detach from old parent
    if (child->parent) {
        child->parent->remove_child(child);
    }
    child->parent = this;
    children.push_back(child);
}

void VFXSceneNode::remove_child(const Ref<VFXSceneNode>& child) {
    if (child.is_null()) return;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (*it == child) {
            (*it)->parent = nullptr;
            children.erase(it);
            return;
        }
    }
}

void VFXSceneNode::remove_child_by_index(int idx) {
    if (idx < 0 || idx >= (int)children.size()) return;
    children[idx]->parent = nullptr;
    children.erase(children.begin() + idx);
}

int VFXSceneNode::get_child_count() const { return children.size(); }
Ref<VFXSceneNode> VFXSceneNode::get_child(int idx) const {
    if (idx < 0 || idx >= (int)children.size()) return Ref<VFXSceneNode>();
    return children[idx];
}
VFXSceneNode* VFXSceneNode::get_parent_node() const { return parent; }

void VFXSceneNode::clear_children() {
    for (auto& c : children) {
        if (c.is_valid()) c->parent = nullptr;
    }
    children.clear();
}

bool VFXSceneNode::is_ancestor_of(const VFXSceneNode* node) const {
    if (!node) return false;
    VFXSceneNode* p = node->parent;
    while (p) {
        if (p == this) return true;
        p = p->parent;
    }
    return false;
}

Ref<VFXSceneNode> VFXSceneNode::find_child_by_name(const String& name) const {
    for (auto& c : children) {
        if (c.is_valid() && c->get_node_name() == name) return c;
        Ref<VFXSceneNode> r = c->find_child_by_name(name);
        if (r.is_valid()) return r;
    }
    return Ref<VFXSceneNode>();
}

void VFXSceneNode::get_all_descendants(Array& out) const {
    for (auto& c : children) {
        if (c.is_valid()) {
            out.append(c);
            c->get_all_descendants(out);
        }
    }
}

PackedByteArray VFXSceneNode::serialize() const {
    PackedByteArray data;
    data.append(0x56); data.append(0x53); data.append(0x4E); // VSN
    return data;
}
void VFXSceneNode::deserialize(const PackedByteArray& data) {}
