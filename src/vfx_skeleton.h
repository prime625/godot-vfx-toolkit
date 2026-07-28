#ifndef VFX_SKELETON_H
#define VFX_SKELETON_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <vector>
#include <string>
#include "vfx_math.h"

using namespace godot;

// Mixamo-standard humanoid bone names
namespace vfx {
    static const char* MIXAMO_BONES[] = {
        "Hips",
        "Spine", "Spine1", "Spine2",
        "Neck", "Head",
        "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
        "RightShoulder", "RightArm", "RightForeArm", "RightHand",
        "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase",
        "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase",
        nullptr
    };
}

struct Bone {
    int id = -1;
    String name;
    int parent_id = -1;

    // Local transform (relative to parent)
    Vector3 local_position;
    Quaternion local_rotation;
    Vector3 local_scale = Vector3(1, 1, 1);

    // Bind pose (model space, for skinning)
    Transform3D bind_pose;
    Transform3D inverse_bind_pose;

    // Runtime transform
    Transform3D model_transform;  // computed each frame

    // IK targets (optional)
    bool has_ik_target = false;
    Vector3 ik_target_position;

    bool is_valid() const { return id >= 0; }
};

class VFXSkeleton : public RefCounted {
    GDCLASS(VFXSkeleton, RefCounted)

private:
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> name_to_index;

    bool dirty = true;

    void _update_transforms_recursive(int bone_idx, const Transform3D& parent_transform);
    void _build_name_map();

protected:
    static void _bind_methods();

public:
    VFXSkeleton();
    ~VFXSkeleton();

    // === SETUP ===
    void create_mixamo_skeleton();
    void clear();

    int add_bone(const String& name, int parent_id = -1);
    int find_bone(const String& name) const;

    void set_bone_parent(int bone_id, int parent_id);
    void set_bone_local_position(int bone_id, const Vector3& pos);
    void set_bone_local_rotation(int bone_id, const Quaternion& rot);
    void set_bone_local_scale(int bone_id, const Vector3& scale);
    void set_bone_bind_pose(int bone_id, const Transform3D& pose);

    Vector3 get_bone_local_position(int bone_id) const;
    Quaternion get_bone_local_rotation(int bone_id) const;
    Vector3 get_bone_local_scale(int bone_id) const;
    Transform3D get_bone_bind_pose(int bone_id) const;
    Transform3D get_bone_model_transform(int bone_id) const;

    String get_bone_name(int bone_id) const;
    int get_bone_count() const;
    int get_bone_parent(int bone_id) const;
    PackedInt32Array get_bone_children(int bone_id) const;

    // === POSE ===
    void set_bone_pose(int bone_id, const Transform3D& pose);
    void reset_to_bind_pose();

    // === IK ===
    void solve_ik_two_bone(int root_bone, int mid_bone, int tip_bone, const Vector3& target, const Vector3& pole, float twist = 0.0f);
    void solve_ik_ccd(int tip_bone, const Vector3& target, int iterations = 10, float threshold = 0.001f);

    // === UPDATE ===
    void update_transforms();

    // === SKINNING MATRICES ===
    // Returns array of SkinMatrix (Transform3D) for GPU skinning
    PackedVector3Array get_skinning_matrices() const;  // Flattened 3x4 matrices

    // === SERIALIZATION ===
    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);

    // === IMPORT ===
    void from_godot_skeleton(const Object* skeleton);
};

#endif
