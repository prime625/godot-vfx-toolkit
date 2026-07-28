#ifndef VFX_ANIMATOR_H
#define VFX_ANIMATOR_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <vector>
#include <string>

using namespace godot;

struct Keyframe {
    float time = 0.0f;
    float value = 0.0f;
    Vector3 vec_value;
    Quaternion quat_value;
    float in_tangent = 0.0f;
    float out_tangent = 0.0f;
    int interp = 0;
};

struct AnimationCurve {
    String name;
    int bone_id = -1;
    bool is_rotation = false;
    bool is_scale = false;
    std::vector<Keyframe> keys;
    
    void sort_keys();
    float sample_scalar(float time) const;
    Vector3 sample_vector(float time) const;
    Quaternion sample_quaternion(float time) const;
};

struct AnimationClip {
    String name;
    float duration = 0.0f;
    float fps = 30.0f;
    bool loop = true;
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
    float get_clip_duration(int idx) const;
    void set_clip_loop(int idx, bool loop);
    bool get_clip_loop(int idx) const;
    
    int add_curve(int clip_idx, const String& name, int bone_id, bool is_rotation, bool is_scale);
    void delete_curve(int clip_idx, int curve_idx);
    int get_curve_count(int clip_idx) const;
    String get_curve_name(int clip_idx, int curve_idx) const;
    
    void add_keyframe_scalar(int clip_idx, int curve_idx, float time, float value, int interp);
    void add_keyframe_vector(int clip_idx, int curve_idx, float time, const Vector3& value, int interp);
    void add_keyframe_quaternion(int clip_idx, int curve_idx, float time, const Quaternion& value, int interp);
    void delete_keyframe(int clip_idx, int curve_idx, int key_idx);
    int get_keyframe_count(int clip_idx, int curve_idx) const;
    float get_keyframe_time(int clip_idx, int curve_idx, int key_idx) const;
    
    void play(int clip_idx);
    void pause();
    void stop();
    void seek(float time);
    void advance(float delta);
    bool is_clip_playing() const;
    float get_playback_time() const;
    
    float sample_scalar(int clip_idx, int curve_idx, float time) const;
    Vector3 sample_vector(int clip_idx, int curve_idx, float time) const;
    Quaternion sample_quaternion(int clip_idx, int curve_idx, float time) const;
    
    PackedFloat32Array sample_pose(int clip_idx, float time, int bone_count) const;
    
    void from_godot_animation(const Object* anim, int bone_count);
    
    PackedByteArray serialize_clip(int idx) const;
    void deserialize_clip(const PackedByteArray& data);
};

VARIANT_ENUM_CAST(VFXAnimator::InterpolationType);

#endif
