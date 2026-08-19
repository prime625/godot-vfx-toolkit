#include "vfx_scene_node.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void VFXSceneNode::_bind_methods() {
    BIND_ENUM_CONSTANT(NODE_EMPTY);
    BIND_ENUM_CONSTANT(NODE_MESH);
    BIND_ENUM_CONSTANT(NODE_ARMATURE);
    BIND_ENUM_CONSTANT(NODE_BONE);
    BIND_ENUM_CONSTANT(NODE_LIGHT);
    BIND_ENUM_CONSTANT(NODE_CAMERA);
    BIND_ENUM_CONSTANT(NODE_CURVE);

    ClassDB::bind_method(D_METHOD("set_node_id", "id"), &VFXSceneNode::set_node_id);
    ClassDB::bind_method(D_METHOD("get_node_id"), &VFXSceneNode::get_node_id);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "node_id"), "set_node_id", "get_node_id");

    ClassDB::bind_method(D_METHOD("set_node_name", "name"), &VFXSceneNode::set_node_name);
    ClassDB::bind_method(D_METHOD("get_node_name"), &VFXSceneNode::get_node_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "node_name"), "set_node_name", "get_node_name");

    ClassDB::bind_method(D_METHOD("set_node_type", "type"), &VFXSceneNode::set_node_type);
    ClassDB::bind_method(D_METHOD("get_node_type"), &VFXSceneNode::get_node_type);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "node_type"), "set_node_type", "get_node_type");

    ClassDB::bind_method(D_METHOD("get_parent_node"), &VFXSceneNode::get_parent_node);

    ClassDB::bind_method(D_METHOD("add_child", "child"), &VFXSceneNode::add_child);
    ClassDB::bind_method(D_METHOD("remove_child", "child"), &VFXSceneNode::remove_child);
    ClassDB::bind_method(D_METHOD("remove_child_by_id", "child_id"), &VFXSceneNode::remove_child_by_id);
    ClassDB::bind_method(D_METHOD("clear_children"), &VFXSceneNode::clear_children);
    ClassDB::bind_method(D_METHOD("get_child_count"), &VFXSceneNode::get_child_count);
    ClassDB::bind_method(D_METHOD("get_child", "index"), &VFXSceneNode::get_child);
    ClassDB::bind_method(D_METHOD("get_children"), &VFXSceneNode::get_children);
    ClassDB::bind_method(D_METHOD("has_child", "child"), &VFXSceneNode::has_child);
    ClassDB::bind_method(D_METHOD("is_ancestor_of", "node"), &VFXSceneNode::is_ancestor_of);
    ClassDB::bind_method(D_METHOD("is_descendant_of", "node"), &VFXSceneNode::is_descendant_of);

    ClassDB::bind_method(D_METHOD("find_node_by_id", "id"), &VFXSceneNode::find_node_by_id);
    ClassDB::bind_method(D_METHOD("find_node_by_name", "name"), &VFXSceneNode::find_node_by_name);
    ClassDB::bind_method(D_METHOD("find_child_by_name", "name"), &VFXSceneNode::find_child_by_name);
    ClassDB::bind_method(D_METHOD("get_all_descendants"), &VFXSceneNode::get_all_descendants);

    ClassDB::bind_method(D_METHOD("set_local_transform", "transform"), &VFXSceneNode::set_local_transform);
    ClassDB::bind_method(D_METHOD("get_local_transform"), &VFXSceneNode::get_local_transform);
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "local_transform"), "set_local_transform", "get_local_transform");

    ClassDB::bind_method(D_METHOD("set_transform", "transform"), &VFXSceneNode::set_transform);
    ClassDB::bind_method(D_METHOD("get_transform"), &VFXSceneNode::get_transform);

    ClassDB::bind_method(D_METHOD("set_local_position", "position"), &VFXSceneNode::set_local_position);
    ClassDB::bind_method(D_METHOD("get_local_position"), &VFXSceneNode::get_local_position);
    ClassDB::bind_method(D_METHOD("set_local_rotation", "rotation"), &VFXSceneNode::set_local_rotation);
    ClassDB::bind_method(D_METHOD("get_local_rotation"), &VFXSceneNode::get_local_rotation);
    ClassDB::bind_method(D_METHOD("set_local_scale", "scale"), &VFXSceneNode::set_local_scale);
    ClassDB::bind_method(D_METHOD("get_local_scale"), &VFXSceneNode::get_local_scale);

    ClassDB::bind_method(D_METHOD("set_visible", "visible"), &VFXSceneNode::set_visible);
    ClassDB::bind_method(D_METHOD("get_visible"), &VFXSceneNode::get_visible);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "get_visible");

    ClassDB::bind_method(D_METHOD("set_expanded", "expanded"), &VFXSceneNode::set_expanded);
    ClassDB::bind_method(D_METHOD("get_expanded"), &VFXSceneNode::get_expanded);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "expanded"), "set_expanded", "get_expanded");

    ClassDB::bind_method(D_METHOD("set_selected", "selected"), &VFXSceneNode::set_selected);
    ClassDB::bind_method(D_METHOD("get_selected"), &VFXSceneNode::get_selected);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selected"), "set_selected", "get_selected");

    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXSceneNode::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXSceneNode::get_mesh);
    ClassDB::bind_method(D_METHOD("set_skeleton", "skeleton"), &VFXSceneNode::set_skeleton);
    ClassDB::bind_method(D_METHOD("get_skeleton"), &VFXSceneNode::get_skeleton);
    ClassDB::bind_method(D_METHOD("set_skin", "skin"), &VFXSceneNode::set_skin);
    ClassDB::bind_method(D_METHOD("get_skin"), &VFXSceneNode::get_skin);
    ClassDB::bind_method(D_METHOD("set_animator", "animator"), &VFXSceneNode::set_animator);
    ClassDB::bind_method(D_METHOD("get_animator"), &VFXSceneNode::get_animator);

    ClassDB::bind_method(D_METHOD("set_bone_parent_index", "index"), &VFXSceneNode::set_bone_parent_index);
    ClassDB::bind_method(D_METHOD("get_bone_parent_index"), &VFXSceneNode::get_bone_parent_index);

    ClassDB::bind_method(D_METHOD("has_mesh"), &VFXSceneNode::has_mesh);
    ClassDB::bind_method(D_METHOD("has_skeleton"), &VFXSceneNode::has_skeleton);
    ClassDB::bind_method(D_METHOD("has_skin"), &VFXSceneNode::has_skin);
    ClassDB::bind_method(D_METHOD("has_animator"), &VFXSceneNode::has_animator);
    ClassDB::bind_method(D_METHOD("get_type_color"), &VFXSceneNode::get_type_color);
    ClassDB::bind_method(D_METHOD("get_type_icon_hint"), &VFXSceneNode::get_type_icon_hint);

    ClassDB::bind_method(D_METHOD("serialize"), &VFXSceneNode::serialize);
    ClassDB::bind_method(D_METHOD("deserialize", "data"), &VFXSceneNode::deserialize);
	ClassDB::bind_method(D_METHOD("is_visible"), &VFXSceneNode::is_visible);
	ClassDB::bind_method(D_METHOD("get_global_transform"), &VFXSceneNode::get_global_transform);
}

