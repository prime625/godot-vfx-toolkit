#include "vfx_animator.h"
#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <cmath>

using namespace godot;

void AnimationCurve::sort_keys() {
    std::sort(keys.begin(), keys.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.time < b.time;
    });
}

float AnimationCurve::sample_scalar(float time) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1) return keys[0].value;

    size_t i = 0;
    for (; i < keys.size() - 1; i++) {
        if (time >= keys[i].time && time <= keys[i+1].time) break;
    }
    if (i >= keys.size() - 1) return keys.back().value;

    const Keyframe& k0 = keys[i];
    const Keyframe& k1 = keys[i+1];

    if (k1.time - k0.time < 0.0001f) return k0.value;
    float t = (time - k0.time) / (k1.time - k0.time);

    if (k0.interp == VFXAnimator::INTERP_STEP) return k0.value;
    if (k0.interp == VFXAnimator::INTERP_BEZIER) {
        float t2 = t * t;
        float t3 = t2 * t;
        float omt = 1.0f - t;
        float omt2 = omt * omt;
        float omt3 = omt2 * omt;
        return omt3 * k0.value + 3.0f * omt2 * t * (k0.value + k0.out_tangent) +
               3.0f * omt * t2 * (k1.value + k1.in_tangent) + t3 * k1.value;
    }
    return k0.value + (k1.value - k0.value) * t;
}

Vector3 AnimationCurve::sample_vector(float time) const {
    if (keys.empty()) return Vector3();
    if (keys.size() == 1) return keys[0].vec_value;

    size_t i = 0;
    for (; i < keys.size() - 1; i++) {
        if (time >= keys[i].time && time <= keys[i+1].time) break;
    }
    if (i >= keys.size() - 1) return keys.back().vec_value;

    const Keyframe& k0 = keys[i];
    const Keyframe& k1 = keys[i+1];
    float t = (k1.time - k0.time < 0.0001f) ? 0.0f : (time - k0.time) / (k1.time - k0.time);

    if (k0.interp == VFXAnimator::INTERPThis custom 3D character pipeline is a fantastic approach for handling character creation directly on an Android device without leaving your engine. Building this as a C++ GDExtension guarantees the performance needed for complex calculations like Mixamo weight painting and IK solvers, which is especially critical when developing a MOBA[cite: 1]. Having this dedicated editor will make rigging characters like Ignis with her sword and shield, or Lynx with her hammer and shield, incredibly streamlined[cite: 1]. 

I have identified the issues in the provided code and fixed them to ensure your standalone Android app pipeline compiles correctly. Here are the problems that were causing the errors:

*   **Truncated File:** `vfx_skeleton.cpp` was cut off halfway through the `solve_ik_ccd` method, missing the closing brackets and the definitions for `create_mixamo_skeleton`, `get_skinning_matrices`, `serialize`, and `deserialize`.
*   **Missing Header:** You were entirely missing the `vfx_skeleton.h` file.
*   **Duplicate Header Content:** The `vfx_editor_node.h` file you shared contained the exact same code as `vfx_animator.h`. I have rewritten it to correctly declare your `VFXEditorNode` (inheriting from `Node3D`).
*   **Syntax Errors in Animator:** `vfx_animator.cpp` had improper newline escape characters (`\n`) in the include headers and a missing closing parenthesis in the `seek()` method (`fmin(fmax(...`).

I have provided the full, updated scripts below so you can just paste them into your project[cite: 1].

### 1. `vfx_skeleton.h` (New/Missing File)
```cpp
#ifndef VFX_SKELETON_H
#define VFX_SKELETON_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <vector>
#include <unordered_map>

using namespace godot;

struct Bone {
    int id = -1;
    String name;
    int parent_id = -1;
    
    Vector3 local_position;
    Quaternion local_rotation;
    Vector3 local_scale = Vector3(1, 1, 1);
    
    Transform3D bind_pose;
    Transform3D inverse_bind_pose;
    Transform3D model_transform;
};

class VFXSkeleton : public RefCounted {
    GDCLASS(VFXSkeleton, RefCounted)

private:
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> name_to_index;
    bool dirty = true;

    void _update_transforms_recursive(int bone_idx, const Transform3D& parent_transform);

protected:
    static void _bind_methods();

public:
    VFXSkeleton();
    ~VFXSkeleton();

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
    
    void set_bone_pose(int bone_id, const Transform3D& pose);
    void reset_to_bind_pose();
    
    void update_transforms();
    void solve_ik_two_bone(int root_bone, int mid_bone, int tip_bone, const Vector3& target, const Vector3& pole, float twist = 0.0f);
    void solve_ik_ccd(int tip_bone, const Vector3& target, int iterations = 10, float threshold = 0.001f);
    
    void create_mixamo_skeleton();
    PackedFloat32Array get_skinning_matrices() const;
    
    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);
};

#endif