#ifndef VFX_ANIMATOR_H
#define VFX_ANIMATOR_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>
#include <string>

using namespace godot;

// Forward declaration for symmetry
class VFXSkeleton;

struct Keyframe {
    float time = 0.0f;
    float value = 0.0f;
    Vector3 vec_value;
    Quaternion quat_value;
    float in_tangent = 0.0f;
    float out_tangent = 0.0f;
    Vector3 vec_in_tangent;
    Vector3 vec_out_tangent;
    int interp = 0;
    int tangent_mode = 0; // 0=free, 1=aligned, 2=mirrored, 3=auto
};

struct AnimationCurve {
    String name;
    int bone_id = -1;           // >= 0: targets a skeleton bone
    int node_id = -1;           // >= 0: targets a VFXSceneNode (custom node)  [NEW]
    bool is_rotation = false;
    bool is_scale = false;
    bool is_scalar = false;
    std::vector<Keyframe> keys;

    void sort_keys();
    float sample_scalar(float time) const;
    Vector3 sample_vector(float time) const;
    Quaternion sample_quaternion(float time) const;
    void auto_compute_tangents();
};

struct AnimationClip {
    String name;
    float duration = 0.0f;
    float fps = 30.0f;
    bool loop = true;
    int loop_mode = 0; // 0=loop, 1=clamp, 2=pingpong
    std::vector<AnimationCurve> curves;
};

class VFXAnimator : public RefCounted {
    GDCLASS(VFXAnimator, RefCounted)

public:
    enum InterpolationType {
        INTERP_LINEAR = 0,
        INTERP_BEZIER = 1,
        INTERP_STEP = 2
    };

    enum TangentMode {
        TANGENT_FREE = 0,
        TANGENT_ALIGNED = 1,
        TANGENT_MIRRORED = 2,
        TANGENT_AUTO = 3
    };

    enum LoopMode {
        LOOP_LOOP = 0,
        LOOP_CLAMP = 1,
        LOOP_PING_PONG = 2
    };

    // What a curve drives (used for coloring & application)
    enum CurveKind {
        CURVE_POSITION = 0,
        CURVE_ROTATION = 1,
        CURVE_SCALE = 2,
        CURVE_SCALAR = 3
    };

private:
    std::vector<AnimationClip> clips;
    int current_clip = -1;
    float current_time = 0.0f;
    bool is_playing = false;
    int selected_curve = -1;
    int selected_key = -1;

protected:
    static void _bind_methods();

public:
    VFXAnimator();
    ~VFXAnimator();

    int create_clip(const String& name, float duration, float fps);
    void delete_clip(int idx);
    int get_clip_count() const;
    String get_clip_name(int idx) const;
    void set_clip_name(int idx, const String& name);
    float get_clip_duration(int idx) const;
    void set_clip_duration(int idx, float duration);
    float get_clip_fps(int idx) const;
    void set_clip_fps(int idx, float fps);
    void set_clip_loop(int idx, bool loop);
    bool get_clip_loop(int idx) const;
    void set_clip_loop_mode(int idx, int mode);
    int get_clip_loop_mode(int idx) const;

    int add_curve(int clip_idx, const String& name, int bone_id, bool is_rotation, bool is_scale);
    int add_node_curve(int clip_idx, const String& name, int node_id, int kind);
    void delete_curve(int clip_idx, int curve_idx);
    int get_curve_count(int clip_idx) const;
    String get_curve_name(int clip_idx, int curve_idx) const;
    int get_curve_bone_id(int clip_idx, int curve_idx) const;
    int get_curve_node_id(int clip_idx, int curve_idx) const;   // [NEW]
    int get_curve_kind(int clip_idx, int curve_idx) const;      // [NEW] CurveKind
    bool get_curve_is_rotation(int clip_idx, int curve_idx) const;
    bool get_curve_is_scale(int clip_idx, int curve_idx) const;
    bool get_curve_is_scalar(int clip_idx, int curve_idx) const;
    bool curve_is_bone(int clip_idx, int curve_idx) const;      // [NEW]
    bool curve_is_node(int clip_idx, int curve_idx) const;      // [NEW]
    int find_curve(int clip_idx, int bone_id, int node_id, int kind) const; // [NEW]

