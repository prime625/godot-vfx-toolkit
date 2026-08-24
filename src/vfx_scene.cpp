#include "vfx_scene.h"
#include "vfx_glb_importer.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

void VFXScene::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_default_root"), &VFXScene::create_default_root);
    ClassDB::bind_method(D_METHOD("get_root"), &VFXScene::get_root);
    ClassDB::bind_method(D_METHOD("create_node", "name", "type", "parent"), &VFXScene::create_node, DEFVAL(Ref<VFXSceneNode>()));
    ClassDB::bind_method(D_METHOD("delete_node", "node"), &VFXScene::delete_node);
    ClassDB::bind_method(D_METHOD("clear"), &VFXScene::clear);
    ClassDB::bind_method(D_METHOD("import_model", "filepath", "parent"), &VFXScene::import_model, DEFVAL(Ref<VFXSceneNode>()));
    ClassDB::bind_method(D_METHOD("import_glb_model", "filepath", "parent", "transform"), &VFXScene::import_glb_model, DEFVAL(Ref<VFXSceneNode>()), DEFVAL(Transform3D()));
    ClassDB::bind_method(D_METHOD("merge_scene", "other", "parent", "transform"), &VFXScene::merge_scene, DEFVAL(Ref<VFXSceneNode>()), DEFVAL(Transform3D()));
    ClassDB::bind_method(D_METHOD("ensure_unique_name", "name"), &VFXScene::ensure_unique_name);
    ClassDB::bind_method(D_METHOD("get_node_count"), &VFXScene::get_node_count);
    ClassDB::bind_method(D_METHOD("find_node_by_name", "name"), &VFXScene::find_node_by_name);
    ClassDB::bind_method(D_METHOD("get_all_mesh_nodes"), &VFXScene::get_all_mesh_nodes);
    ClassDB::bind_method(D_METHOD("flatten_tree"), &VFXScene::flatten_tree);
    ClassDB::bind_method(D_METHOD("get_unique_id"), &VFXScene::get_unique_id);
}

VFXScene::VFXScene() {}
VFXScene::~VFXScene() {}

void VFXScene::create_default_root() {
    if (root.is_valid()) return;
    root.instantiate();
    root->set_node_name("Scene");
    root->set_node_type(VFXSceneNode::NODE_EMPTY);
}

Ref<VFXSceneNode> VFXScene::get_root() const { return root; }

Ref<VFXSceneNode> VFXScene::create_node(const String& name, int type, const Ref<VFXSceneNode>& parent) {
    Ref<VFXSceneNode> node;
    node.instantiate();
    node->set_node_name(name.is_empty() ? "Node_" + String::num_int64(node_counter++) : name);
    node->set_node_type(type);
    node->set_node_id(get_unique_id());

    Ref<VFXSceneNode> p = parent;
    if (!p.is_valid() && root.is_valid()) p = root;
    if (p.is_valid()) {
        p->add_child(node);
    } else {
        root = node;
    }
    return node;
}

void VFXScene::delete_node(const Ref<VFXSceneNode>& node) {
    if (node.is_null()) return;
    if (node == root) {
        root = Ref<VFXSceneNode>();
        return;
    }
    VFXSceneNode* p = node->get_parent_node();
    if (p) {
        Ref<VFXSceneNode> pr(p);
        pr->remove_child(node);
    }
}

void VFXScene::clear() {
    root = Ref<VFXSceneNode>();
    node_counter = 0;
}

// ============================================================================
// Legacy Godot ResourceLoader import
// ============================================================================
bool VFXScene::import_model(const String& filepath, const Ref<VFXSceneNode>& parent) {
    Ref<Resource> res = ResourceLoader::get_singleton()->load(filepath);
    if (res.is_null()) {
        UtilityFunctions::print("Import failed: cannot load ", filepath);
        return false;
    }

    Ref<PackedScene> packed = res;
    if (packed.is_valid()) {
        Node* imported = packed->instantiate();
        if (!imported) return false;
        _import_godot_node(imported, parent.is_valid() ? parent.ptr() : root.ptr());
        memdelete(imported);
        return true;
    }

    Ref<Mesh> mesh = res;
    if (mesh.is_valid()) {
        Ref<VFXSceneNode> node = create_node(filepath.get_file(), VFXSceneNode::NODE_MESH, parent);
        Ref<VFXMesh> vmesh;
        vmesh.instantiate();
        vmesh->from_godot_mesh(mesh);
        node->set_mesh(vmesh);
        return true;
    }

    UtilityFunctions::print("Import failed: unsupported resource type for ", filepath);
    return false;
}

