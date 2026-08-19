#include "vfx_scene.h"
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

    Ref<VFXSceneNode> p = parent;
    if (!p.is_valid() && root.is_valid()) p = root;
    if (p.is_valid()) {
        p->add_child(node);
    } else {
        // No root yet, this becomes root
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

bool VFXScene::import_model(const String& filepath, const Ref<VFXSceneNode>& parent) {
    // NOTE: on Android copy files to user:// first, then load from there
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

    // Direct mesh resource (e.g. .obj imported as Mesh)
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
        // Skeleton reference on MeshInstance3D
        NodePath skel_path = mi->get_skeleton();
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

void VFXScene::_flatten_recursive(const Ref<VFXSceneNode>& node, Array& out) const {
    if (node.is_null()) return;
    out.append(node);
    for (int i = 0; i < node->get_child_count(); i++) {
        _flatten_recursive(node->get_child(i), out);
    }
}

int VFXScene::get_unique_id() { return node_counter++; }

PackedByteArray VFXScene::serialize() const {
    PackedByteArray data;
    data.append(0x56); data.append(0x53); data.append(0x43); // VSC
    return data;
}
void VFXScene::deserialize(const PackedByteArray& data) {}