    void add_keyframe_scalar(int clip_idx, int curve_idx, float time, float value, int interp);
    void add_keyframe_vector(int clip_idx, int curve_idx, float time, const Vector3& value, int interp);
    void add_keyframe_quaternion(int clip_idx, int curve_idx, float time, const Quaternion& value, int interp);
    void delete_keyframe(int clip_idx, int curve_idx, int key_idx);
    int get_keyframe_count(int clip_idx, int curve_idx) const;
    float get_keyframe_time(int clip_idx, int curve_idx, int key_idx) const;

    void set_keyframe_scalar_value(int clip_idx, int curve_idx, int key_idx, float value);
    void set_keyframe_vector_value(int clip_idx, int curve_idx, int key_idx, const Vector3& value);
    void set_keyframe_quaternion_value(int clip_idx, int curve_idx, int key_idx, const Quaternion& value);
    float get_keyframe_scalar_value(int clip_idx, int curve_idx, int key_idx) const;          // [NEW]
    Vector3 get_keyframe_vector_value(int clip_idx, int curve_idx, int key_idx) const;        // [NEW]
    Quaternion get_keyframe_quaternion_value(int clip_idx, int curve_idx, int key_idx) const; // [NEW]
    void set_keyframe_time(int clip_idx, int curve_idx, int key_idx, float time);

    void set_keyframe_interp(int clip_idx, int curve_idx, int key_idx, int interp);   // [NEW]
    int get_keyframe_interp(int clip_idx, int curve_idx, int key_idx) const;         // [NEW]

    void set_keyframe_tangent_mode(int clip_idx, int curve_idx, int key_idx, int mode);
    int get_keyframe_tangent_mode(int clip_idx, int curve_idx, int key_idx) const;
    void set_keyframe_scalar_tangents(int clip_idx, int curve_idx, int key_idx, float in_tan, float out_tan);
    void set_keyframe_vector_tangents(int clip_idx, int curve_idx, int key_idx, const Vector3& in_tan, const Vector3& out_tan);
    Vector2 get_keyframe_scalar_tangents(int clip_idx, int curve_idx, int key_idx) const;              // [NEW] (in, out)
    Vector3 get_keyframe_vector_in_tangent(int clip_idx, int curve_idx, int key_idx) const;           // [NEW]
    Vector3 get_keyframe_vector_out_tangent(int clip_idx, int curve_idx, int key_idx) const;           // [NEW]
    void auto_compute_tangents(int clip_idx, int curve_idx);
    void flatten_keyframe(int clip_idx, int curve_idx, int key_idx); // [NEW] zero tangents (handle "flat")

    void play(int clip_idx);
    void pause();
    void stop();
    void seek(float time);
    void advance(float delta);
    bool is_clip_playing() const;
    float get_playback_time() const;
    int get_current_clip() const;     // [NEW]
    void set_current_clip(int idx);   // [NEW] switches clip, keeps time valid, no auto-play

    float sample_scalar(int clip_idx, int curve_idx, float time) const;
    Vector3 sample_vector(int clip_idx, int curve_idx, float time) const;
    Quaternion sample_quaternion(int clip_idx, int curve_idx, float time) const;

    PackedFloat32Array sample_pose(int clip_idx, float time, int bone_count) const;

    // [NEW] Sample all node-targeting curves for one node at a time.
    // Returns Array: [has_pos, pos, has_rot, quat, has_scale, scale]
    Array sample_node_transform(int clip_idx, int node_id, float time) const;
    PackedInt32Array get_animated_node_ids(int clip_idx) const;   // [NEW]

    // Curve visualization: returns Array of Vector2(time, value) for drawing
    PackedVector2Array get_curve_samples(int clip_idx, int curve_idx, int component, int resolution) const;

    // Symmetry
    void mirror_bone_curves(int clip_idx, int source_bone, int target_bone, int mirror_axis);
    void apply_symmetry(int clip_idx, const Ref<VFXSkeleton>& skeleton);

    void from_godot_animation(const Object* anim, int bone_count);

    PackedByteArray serialize_clip(int idx) const;
    void deserialize_clip(const PackedByteArray& data);
};

VARIANT_ENUM_CAST(VFXAnimator::InterpolationType);
VARIANT_ENUM_CAST(VFXAnimator::TangentMode);
VARIANT_ENUM_CAST(VFXAnimator::LoopMode);
VARIANT_ENUM_CAST(VFXAnimator::CurveKind);

#endif