void VFXScene::_import_godot_node(Node* godot_node, VFXSceneNode* parent) {
    if (!godot_node) return;

    String name = godot_node->get_name();
    Ref<VFXSceneNode> vfx_node;

    MeshInstance3D* mi = Object::cast_to<MeshInstance3D>(godot_node);
    Skeleton3D* skel = Object::cast_to<Skeleton3D>(godot_node);
    Node3D* node3d = Object::cast_to<Node3D>(godot_node);

    if (mi) {
        vfx_node = create_node(name, VFXSceneNode::NODE_MESH, Ref<VFXSceneNode>(parent));
        Ref<Mesh> gmesh = mi->get_mesh();
        if (gmesh.is_valid()) {
            Ref<VFXMesh> vmesh;
            vmesh.instantiate();
            vmesh->from_godot_mesh(gmesh);
            vfx_node->set_mesh(vmesh);
        }
        Variant skel_var = mi->get("skeleton");
        if (skel_var.get_type() == Variant::NODE_PATH) {
            NodePath skel_path = skel_var;
            if (!skel_path.is_empty()) {
                Skeleton3D* skel_node = Object::cast_to<Skeleton3D>(mi->get_node_or_null(skel_path));
                if (skel_node) {
                    Ref<VFXSkeleton> vskel;
                    vskel.instantiate();
                    vskel->from_godot_skeleton(skel_node);
                    vfx_node->set_skeleton(vskel);
                    if (vfx_node->get_mesh().is_valid()) {
                        Ref<VFXSkin> skin;
                        skin.instantiate();
                        skin->set_mesh(vfx_node->get_mesh());
                        skin->set_skeleton(vskel);
                        skin->auto_weight_from_bones(4);
                        vfx_node->set_skin(skin);
                    }
                }
            }
        }
    } else if (skel) {
        vfx_node = create_node(name, VFXSceneNode::NODE_ARMATURE, Ref<VFXSceneNode>(parent));
        Ref<VFXSkeleton> vskel;
        vskel.instantiate();
        vskel->from_godot_skeleton(skel);
        vfx_node->set_skeleton(vskel);
    } else if (node3d) {
        vfx_node = create_node(name, VFXSceneNode::NODE_EMPTY, Ref<VFXSceneNode>(parent));
    } else {
        vfx_node = create_node(name, VFXSceneNode::NODE_EMPTY, Ref<VFXSceneNode>(parent));
    }

    if (node3d && vfx_node.is_valid()) {
        vfx_node->set_transform(node3d->get_transform());
    }

    for (int i = 0; i < godot_node->get_child_count(); i++) {
        _import_godot_node(godot_node->get_child(i), vfx_node.ptr());
    }
}

// ============================================================================
// NEW: Custom GLB import
// ============================================================================
bool VFXScene::import_glb_model(const String& filepath,
                                const Ref<VFXSceneNode>& parent,
                                const Transform3D& transform) {
    if (root.is_null()) create_default_root();

    Ref<VFXGLBImporter> importer;
    importer.instantiate();
    Dictionary result = importer->import_glb(filepath);

    if (!result.get(VFXGLBImporter::KEY_SUCCESS, false)) {
        UtilityFunctions::print("GLB import failed: ", result.get(VFXGLBImporter::KEY_ERROR, "unknown error"));
        return false;
    }

    Ref<VFXScene> imported_scene = result[VFXGLBImporter::KEY_SCENE];
    if (!imported_scene.is_valid() || imported_scene->get_root().is_null()) {
        UtilityFunctions::print("GLB import returned no scene");
        return false;
    }

    return merge_scene(imported_scene, parent, transform);
}

// ============================================================================
// NEW: Merge another VFXScene into this one
// ============================================================================
bool VFXScene::merge_scene(const Ref<VFXScene>& other,
                           const Ref<VFXSceneNode>& parent,
                           const Transform3D& transform) {
    if (other.is_null() || other->get_root().is_null()) return false;
    if (root.is_null()) create_default_root();

    Ref<VFXSceneNode> target = parent.is_valid() ? parent : root;

    Array other_children = other->get_root()->get_children();
    for (int i = 0; i < other_children.size(); i++) {
        Ref<VFXSceneNode> node = other_children[i];
        if (node.is_null()) continue;

        Ref<VFXSceneNode> cloned = _clone_node_recursive(node);
        if (cloned.is_null()) continue;

        if (transform != Transform3D()) {
            cloned->set_local_transform(transform * cloned->get_local_transform());
        }

        String unique_name = ensure_unique_name(cloned->get_node_name());
        cloned->set_node_name(unique_name);
        _reassign_ids_recursive(cloned);

        target->add_child(cloned);
    }

    return true;
}