VFXSceneNode::VFXSceneNode() {}
VFXSceneNode::~VFXSceneNode() {
    for (auto& c : children) {
        if (c.is_valid()) c->parent_node = nullptr;
    }
    children.clear();
}

void VFXSceneNode::set_node_id(int p_id) { node_id = p_id; }
int VFXSceneNode::get_node_id() const { return node_id; }

void VFXSceneNode::set_node_name(const String& p_name) { node_name = p_name; }
String VFXSceneNode::get_node_name() const { return node_name; }

void VFXSceneNode::set_node_type(int p_type) {
    if (p_type >= NODE_EMPTY && p_type <= NODE_CURVE)
        node_type = static_cast<NodeType>(p_type);
}
int VFXSceneNode::get_node_type() const { return static_cast<int>(node_type); }

void VFXSceneNode::set_parent_node(VFXSceneNode* p_parent) { parent_node = p_parent; }
VFXSceneNode* VFXSceneNode::get_parent_node() const { return parent_node; }

void VFXSceneNode::_unparent() { parent_node = nullptr; }

void VFXSceneNode::add_child(const Ref<VFXSceneNode>& p_child) {
    if (p_child.is_null() || p_child.ptr() == this) return;
    if (is_descendant_of(p_child)) return;
    if (p_child->parent_node != nullptr)
        p_child->parent_node->remove_child(p_child);
    p_child->parent_node = this;
    children.push_back(p_child);
}

