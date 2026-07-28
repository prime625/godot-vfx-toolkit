#include "vfx_skeleton.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

void VFXSkeleton::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_mixamo_skeleton"), &VFXSkeleton::create_mixamo_skeleton);
    ClassDB::bind_method(D_METHOD("clear"), &VFXSkeleton::clear);
    ClassDB::bind_method(D_METHOD("add_bone", "name", "parent_id"), &VFXSkeleton::add_bone, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("find_bone", "name"), &VFXSkeleton::find_bone);
    ClassDB::bind_method(D_METHOD("set_bone_parent", "bone_id", "parent_id"), &VFXSkeleton::set_bone_parent);
    ClassDB::bind_method(D_METHOD("set_bone_local_position", "bone_id", "pos"), &VFXSkeleton::set_bone_local_position);
    ClassDB::bind_method(D_METHOD("set_bone_local_rotation", "bone_id", "rot"), &VFXSkeleton::set_bone_local_rotation);
    ClassDB::bind_method(D_METHOD("set_bone_local_scale", "bone_id", "scale"), &VFXSkeleton::set_bone_local_scale);
    ClassDB::bind_method(D_METHOD("set_bone_bind_pose", "bone_id", "pose"), &VFXSkeleton::set_bone_bind_pose);
    ClassDB::bind_method(D_METHOD("get_bone_local_position", "bone_id"), &VFXSkeleton::get_bone_local_position);
    ClassDB::bind_method(D_METHOD("get_bone_local_rotation", "bone_id"), &VFXSkeleton::get_bone_local_rotation);
    ClassDB::bind_method(D_METHOD("get_bone_local_scale", "bone_id"), &VFXSkeleton::get_bone_local_scale);
    ClassDB::bind_method(D_METHOD("get_bone_bind_pose", "bone_id"), &VFXSkeleton::get_bone_bind_pose);
    ClassDB::bind_method(D_METHOD("get_bone_model_transform", "bone_id"), &VFXSkeleton::get_bone_model_transform);
    ClassDB::bind_method(D_METHOD("get_bone_name", "bone_id"), &VFXSkeleton::get_bone_name);
    ClassDB::bind_method(D_METHOD("get_bone_count"), &VFXSkeleton::get_bone_count);
    ClassDB::bind_method(D_METHOD("get_bone_parent", "bone_id"), &VFXSkeleton::get_bone_parent);
    ClassDB::bind_method(D_METHOD("get_bone_children", "bone_id"), &VFXSkeleton::get_bone_children);
    ClassDB::bind_method(D_METHOD("set_bone_pose", "bone_id", "pose"), &VFXSkeleton::set_bone_pose);
    ClassDB::bind_method(D_METHOD("reset_to_bind_pose"), &VFXSkeleton::reset_to_bind_pose);
    ClassDB::bind_method(D_METHOD("solve_ik_two_bone", "root_bone", "mid_bone", "tip_bone", "target", "pole", "twist"), &VFXSkeleton::solve_ik_two_bone, DEFVAL(0.0f));
    ClassDB::bind_method(D_METHOD("solve_ik_ccd", "tip_bone", "target", "iterations", "threshold"), &VFXSkeleton::solve_ik_ccd, DEFVAL(10), DEFVAL(0.001f));
    ClassDB::bind_method(D_METHOD("update_transforms"), &VFXSkeleton::update_transforms);
    ClassDB::bind_method(D_METHOD("get_skinning_matrices"), &VFXSkeleton::get_skinning_matrices);
    ClassDB::bind_method(D_METHOD("serialize"), &VFXSkeleton::serialize);
    ClassDB::bind_method(D_METHOD("deserialize", "data"), &VFXSkeleton::deserialize);
}

VFXSkeleton::VFXSkeleton() {}
VFXSkeleton::~VFXSkeleton() {}

void VFXSkeleton::clear() {
    bones.clear();
    name_to_index.clear();
    dirty = true;
}