// ============================================================================
// NEW: Deep-clone a node subtree
// ============================================================================
Ref<VFXSceneNode> VFXScene::_clone_node_recursive(const Ref<VFXSceneNode>& source) const {
    if (source.is_null()) return Ref<VFXSceneNode>();

    Ref<VFXSceneNode> clone;
    clone.instantiate();
    clone->set_node_name(source->get_node_name());
    clone->set_node_type(source->get_node_type());
    clone->set_local_transform(source->get_local_transform());
    clone->set_visible(source->get_visible());
    clone->set_expanded(source->get_expanded());
    clone->set_selected(source->get_selected());

    if (source->has_mesh())      clone->set_mesh(source->get_mesh());
    if (source->has_skeleton())  clone->set_skeleton(source->get_skeleton());
    if (source->has_skin())      clone->set_skin(source->get_skin());
    if (source->has_animator())  clone->set_animator(source->get_animator());

    Array children = source->get_children();
    for (int i = 0; i < children.size(); i++) {
        Ref<VFXSceneNode> child = children[i];
        Ref<VFXSceneNode> child_clone = _clone_node_recursive(child);
        if (child_clone.is_valid()) {
            clone->add_child(child_clone);
        }
    }

    return clone;
}

void VFXScene::_reassign_ids_recursive(const Ref<VFXSceneNode>& node) {
    if (node.is_null()) return;
    node->set_node_id(get_unique_id());
    Array children = node->get_children();
    for (int i = 0; i < children.size(); i++) {
        Ref<VFXSceneNode> child = children[i];
        _reassign_ids_recursive(child);
    }
}

// ============================================================================
// FIXED: Returns String instead of using non-const reference
// ============================================================================
String VFXScene::ensure_unique_name(const String& name) const {
    if (root.is_null()) return name;
    if (!_name_exists_recursive(root, name)) return name;

    String base = name;
    int start_num = 1;

    int dot_pos = base.rfind(".");
    if (dot_pos > 0) {
        String suffix = base.substr(dot_pos + 1);
        bool all_digits = true;
        for (int i = 0; i < suffix.length(); i++) {
            char32_t c = suffix[i];
            if (c < '0' || c > '9') { all_digits = false; break; }
        }
        if (all_digits && suffix.length() > 0) {
            base = base.substr(0, dot_pos);
            start_num = suffix.to_int() + 1;
        }
    }

    for (int n = start_num; n < 9999; n++) {
        String candidate = base + "." + String::num_int64(n).pad_zeros(3);
        if (!_name_exists_recursive(root, candidate)) return candidate;
    }

    return name + "_dup";
}

bool VFXScene::_name_exists_recursive(const Ref<VFXSceneNode>& node, const String& name) const {
    if (node.is_null()) return false;
    if (node->get_node_name() == name) return true;
    Array children = node->get_children();
    for (int i = 0; i < children.size(); i++) {
        Ref<VFXSceneNode> child = children[i];
        if (_name_exists_recursive(child, name)) return true;
    }
    return false;
}

int VFXScene::get_node_count() const {
    if (root.is_null()) return 0;
    int count = 1;
    Array all = root->get_all_descendants();
    count += all.size();
    return count;
}

Ref<VFXSceneNode> VFXScene::find_node_by_name(const String& name) const {
    if (root.is_valid()) {
        if (root->get_node_name() == name) return root;
        return root->find_child_by_name(name);
    }
    return Ref<VFXSceneNode>();
}

Array VFXScene::get_all_mesh_nodes() const {
    Array out;
    Array all = flatten_tree();
    for (int i = 0; i < all.size(); i++) {
        Ref<VFXSceneNode> n = all[i];
        if (n.is_valid() && n->get_node_type() == VFXSceneNode::NODE_MESH) out.append(n);
    }
    return out;
}

Array VFXScene::flatten_tree() const {
    Array out;
    if (root.is_valid()) {
        out.append(root);
        out.append_array(root->get_all_descendants());
    }
    return out;
}

int VFXScene::get_unique_id() { return node_counter++; }

PackedByteArray VFXScene::serialize() const {
    PackedByteArray data;
    data.append(0x56); data.append(0x53); data.append(0x43);
    return data;
}

void VFXScene::deserialize(const PackedByteArray& data) {}