void VFXSceneNode::remove_child(const Ref<VFXSceneNode>& p_child) {
    if (p_child.is_null()) return;
    for (auto it = children.begin(); it != children.end(); ++it) {
        if ((*it).ptr() == p_child.ptr()) {
            (*it)->_unparent();
            children.erase(it);
            return;
        }
    }
}

void VFXSceneNode::remove_child_by_id(int p_child_id) {
    for (auto it = children.begin(); it != children.end(); ++it) {
        if ((*it)->node_id == p_child_id) {
            (*it)->_unparent();
            children.erase(it);
            return;
        }
    }
}

void VFXSceneNode::clear_children() {
    for (auto& c : children) {
        if (c.is_valid()) c->_unparent();
    }
    children.clear();
}

int VFXSceneNode::get_child_count() const { return static_cast<int>(children.size()); }

Ref<VFXSceneNode> VFXSceneNode::get_child(int p_index) const {
    if (p_index < 0 || p_index >= static_cast<int>(children.size()))
        return Ref<VFXSceneNode>();
    return children[p_index];
}

Array VFXSceneNode::get_children() const {
    Array arr;
    arr.resize(children.size());
    for (size_t i = 0; i < children.size(); ++i)
        arr[i] = children[i];
    return arr;
}

bool VFXSceneNode::has_child(const Ref<VFXSceneNode>& p_child) const {
    if (p_child.is_null()) return false;
    for (const auto& c : children)
        if (c.ptr() == p_child.ptr()) return true;
    return false;
}

bool VFXSceneNode::is_ancestor_of(const Ref<VFXSceneNode>& p_node) const {
    if (p_node.is_null()) return false;
    return p_node->is_descendant_of(Ref<VFXSceneNode>(const_cast<VFXSceneNode*>(this)));
}

bool VFXSceneNode::is_descendant_of(const Ref<VFXSceneNode>& p_node) const {
    if (p_node.is_null()) return false;
    if (parent_node == p_node.ptr()) return true;
    if (parent_node != nullptr)
        return parent_node->is_descendant_of(p_node);
    return false;
}

Ref<VFXSceneNode> VFXSceneNode::_find_node_by_id_recursive(int p_id) const {
    if (node_id == p_id) return Ref<VFXSceneNode>(const_cast<VFXSceneNode*>(this));
    for (const auto& c : children) {
        if (c.is_null()) continue;
        Ref<VFXSceneNode> found = c->_find_node_by_id_recursive(p_id);
        if (found.is_valid()) return found;
    }
    return Ref<VFXSceneNode>();
}

Ref<VFXSceneNode> VFXSceneNode::find_node_by_id(int p_id) const {
    return _find_node_by_id_recursive(p_id);
}