int VFXSkeleton::add_bone(const String& name, int parent_id) {
    Bone b;
    b.id = bones.size();
    b.name = name;
    b.parent_id = parent_id;
    bones.push_back(b);
    name_to_index[name.utf8().get_data()] = b.id;
    dirty = true;
    return b.id;
}

int VFXSkeleton::find_bone(const String& name) const {
    auto it = name_to_index.find(name.utf8().get_data());
    if (it != name_to_index.end()) return it->second;
    return -1;
}

void VFXSkeleton::set_bone_parent(int bone_id, int parent_id) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].parent_id = parent_id;
        dirty = true;
    }
}

void VFXSkeleton::set_bone_local_position(int bone_id, const Vector3& pos) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].local_position = pos;
        dirty = true;
    }
}

void VFXSkeleton::set_bone_local_rotation(int bone_id, const Quaternion& rot) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].local_rotation = rot;
        dirty = true;
    }
}

void VFXSkeleton::set_bone_local_scale(int bone_id, const Vector3& scale) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].local_scale = scale;
        dirty = true;
    }
}

void VFXSkeleton::set_bone_bind_pose(int bone_id, const Transform3D& pose) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].bind_pose = pose;
        bones[bone_id].inverse_bind_pose = pose.affine_inverse();
        dirty = true;
    }
}

Vector3 VFXSkeleton::get_bone_local_position(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].local_position;
    return Vector3();
}

Quaternion VFXSkeleton::get_bone_local_rotation(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].local_rotation;
    return Quaternion();
}

Vector3 VFXSkeleton::get_bone_local_scale(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].local_scale;
    return Vector3(1,1,1);
}

Transform3D VFXSkeleton::get_bone_bind_pose(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].bind_pose;
    return Transform3D();
}

Transform3D VFXSkeleton::get_bone_model_transform(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].model_transform;
    return Transform3D();
}

String VFXSkeleton::get_bone_name(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].name;
    return "";
}

int VFXSkeleton::get_bone_count() const { return bones.size(); }

int VFXSkeleton::get_bone_parent(int bone_id) const {
    if (bone_id >= 0 && bone_id < (int)bones.size()) return bones[bone_id].parent_id;
    return -1;
}

PackedInt32Array VFXSkeleton::get_bone_children(int bone_id) const {
    PackedInt32Array children;
    for (int i = 0; i < (int)bones.size(); i++) {
        if (bones[i].parent_id == bone_id) children.push_back(i);
    }
    return children;
}

void VFXSkeleton::set_bone_pose(int bone_id, const Transform3D& pose) {
    if (bone_id >= 0 && bone_id < (int)bones.size()) {
        bones[bone_id].local_position = pose.get_origin();
        bones[bone_id].local_rotation = pose.get_basis().get_rotation_quaternion();
        bones[bone_id].local_scale = pose.get_basis().get_scale();
        dirty = true;
    }
}

void VFXSkeleton::reset_to_bind_pose() {
    for (auto& b : bones) {
        b.local_position = b.bind_pose.get_origin();
        b.local_rotation = b.bind_pose.get_basis().get_rotation_quaternion();
        b.local_scale = b.bind_pose.get_basis().get_scale();
    }
    dirty = true;
    update_transforms();
}

void VFXSkeleton::_update_transforms_recursive(int bone_idx, const Transform3D& parent_transform) {
    if (bone_idx < 0 || bone_idx >= (int)bones.size()) return;
    
    Bone& b = bones[bone_idx];
    
    // Build local transform properly: Basis from rotation, then apply scale
    Basis rot_basis(b.local_rotation);
    rot_basis.scale(b.local_scale);
    
    Transform3D local;
    local.set_basis(rot_basis);
    local.set_origin(b.local_position);
    
    b.model_transform = parent_transform * local;
    
    // Update children
    for (int i = 0; i < (int)bones.size(); i++) {
        if (bones[i].parent_id == bone_idx) {
            _update_transforms_recursive(i, b.model_transform);
        }
    }
}

void VFXSkeleton::update_transforms() {
    // Find root bones and update from there
    for (int i = 0; i < (int)bones.size(); i++) {
        if (bones[i].parent_id < 0) {
            _update_transforms_recursive(i, Transform3D());
        }
    }
    dirty = false;
}

// === IK: Two-Bone (Analytic) ===
void VFXSkeleton::solve_ik_two_bone(int root_bone, int mid_bone, int tip_bone, const Vector3& target, const Vector3& pole, float twist) {
    if (root_bone < 0 || mid_bone < 0 || tip_bone < 0) return;
    if (root_bone >= (int)bones.size() || mid_bone >= (int)bones.size() || tip_bone >= (int)bones.size()) return;
    
    Vector3 root_pos = bones[root_bone].model_transform.get_origin();
    Vector3 mid_pos = bones[mid_bone].model_transform.get_origin();
    Vector3 tip_pos = bones[tip_bone].model_transform.get_origin();
    
    float len1 = (mid_pos - root_pos).length();
    float len2 = (tip_pos - mid_pos).length();
    float dist = (target - root_pos).length();
    
    // Clamp to reach
    if (dist > len1 + len2 - 0.001f) dist = len1 + len2 - 0.001f;
    if (dist < fabs(len1 - len2) + 0.001f) dist = fabs(len1 - len2) + 0.001f;
    
    // Law of cosines for elbow angle
    float cos_angle = (dist * dist + len1 * len1 - len2 * len2) / (2.0f * dist * len1);
    cos_angle = vfx::clampf(cos_angle, -1.0f, 1.0f);
    float angle = acos(cos_angle);
    
    UtilityFunctions::print("IK two-bone: angle = ", angle);
    dirty = true;
}

// === IK: CCD (Cyclic Coordinate Descent) ===
void VFXSkeleton::solve_ik_ccd(int tip_bone, const Vector3& target, int iterations, float threshold) {
    if (tip_bone < 0 || tip_bone >= (int)bones.size()) return;
    
    for (int iter = 0; iter < iterations; iter++) {
        int current = bones[tip_bone].parent_id;
        while (current >= 0) {
            Vector3 tip_world = bones[tip_bone].model_transform.get_origin();
            Vector3 joint_world = bones[current].model_transform.get_origin();
            
            Vector3 to_tip = (tip_world - joint_world).normalized();
            Vector3 to_target = (target - joint_world).normalized();
            
            if (to_tip.length_squared() < 0.0001f || to_target.length_squared() < 0.0001f) {
                current = bones[current].parent_id;
                continue;
            }
            
            float dot = vfx::clampf(to_tip.dot(to_target), -1.0f, 1.0f);
            float angle = acos(dot);
            if (angle < 0.0001f) {
                current = bones[current].parent_id;
                continue;
            }
            
            Vector3 axis = to_tip.cross(to_target).normalized();
            if (axis.length_squared() < 0.0001f) {
                current = bones[current].parent_id;
                continue;
            }
            
            Quaternion rot(axis, angle);
            bones[current].local_rotation = rot * bones[current].local_rotation;
            update_transforms();
            
            if ((bones[tip_bone].model_transform.get_origin() - target).length() < threshold) {
                return;
            }
            
            current = bones[current].parent_id;
        }
    }
}

PackedVector3Array VFXSkeleton::get_skinning_matrices() const {
    PackedVector3Array matrices;
    matrices.resize(bones.size() * 3);
    
    for (int i = 0; i < (int)bones.size(); i++) {
        Transform3D skin = bones[i].model_transform * bones[i].inverse_bind_pose;
        Basis b = skin.get_basis();
        
        int base = i * 3;
        matrices[base + 0] = b.get_column(0);
        matrices[base + 1] = b.get_column(1);
        matrices[base + 2] = b.get_column(2);
    }
    return matrices;
}