void VFXSceneNode::_find_node_by_name_recursive(const String& p_name, Ref<VFXSceneNode>& r_result) const {
    if (!r_result.is_null()) return;
    if (node_name == p_name) {
        r_result = Ref<VFXSceneNode>(const_cast<VFXSceneNode*>(this));
        return;
    }
    for (const auto& c : children) {
        if (c.is_null()) continue;
        c->_find_node_by_name_recursive(p_name, r_result);
        if (!r_result.is_null()) return;
    }
}

Ref<VFXSceneNode> VFXSceneNode::find_node_by_name(const String& p_name) const {
    Ref<VFXSceneNode> result;
    _find_node_by_name_recursive(p_name, result);
    return result;
}

Ref<VFXSceneNode> VFXSceneNode::find_child_by_name(const String& p_name) const {
    for (const auto& c : children) {
        if (c.is_null()) continue;
        if (c->node_name == p_name) return c;
    }
    return Ref<VFXSceneNode>();
}

void VFXSceneNode::_get_all_descendants_recursive(Array& r_out) const {
    for (const auto& c : children) {
        if (c.is_null()) continue;
        r_out.push_back(c);
        c->_get_all_descendants_recursive(r_out);
    }
}

Array VFXSceneNode::get_all_descendants() const {
    Array arr;
    _get_all_descendants_recursive(arr);
    return arr;
}

void VFXSceneNode::set_local_transform(const Transform3D& p_transform) { local_transform = p_transform; }
Transform3D VFXSceneNode::get_local_transform() const { return local_transform; }
void VFXSceneNode::set_transform(const Transform3D& p_transform) { local_transform = p_transform; }
Transform3D VFXSceneNode::get_transform() const { return local_transform; }

void VFXSceneNode::set_local_position(const Vector3& p_pos) { local_transform.set_origin(p_pos); }
Vector3 VFXSceneNode::get_local_position() const { return local_transform.get_origin(); }

void VFXSceneNode::set_local_rotation(const Quaternion& p_rot) {
    Vector3 s = local_transform.get_basis().get_scale();
    Basis b(p_rot);
    b.scale(s);
    local_transform.set_basis(b);
}
Quaternion VFXSceneNode::get_local_rotation() const {
    return local_transform.get_basis().get_rotation_quaternion();
}

void VFXSceneNode::set_local_scale(const Vector3& p_scale) {
    Quaternion r = local_transform.get_basis().get_rotation_quaternion();
    local_transform.set_basis(Basis(r).scaled(p_scale));
}
Vector3 VFXSceneNode::get_local_scale() const {
    return local_transform.get_basis().get_scale();
}

void VFXSceneNode::set_visible(bool p_visible) { node_visible = p_visible; }
bool VFXSceneNode::get_visible() const { return node_visible; }

void VFXSceneNode::set_expanded(bool p_expanded) { node_expanded = p_expanded; }
bool VFXSceneNode::get_expanded() const { return node_expanded; }

void VFXSceneNode::set_selected(bool p_selected) { node_selected = p_selected; }
bool VFXSceneNode::get_selected() const { return node_selected; }

void VFXSceneNode::set_mesh(const Ref<VFXMesh>& p_mesh) { mesh = p_mesh; }
Ref<VFXMesh> VFXSceneNode::get_mesh() const { return mesh; }

void VFXSceneNode::set_skeleton(const Ref<VFXSkeleton>& p_skeleton) { skeleton = p_skeleton; }
Ref<VFXSkeleton> VFXSceneNode::get_skeleton() const { return skeleton; }

void VFXSceneNode::set_skin(const Ref<VFXSkin>& p_skin) { skin = p_skin; }
Ref<VFXSkin> VFXSceneNode::get_skin() const { return skin; }

void VFXSceneNode::set_animator(const Ref<VFXAnimator>& p_animator) { animator = p_animator; }
Ref<VFXAnimator> VFXSceneNode::get_animator() const { return animator; }

void VFXSceneNode::set_bone_parent_index(int p_idx) { bone_parent_idx = p_idx; }
int VFXSceneNode::get_bone_parent_index() const { return bone_parent_idx; }