void VFXSkeleton::create_mixamo_skeleton() {
    clear();
    
    int hips = add_bone("mixamorig_Hips", -1);
    
    int spine = add_bone("mixamorig_Spine", hips);
    int spine1 = add_bone("mixamorig_Spine1", spine);
    int spine2 = add_bone("mixamorig_Spine2", spine1);
    
    int neck = add_bone("mixamorig_Neck", spine2);
    int head = add_bone("mixamorig_Head", neck);
    
    int l_shoulder = add_bone("mixamorig_LeftShoulder", spine2);
    int l_arm = add_bone("mixamorig_LeftArm", l_shoulder);
    int l_forearm = add_bone("mixamorig_LeftForeArm", l_arm);
    int l_hand = add_bone("mixamorig_LeftHand", l_forearm);
    
    int r_shoulder = add_bone("mixamorig_RightShoulder", spine2);
    int r_arm = add_bone("mixamorig_RightArm", r_shoulder);
    int r_forearm = add_bone("mixamorig_RightForeArm", r_arm);
    int r_hand = add_bone("mixamorig_RightHand", r_forearm);
    
    int l_upleg = add_bone("mixamorig_LeftUpLeg", hips);
    int l_leg = add_bone("mixamorig_LeftLeg", l_upleg);
    int l_foot = add_bone("mixamorig_LeftFoot", l_leg);
    int l_toe = add_bone("mixamorig_LeftToeBase", l_foot);
    
    int r_upleg = add_bone("mixamorig_RightUpLeg", hips);
    int r_leg = add_bone("mixamorig_RightLeg", r_upleg);
    int r_foot = add_bone("mixamorig_RightFoot", r_leg);
    int r_toe = add_bone("mixamorig_RightToeBase", r_foot);
    
    // Set default bind poses
    set_bone_local_position(spine, Vector3(0, 1.0f, 0));
    set_bone_local_position(spine1, Vector3(0, 0.15f, 0));
    set_bone_local_position(spine2, Vector3(0, 0.15f, 0));
    set_bone_local_position(neck, Vector3(0, 0.15f, 0));
    set_bone_local_position(head, Vector3(0, 0.1f, 0));
    
    set_bone_local_position(l_shoulder, Vector3(0.15f, 0.05f, 0));
    set_bone_local_position(l_arm, Vector3(0.1f, 0, 0));
    set_bone_local_position(l_forearm, Vector3(0.25f, 0, 0));
    set_bone_local_position(l_hand, Vector3(0.25f, 0, 0));
    
    set_bone_local_position(r_shoulder, Vector3(-0.15f, 0.05f, 0));
    set_bone_local_position(r_arm, Vector3(-0.1f, 0, 0));
    set_bone_local_position(r_forearm, Vector3(-0.25f, 0, 0));
    set_bone_local_position(r_hand, Vector3(-0.25f, 0, 0));
    
    set_bone_local_position(l_upleg, Vector3(0.1f, 0, 0));
    set_bone_local_position(l_leg, Vector3(0, -0.45f, 0));
    set_bone_local_position(l_foot, Vector3(0, -0.45f, 0));
    set_bone_local_position(l_toe, Vector3(0, 0, 0.15f));
    
    set_bone_local_position(r_upleg, Vector3(-0.1f, 0, 0));
    set_bone_local_position(r_leg, Vector3(0, -0.45f, 0));
    set_bone_local_position(r_foot, Vector3(0, -0.45f, 0));
    set_bone_local_position(r_toe, Vector3(0, 0, 0.15f));
    
    update_transforms();
    for (auto& b : bones) {
        b.bind_pose = b.model_transform;
        b.inverse_bind_pose = b.model_transform.affine_inverse();
    }
}

PackedByteArray VFXSkeleton::serialize() const {
    PackedByteArray data;
    data.append(0x56); data.append(0x46); data.append(0x58); data.append(0x53);
    return data;
}

void VFXSkeleton::deserialize(const PackedByteArray& data) {
    clear();
}