bool VFXSceneNode::has_mesh() const { return mesh.is_valid(); }
bool VFXSceneNode::has_skeleton() const { return skeleton.is_valid(); }
bool VFXSceneNode::has_skin() const { return skin.is_valid(); }
bool VFXSceneNode::has_animator() const { return animator.is_valid(); }

Color VFXSceneNode::get_type_color() const {
    switch (node_type) {
        case NODE_MESH:     return Color(0.35f, 0.56f, 0.78f);
        case NODE_ARMATURE: return Color(0.78f, 0.56f, 0.35f);
        case NODE_BONE:     return Color(0.90f, 0.70f, 0.13f);
        case NODE_LIGHT:    return Color(0.90f, 0.90f, 0.20f);
        case NODE_CAMERA:   return Color(0.50f, 0.80f, 0.50f);
        case NODE_CURVE:    return Color(0.80f, 0.50f, 0.80f);
        default:            return Color(0.53f, 0.53f, 0.53f);
    }
}

String VFXSceneNode::get_type_icon_hint() const {
    switch (node_type) {
        case NODE_MESH:     return "mesh";
        case NODE_ARMATURE: return "armature";
        case NODE_BONE:     return "bone";
        case NODE_LIGHT:    return "light";
        case NODE_CAMERA:   return "camera";
        case NODE_CURVE:    return "curve";
        default:            return "empty";
    }
}



bool VFXSceneNode::is_visible() const {
    return node_visible;
}

Transform3D VFXSceneNode::get_global_transform() const {
    Transform3D global = local_transform;
    VFXSceneNode* p = parent_node;
    while (p != nullptr) {
        global = p->local_transform * global;
        p = p->parent_node;
    }
    return global;
}

PackedByteArray VFXSceneNode::serialize() const {
    PackedByteArray data;
    data.append('V'); data.append('F'); data.append('X'); data.append('N');
    data.append(1);

    auto append_int = [&](int v) {
        data.append((v >> 0) & 0xFF);
        data.append((v >> 8) & 0xFF);
        data.append((v >> 16) & 0xFF);
        data.append((v >> 24) & 0xFF);
    };
    auto append_float = [&](float v) {
        uint32_t u; memcpy(&u, &v, 4);
        data.append((u >> 0) & 0xFF);
        data.append((u >> 8) & 0xFF);
        data.append((u >> 16) & 0xFF);
        data.append((u >> 24) & 0xFF);
    };

    append_int(node_id);
    append_int(parent_node ? parent_node->node_id : -1);
    append_int(static_cast<int>(node_type));
    append_int(bone_parent_idx);

    CharString cs = node_name.utf8();
    int name_len = cs.length();
    append_int(name_len);

	for (int i = 0; i < name_len; ++i) data.append(cs.ptr()[i]);


    Basis b = local_transform.get_basis();
    Vector3 o = local_transform.get_origin();
    append_float(b[0][0]); append_float(b[0][1]); append_float(b[0][2]);
    append_float(b[1][0]); append_float(b[1][1]); append_float(b[1][2]);
    append_float(b[2][0]); append_float(b[2][1]); append_float(b[2][2]);
    append_float(o.x); append_float(o.y); append_float(o.z);

    data.append(node_visible ? 1 : 0);
    data.append(node_expanded ? 1 : 0);
    data.append(node_selected ? 1 : 0);

    data.append(has_mesh() ? 1 : 0);
    data.append(has_skeleton() ? 1 : 0);
    data.append(has_skin() ? 1 : 0);
    data.append(has_animator() ? 1 : 0);

    append_int(static_cast<int>(children.size()));
    for (const auto& c : children) {
        if (c.is_valid()) {
            PackedByteArray child_data = c->serialize();
            data.append_array(child_data);
        }
    }
    return data;
}

void VFXSceneNode::deserialize(const PackedByteArray& p_data) {
    clear_children();
}
