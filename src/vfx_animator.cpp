#include "vfx_animator.h"
#include "vfx_skeleton.h"
#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <algorithm>
#include <cmath>
#include <set>

using namespace godot;

// Rotation curves are euler-backed (radians in vec_value) so bezier handles
// work per component. Unwrap keeps each component within ±PI of a reference
// so keys never spin the long way between poses.
static Vector3 _unwrap_euler_near(Vector3 euler, const Vector3& ref) {
    const float TAU = 6.283185307179586f;
    const float PI  = 3.141592653589793f;
    for (int i = 0; i < 3; i++) {
        float d = fmodf(euler[i] - ref[i] + PI, TAU);
        if (d < 0.0f) d += TAU;
        euler[i] = ref[i] + d - PI;
    }
    return euler;
}

// Euler reference for a new key: the last key at or before `time`
// (or the first key if `time` is before everything).
static Vector3 _nearest_euler_ref(const AnimationCurve& curve, float time) {
    Vector3 ref = curve.keys.empty() ? Vector3() : curve.keys.front().vec_value;
    for (size_t i = 0; i < curve.keys.size(); i++) {
        if (curve.keys[i].time <= time + 0.0005f) ref = curve.keys[i].vec_value;
        else break;
    }
    return ref;
}

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
        float p0 = k0.value;
        float p1 = k0.value + k0.out_tangent;
        float p2 = k1.value + k1.in_tangent;
        float p3 = k1.value;
        return omt3 * p0 + 3.0f * omt2 * t * p1 + 3.0f * omt * t2 * p2 + t3 * p3;
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

    if (k0.interp == VFXAnimator::INTERP_STEP) return k0.vec_value;
    if (k0.interp == VFXAnimator::INTERP_BEZIER) {
        float t2 = t * t;
        float t3 = t2 * t;
        float omt = 1.0f - t;
        float omt2 = omt * omt;
        float omt3 = omt2 * omt;
        Vector3 p0 = k0.vec_value;
        Vector3 p1 = k0.vec_value + k0.vec_out_tangent;
        Vector3 p2 = k1.vec_value + k1.vec_in_tangent;
        Vector3 p3 = k1.vec_value;
        return omt3 * p0 + 3.0f * omt2 * t * p1 + 3.0f * omt * t2 * p2 + t3 * p3;
    }
    return k0.vec_value.lerp(k1.vec_value, t);
}

Quaternion AnimationCurve::sample_quaternion(float time) const {
    if (keys.empty()) return Quaternion();
    if (keys.size() == 1) {
        return keys[0].has_quaternion
            ? keys[0].quat_value.normalized()
            : Basis::from_euler(keys[0].vec_value).get_rotation_quaternion();
    }

    size_t i = 0;
    for (; i < keys.size() - 1; i++) {
        if (time >= keys[i].time && time <= keys[i + 1].time) break;
    }
    if (i >= keys.size() - 1) {
        const Keyframe& last = keys.back();
        return last.has_quaternion
            ? last.quat_value.normalized()
            : Basis::from_euler(last.vec_value).get_rotation_quaternion();
    }

    const Keyframe& k0 = keys[i];
    const Keyframe& k1 = keys[i + 1];
    const float dt = k1.time - k0.time;
    const float t = dt > 0.0001f ? CLAMP((time - k0.time) / dt, 0.0f, 1.0f) : 0.0f;

    // GLTF LINEAR rotation interpolation is quaternion slerp, not independent
    // interpolation of Euler components. Keep Euler-backed Bezier editing for
    // curves explicitly authored with the editor's Bezier mode.
    if (k0.interp == VFXAnimator::INTERP_STEP) {
        return k0.has_quaternion
            ? k0.quat_value.normalized()
            : Basis::from_euler(k0.vec_value).get_rotation_quaternion();
    }
    if (k0.interp == VFXAnimator::INTERP_LINEAR && k0.has_quaternion && k1.has_quaternion) {
        Quaternion from = k0.quat_value.normalized();
        Quaternion to = k1.quat_value.normalized();
        // Choose the equivalent sign that gives the shortest spherical path.
        const float dot = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
        if (dot < 0.0f) to = Quaternion(-to.x, -to.y, -to.z, -to.w);
        return from.slerp(to, t).normalized();
    }

    return Basis::from_euler(sample_vector(time)).get_rotation_quaternion();
}

void AnimationCurve::auto_compute_tangents() {
    if (keys.size() < 2) return;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].tangent_mode != VFXAnimator::TANGENT_AUTO) continue;

        float prev_t = (i > 0) ? keys[i-1].time : keys[0].time;
        float next_t = (i + 1 < keys.size()) ? keys[i+1].time : keys.back().time;
        float dt = next_t - prev_t;
        if (dt < 0.0001f) dt = 1.0f;
        float dt_out = next_t - keys[i].time;
        float dt_in = keys[i].time - prev_t;

        if (is_scalar) {
            float prev_val = (i > 0) ? keys[i-1].value : keys[0].value;
            float next_val = (i + 1 < keys.size()) ? keys[i+1].value : keys.back().value;
            float slope = (next_val - prev_val) / dt;
            keys[i].out_tangent = slope * dt_out * 0.3333f;
            keys[i].in_tangent = slope * dt_in * 0.3333f;
        } else {
            Vector3 prev_val = (i > 0) ? keys[i-1].vec_value : keys[0].vec_value;
            Vector3 next_val = (i + 1 < keys.size()) ? keys[i+1].vec_value : keys.back().vec_value;
            Vector3 slope = (next_val - prev_val) / dt;
            keys[i].vec_out_tangent = slope * dt_out * 0.3333f;
            keys[i].vec_in_tangent = slope * dt_in * 0.3333f;
        }
    }
}

// ------------------------------------------------------------------
// Bindings
// ------------------------------------------------------------------
void VFXAnimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_clip", "name", "duration", "fps"), &VFXAnimator::create_clip);
    ClassDB::bind_method(D_METHOD("delete_clip", "idx"), &VFXAnimator::delete_clip);
    ClassDB::bind_method(D_METHOD("get_clip_count"), &VFXAnimator::get_clip_count);
    ClassDB::bind_method(D_METHOD("get_clip_name", "idx"), &VFXAnimator::get_clip_name);
    ClassDB::bind_method(D_METHOD("set_clip_name", "idx", "name"), &VFXAnimator::set_clip_name);
    ClassDB::bind_method(D_METHOD("get_clip_duration", "idx"), &VFXAnimator::get_clip_duration);
    ClassDB::bind_method(D_METHOD("set_clip_duration", "idx", "duration"), &VFXAnimator::set_clip_duration);
    ClassDB::bind_method(D_METHOD("get_clip_fps", "idx"), &VFXAnimator::get_clip_fps);
    ClassDB::bind_method(D_METHOD("set_clip_fps", "idx", "fps"), &VFXAnimator::set_clip_fps);
    ClassDB::bind_method(D_METHOD("set_clip_loop", "idx", "loop"), &VFXAnimator::set_clip_loop);
    ClassDB::bind_method(D_METHOD("get_clip_loop", "idx"), &VFXAnimator::get_clip_loop);
    ClassDB::bind_method(D_METHOD("set_clip_loop_mode", "idx", "mode"), &VFXAnimator::set_clip_loop_mode);
    ClassDB::bind_method(D_METHOD("get_clip_loop_mode", "idx"), &VFXAnimator::get_clip_loop_mode);

    ClassDB::bind_method(D_METHOD("add_curve", "clip_idx", "name", "bone_id", "is_rotation", "is_scale"), &VFXAnimator::add_curve);
    ClassDB::bind_method(D_METHOD("add_node_curve", "clip_idx", "name", "node_id", "kind"), &VFXAnimator::add_node_curve);
    ClassDB::bind_method(D_METHOD("delete_curve", "clip_idx", "curve_idx"), &VFXAnimator::delete_curve);
    ClassDB::bind_method(D_METHOD("get_curve_count", "clip_idx"), &VFXAnimator::get_curve_count);
    ClassDB::bind_method(D_METHOD("get_curve_name", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_name);
    ClassDB::bind_method(D_METHOD("get_curve_bone_id", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_bone_id);
    ClassDB::bind_method(D_METHOD("get_curve_node_id", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_node_id);
    ClassDB::bind_method(D_METHOD("get_curve_kind", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_kind);
    ClassDB::bind_method(D_METHOD("get_curve_is_rotation", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_is_rotation);
    ClassDB::bind_method(D_METHOD("get_curve_is_scale", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_is_scale);
    ClassDB::bind_method(D_METHOD("get_curve_is_scalar", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_is_scalar);
    ClassDB::bind_method(D_METHOD("curve_is_bone", "clip_idx", "curve_idx"), &VFXAnimator::curve_is_bone);
    ClassDB::bind_method(D_METHOD("curve_is_node", "clip_idx", "curve_idx"), &VFXAnimator::curve_is_node);
    ClassDB::bind_method(D_METHOD("find_curve", "clip_idx", "bone_id", "node_id", "kind"), &VFXAnimator::find_curve);

    ClassDB::bind_method(D_METHOD("add_keyframe_scalar", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_scalar);
    ClassDB::bind_method(D_METHOD("add_keyframe_vector", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_vector);
    ClassDB::bind_method(D_METHOD("add_keyframe_quaternion", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_quaternion);
    ClassDB::bind_method(D_METHOD("delete_keyframe", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::delete_keyframe);
    ClassDB::bind_method(D_METHOD("get_keyframe_count", "clip_idx", "curve_idx"), &VFXAnimator::get_keyframe_count);
    ClassDB::bind_method(D_METHOD("get_keyframe_time", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_time);

    ClassDB::bind_method(D_METHOD("set_keyframe_scalar_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_scalar_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_vector_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_vector_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_quaternion_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_quaternion_value);
    ClassDB::bind_method(D_METHOD("get_keyframe_scalar_value", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_scalar_value);
    ClassDB::bind_method(D_METHOD("get_keyframe_vector_value", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_vector_value);
    ClassDB::bind_method(D_METHOD("get_keyframe_quaternion_value", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_quaternion_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_time", "clip_idx", "curve_idx", "key_idx", "time"), &VFXAnimator::set_keyframe_time);

    ClassDB::bind_method(D_METHOD("set_keyframe_interp", "clip_idx", "curve_idx", "key_idx", "interp"), &VFXAnimator::set_keyframe_interp);
    ClassDB::bind_method(D_METHOD("get_keyframe_interp", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_interp);

    ClassDB::bind_method(D_METHOD("set_keyframe_tangent_mode", "clip_idx", "curve_idx", "key_idx", "mode"), &VFXAnimator::set_keyframe_tangent_mode);
    ClassDB::bind_method(D_METHOD("get_keyframe_tangent_mode", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_tangent_mode);
    ClassDB::bind_method(D_METHOD("set_keyframe_scalar_tangents", "clip_idx", "curve_idx", "key_idx", "in_tan", "out_tan"), &VFXAnimator::set_keyframe_scalar_tangents);
    ClassDB::bind_method(D_METHOD("set_keyframe_vector_tangents", "clip_idx", "curve_idx", "key_idx", "in_tan", "out_tan"), &VFXAnimator::set_keyframe_vector_tangents);
    ClassDB::bind_method(D_METHOD("get_keyframe_scalar_tangents", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_scalar_tangents);
    ClassDB::bind_method(D_METHOD("get_keyframe_vector_in_tangent", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_vector_in_tangent);
    ClassDB::bind_method(D_METHOD("get_keyframe_vector_out_tangent", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_vector_out_tangent);
    ClassDB::bind_method(D_METHOD("auto_compute_tangents", "clip_idx", "curve_idx"), &VFXAnimator::auto_compute_tangents);
    ClassDB::bind_method(D_METHOD("flatten_keyframe", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::flatten_keyframe);

    ClassDB::bind_method(D_METHOD("play", "clip_idx"), &VFXAnimator::play);
    ClassDB::bind_method(D_METHOD("pause"), &VFXAnimator::pause);
    ClassDB::bind_method(D_METHOD("stop"), &VFXAnimator::stop);
    ClassDB::bind_method(D_METHOD("seek", "time"), &VFXAnimator::seek);
    ClassDB::bind_method(D_METHOD("advance", "delta"), &VFXAnimator::advance);
    ClassDB::bind_method(D_METHOD("is_clip_playing"), &VFXAnimator::is_clip_playing);
    ClassDB::bind_method(D_METHOD("get_playback_time"), &VFXAnimator::get_playback_time);
    ClassDB::bind_method(D_METHOD("get_current_clip"), &VFXAnimator::get_current_clip);
    ClassDB::bind_method(D_METHOD("set_current_clip", "idx"), &VFXAnimator::set_current_clip);

    ClassDB::bind_method(D_METHOD("sample_scalar", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_scalar);
    ClassDB::bind_method(D_METHOD("sample_vector", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_vector);
    ClassDB::bind_method(D_METHOD("sample_quaternion", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_quaternion);
    ClassDB::bind_method(D_METHOD("sample_pose", "clip_idx", "time", "bone_count"), &VFXAnimator::sample_pose);
    ClassDB::bind_method(D_METHOD("sample_node_transform", "clip_idx", "node_id", "time"), &VFXAnimator::sample_node_transform);
    ClassDB::bind_method(D_METHOD("get_animated_node_ids", "clip_idx"), &VFXAnimator::get_animated_node_ids);

    ClassDB::bind_method(D_METHOD("get_curve_samples", "clip_idx", "curve_idx", "component", "resolution"), &VFXAnimator::get_curve_samples);

    ClassDB::bind_method(D_METHOD("mirror_bone_curves", "clip_idx", "source_bone", "target_bone", "mirror_axis"), &VFXAnimator::mirror_bone_curves);
    ClassDB::bind_method(D_METHOD("apply_symmetry", "clip_idx", "skeleton"), &VFXAnimator::apply_symmetry);

    ClassDB::bind_method(D_METHOD("serialize_clip", "idx"), &VFXAnimator::serialize_clip);
    ClassDB::bind_method(D_METHOD("deserialize_clip", "data"), &VFXAnimator::deserialize_clip);
    ClassDB::bind_method(D_METHOD("from_godot_animation", "anim", "bone_count"), &VFXAnimator::from_godot_animation);

    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_LINEAR", INTERP_LINEAR);
    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_BEZIER", INTERP_BEZIER);
    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_STEP", INTERP_STEP);

    ClassDB::bind_integer_constant(get_class_static(), "", "TANGENT_FREE", TANGENT_FREE);
    ClassDB::bind_integer_constant(get_class_static(), "", "TANGENT_ALIGNED", TANGENT_ALIGNED);
    ClassDB::bind_integer_constant(get_class_static(), "", "TANGENT_MIRRORED", TANGENT_MIRRORED);
    ClassDB::bind_integer_constant(get_class_static(), "", "TANGENT_AUTO", TANGENT_AUTO);

    ClassDB::bind_integer_constant(get_class_static(), "", "LOOP_LOOP", LOOP_LOOP);
    ClassDB::bind_integer_constant(get_class_static(), "", "LOOP_CLAMP", LOOP_CLAMP);
    ClassDB::bind_integer_constant(get_class_static(), "", "LOOP_PING_PONG", LOOP_PING_PONG);

    ClassDB::bind_integer_constant(get_class_static(), "", "CURVE_POSITION", CURVE_POSITION);
    ClassDB::bind_integer_constant(get_class_static(), "", "CURVE_ROTATION", CURVE_ROTATION);
    ClassDB::bind_integer_constant(get_class_static(), "", "CURVE_SCALE", CURVE_SCALE);
    ClassDB::bind_integer_constant(get_class_static(), "", "CURVE_SCALAR", CURVE_SCALAR);
}

VFXAnimator::VFXAnimator() {}
VFXAnimator::~VFXAnimator() {}

int VFXAnimator::create_clip(const String& name, float duration, float fps) {
    AnimationClip clip;
    clip.name = name;
    clip.duration = duration;
    clip.fps = fps;
    clips.push_back(clip);
    return clips.size() - 1;
}

void VFXAnimator::delete_clip(int idx) {
    if (idx >= 0 && idx < (int)clips.size()) {
        clips.erase(clips.begin() + idx);
        if (current_clip == idx) { current_clip = -1; is_playing = false; }
        else if (current_clip > idx) current_clip--;
    }
}

int VFXAnimator::get_clip_count() const { return clips.size(); }
String VFXAnimator::get_clip_name(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].name;
    return "";
}
void VFXAnimator::set_clip_name(int idx, const String& name) {
    if (idx >= 0 && idx < (int)clips.size()) clips[idx].name = name;
}
float VFXAnimator::get_clip_duration(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].duration;
    return 0.0f;
}
void VFXAnimator::set_clip_duration(int idx, float duration) {
    if (idx >= 0 && idx < (int)clips.size()) clips[idx].duration = MAX(duration, 0.001f);
}
float VFXAnimator::get_clip_fps(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].fps;
    return 30.0f;
}
void VFXAnimator::set_clip_fps(int idx, float fps) {
    if (idx >= 0 && idx < (int)clips.size()) clips[idx].fps = MAX(fps, 1.0f);
}
void VFXAnimator::set_clip_loop(int idx, bool loop) {
    if (idx >= 0 && idx < (int)clips.size()) clips[idx].loop = loop;
}
bool VFXAnimator::get_clip_loop(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].loop;
    return true;
}
void VFXAnimator::set_clip_loop_mode(int idx, int mode) {
    if (idx >= 0 && idx < (int)clips.size()) clips[idx].loop_mode = mode;
}
int VFXAnimator::get_clip_loop_mode(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].loop_mode;
    return LOOP_LOOP;
}

int VFXAnimator::add_curve(int clip_idx, const String& name, int bone_id, bool is_rotation, bool is_scale) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return -1;
    AnimationCurve curve;
    curve.name = name;
    curve.bone_id = bone_id;
    curve.is_rotation = is_rotation;
    curve.is_scale = is_scale;
    clips[clip_idx].curves.push_back(curve);
    return clips[clip_idx].curves.size() - 1;
}

int VFXAnimator::add_node_curve(int clip_idx, const String& name, int node_id, int kind) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return -1;
    AnimationCurve curve;
    curve.name = name;
    curve.node_id = node_id;
    switch (kind) {
        case CURVE_ROTATION: curve.is_rotation = true; break;
        case CURVE_SCALE: curve.is_scale = true; break;
        case CURVE_SCALAR: curve.is_scalar = true; break;
        default: break; // CURVE_POSITION
    }
    clips[clip_idx].curves.push_back(curve);
    return clips[clip_idx].curves.size() - 1;
}

void VFXAnimator::delete_curve(int clip_idx, int curve_idx) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    auto& curves = clips[clip_idx].curves;
    if (curve_idx >= 0 && curve_idx < (int)curves.size()) {
        curves.erase(curves.begin() + curve_idx);
    }
}

int VFXAnimator::get_curve_count(int clip_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return 0;
    return clips[clip_idx].curves.size();
}

String VFXAnimator::get_curve_name(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return "";
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return "";
    return clips[clip_idx].curves[curve_idx].name;
}

int VFXAnimator::get_curve_bone_id(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return -1;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return -1;
    return clips[clip_idx].curves[curve_idx].bone_id;
}

int VFXAnimator::get_curve_node_id(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return -1;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return -1;
    return clips[clip_idx].curves[curve_idx].node_id;
}

int VFXAnimator::get_curve_kind(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return CURVE_POSITION;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return CURVE_POSITION;
    const AnimationCurve& c = clips[clip_idx].curves[curve_idx];
    if (c.is_rotation) return CURVE_ROTATION;
    if (c.is_scale) return CURVE_SCALE;
    if (c.is_scalar) return CURVE_SCALAR;
    return CURVE_POSITION;
}

bool VFXAnimator::get_curve_is_rotation(int clip_idx, int curve_idx) const {
    return get_curve_kind(clip_idx, curve_idx) == CURVE_ROTATION;
}
bool VFXAnimator::get_curve_is_scale(int clip_idx, int curve_idx) const {
    return get_curve_kind(clip_idx, curve_idx) == CURVE_SCALE;
}
bool VFXAnimator::get_curve_is_scalar(int clip_idx, int curve_idx) const {
    return get_curve_kind(clip_idx, curve_idx) == CURVE_SCALAR;
}
bool VFXAnimator::curve_is_bone(int clip_idx, int curve_idx) const {
    return get_curve_bone_id(clip_idx, curve_idx) >= 0;
}
bool VFXAnimator::curve_is_node(int clip_idx, int curve_idx) const {
    return get_curve_node_id(clip_idx, curve_idx) >= 0;
}

int VFXAnimator::find_curve(int clip_idx, int bone_id, int node_id, int kind) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return -1;
    const auto& curves = clips[clip_idx].curves;
    for (int i = 0; i < (int)curves.size(); i++) {
        if (curves[i].bone_id == bone_id && curves[i].node_id == node_id &&
            get_curve_kind(clip_idx, i) == kind) {
            return i;
        }
    }
    return -1;
}

// ------------------------------------------------------------------
// Keys — add_* now REPLACES an existing key at the same time
// ------------------------------------------------------------------
void VFXAnimator::add_keyframe_scalar(int clip_idx, int curve_idx, float time, float value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& curve = clips[clip_idx].curves[curve_idx];
    if (curve.keys.empty()) curve.is_scalar = true;
    for (auto& k : curve.keys) {
        if (fabsf(k.time - time) < 0.0005f) {
            k.value = value;
            k.interp = interp;
            return;
        }
    }
    Keyframe k;
    k.time = time;
    k.value = value;
    k.interp = interp;
    curve.keys.push_back(k);
    curve.sort_keys();
}

void VFXAnimator::add_keyframe_vector(int clip_idx, int curve_idx, float time, const Vector3& value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& curve = clips[clip_idx].curves[curve_idx];
    curve.is_scalar = false;

    if (curve.is_rotation) {
        // Euler-backed rotation curve: store angles, unwrapped vs the
        // nearest key in time for continuity.
        for (auto& k : curve.keys) {
            if (fabsf(k.time - time) < 0.0005f) {
                k.vec_value = _unwrap_euler_near(value, k.vec_value);
                k.quat_value = Basis::from_euler(k.vec_value).get_rotation_quaternion().normalized();
                k.has_quaternion = true;
                k.interp = interp;
                return;
            }
        }
        Keyframe k;
        k.time = time;
        k.vec_value = _unwrap_euler_near(value, _nearest_euler_ref(curve, time));
        k.quat_value = Basis::from_euler(k.vec_value).get_rotation_quaternion().normalized();
        k.has_quaternion = true;
        k.interp = interp;
        curve.keys.push_back(k);
        curve.sort_keys();
        return;
    }
    for (auto& k : curve.keys) {
        if (fabsf(k.time - time) < 0.0005f) {
            k.vec_value = value;
            k.interp = interp;
            return;
        }
    }
    Keyframe k;
    k.time = time;
    k.vec_value = value;
    k.interp = interp;
    curve.keys.push_back(k);
    curve.sort_keys();
}

void VFXAnimator::add_keyframe_quaternion(int clip_idx, int curve_idx, float time, const Quaternion& value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& curve = clips[clip_idx].curves[curve_idx];
    curve.is_scalar = false;

    if (curve.is_rotation) {
        // Keep Euler values for the curve editor, but retain the original
        // quaternion for faithful LINEAR (slerp) playback.
        Quaternion q = value.normalized();
        add_keyframe_vector(clip_idx, curve_idx, time, Basis(q).get_euler(), interp);
        auto& keys = clips[clip_idx].curves[curve_idx].keys;
        for (auto& key : keys) {
            if (fabsf(key.time - time) < 0.0005f) {
                key.quat_value = q;
                key.has_quaternion = true;
                break;
            }
        }
        return;
    }
    for (auto& k : curve.keys) {
        if (fabsf(k.time - time) < 0.0005f) {
            k.quat_value = value;
            k.interp = interp;
            return;
        }
    }
    Keyframe k;
    k.time = time;
    k.quat_value = value;
    k.interp = interp;
    curve.keys.push_back(k);
    curve.sort_keys();
}

void VFXAnimator::delete_keyframe(int clip_idx, int curve_idx, int key_idx) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx >= 0 && key_idx < (int)keys.size()) {
        keys.erase(keys.begin() + key_idx);
    }
}

int VFXAnimator::get_keyframe_count(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return 0;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return 0;
    return clips[clip_idx].curves[curve_idx].keys.size();
}

float VFXAnimator::get_keyframe_time(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return 0.0f;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return 0.0f;
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return 0.0f;
    return clips[clip_idx].curves[curve_idx].keys[key_idx].time;
}

void VFXAnimator::set_keyframe_scalar_value(int clip_idx, int curve_idx, int key_idx, float value) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].value = value;
}

void VFXAnimator::set_keyframe_vector_value(int clip_idx, int curve_idx, int key_idx, const Vector3& value) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].vec_value = value;
    if (clips[clip_idx].curves[curve_idx].is_rotation) {
        keys[key_idx].quat_value = Basis::from_euler(value).get_rotation_quaternion().normalized();
        keys[key_idx].has_quaternion = true;
    }
}

void VFXAnimator::set_keyframe_quaternion_value(int clip_idx, int curve_idx, int key_idx, const Quaternion& value) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    auto& curve = clips[clip_idx].curves[curve_idx];
    if (curve.is_rotation) {
        keys[key_idx].vec_value = Basis(value).get_euler();
        keys[key_idx].quat_value = value.normalized();
        keys[key_idx].has_quaternion = true;
        return;
    }
    keys[key_idx].quat_value = value;
}

float VFXAnimator::get_keyframe_scalar_value(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return 0.0f;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return 0.0f;
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return 0.0f;
    return clips[clip_idx].curves[curve_idx].keys[key_idx].value;
}

Vector3 VFXAnimator::get_keyframe_vector_value(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Vector3();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Vector3();
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return Vector3();
    return clips[clip_idx].curves[curve_idx].keys[key_idx].vec_value;
}

Quaternion VFXAnimator::get_keyframe_quaternion_value(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Quaternion();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Quaternion();
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return Quaternion();
    const auto& curve = clips[clip_idx].curves[curve_idx];
    if (curve.is_rotation) {
        const Keyframe& key = curve.keys[key_idx];
        return key.has_quaternion
            ? key.quat_value.normalized()
            : Basis::from_euler(key.vec_value).get_rotation_quaternion();
    }
    return curve.keys[key_idx].quat_value;
}

void VFXAnimator::set_keyframe_time(int clip_idx, int curve_idx, int key_idx, float time) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].time = MAX(time, 0.0f);
    clips[clip_idx].curves[curve_idx].sort_keys();
}

void VFXAnimator::set_keyframe_interp(int clip_idx, int curve_idx, int key_idx, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].interp = interp;
}

int VFXAnimator::get_keyframe_interp(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return INTERP_LINEAR;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return INTERP_LINEAR;
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return INTERP_LINEAR;
    return clips[clip_idx].curves[curve_idx].keys[key_idx].interp;
}

void VFXAnimator::set_keyframe_tangent_mode(int clip_idx, int curve_idx, int key_idx, int mode) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].tangent_mode = mode;
    if (mode == TANGENT_AUTO) {
        clips[clip_idx].curves[curve_idx].auto_compute_tangents();
    } else if (mode == TANGENT_ALIGNED || mode == TANGENT_MIRRORED) {
        // mirror the two handle heights (continuity): in = -out
        if (clips[clip_idx].curves[curve_idx].is_scalar) {
            float out = keys[key_idx].out_tangent;
            keys[key_idx].in_tangent = -out;
        } else {
            keys[key_idx].vec_in_tangent = keys[key_idx].vec_out_tangent * -1.0f;
        }
    }
}

int VFXAnimator::get_keyframe_tangent_mode(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return TANGENT_FREE;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return TANGENT_FREE;
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return TANGENT_FREE;
    return clips[clip_idx].curves[curve_idx].keys[key_idx].tangent_mode;
}

void VFXAnimator::set_keyframe_scalar_tangents(int clip_idx, int curve_idx, int key_idx, float in_tan, float out_tan) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].in_tangent = in_tan;
    keys[key_idx].out_tangent = out_tan;
    keys[key_idx].tangent_mode = TANGENT_FREE;
}

void VFXAnimator::set_keyframe_vector_tangents(int clip_idx, int curve_idx, int key_idx, const Vector3& in_tan, const Vector3& out_tan) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].vec_in_tangent = in_tan;
    keys[key_idx].vec_out_tangent = out_tan;
    keys[key_idx].tangent_mode = TANGENT_FREE;
}

Vector2 VFXAnimator::get_keyframe_scalar_tangents(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Vector2();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Vector2();
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return Vector2();
    const Keyframe& k = clips[clip_idx].curves[curve_idx].keys[key_idx];
    return Vector2(k.in_tangent, k.out_tangent);
}

Vector3 VFXAnimator::get_keyframe_vector_in_tangent(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Vector3();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Vector3();
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return Vector3();
    return clips[clip_idx].curves[curve_idx].keys[key_idx].vec_in_tangent;
}

Vector3 VFXAnimator::get_keyframe_vector_out_tangent(int clip_idx, int curve_idx, int key_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Vector3();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Vector3();
    if (key_idx < 0 || key_idx >= (int)clips[clip_idx].curves[curve_idx].keys.size()) return Vector3();
    return clips[clip_idx].curves[curve_idx].keys[key_idx].vec_out_tangent;
}

void VFXAnimator::auto_compute_tangents(int clip_idx, int curve_idx) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    clips[clip_idx].curves[curve_idx].auto_compute_tangents();
}

void VFXAnimator::flatten_keyframe(int clip_idx, int curve_idx, int key_idx) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].in_tangent = 0.0f;
    keys[key_idx].out_tangent = 0.0f;
    keys[key_idx].vec_in_tangent = Vector3();
    keys[key_idx].vec_out_tangent = Vector3();
}

// ------------------------------------------------------------------
// Playback
// ------------------------------------------------------------------
void VFXAnimator::play(int clip_idx) {
    if (clip_idx >= 0 && clip_idx < (int)clips.size()) {
        current_clip = clip_idx;
        current_time = 0.0f;
        pingpong_time = 0.0f;
        is_playing = true;
    }
}

void VFXAnimator::pause() { is_playing = false; }
void VFXAnimator::stop() { is_playing = false; current_time = 0.0f; pingpong_time = 0.0f; }

void VFXAnimator::seek(float time) {
    if (current_clip < 0 || current_clip >= (int)clips.size()) return;
    const AnimationClip& clip = clips[current_clip];
    switch (clip.loop_mode) {
        case LOOP_CLAMP:
            current_time = fmin(fmax(time, 0.0f), clip.duration);
            break;
        case LOOP_PING_PONG: {
            float d = clip.duration * 2.0f;
            pingpong_time = time;  // keep playback continuous after a scrub
            float t = fmod(time, d);
            if (t < 0) t += d;
            current_time = (t > clip.duration) ? d - t : t;
            break;
        }
        case LOOP_LOOP:
        default:
            current_time = fmod(time, clip.duration);
            if (current_time < 0) current_time += clip.duration;
            break;
    }
}

void VFXAnimator::advance(float delta) {
    if (!is_playing || current_clip < 0) return;
    const AnimationClip& clip = clips[current_clip];
    if (clip.loop_mode == LOOP_PING_PONG) {
        // Advance an UNFOLDED accumulator; folding each frame from the folded
        // time makes it oscillate in place at the boundary (endless jitter).
        float d = clip.duration * 2.0f;
        if (d <= 0.0f) { current_time = 0.0f; return; }
        pingpong_time += delta;
        float t = fmod(pingpong_time, d);
        if (t < 0) t += d;
        current_time = (t > clip.duration) ? d - t : t;
    } else {
        seek(current_time + delta);
    }
}

bool VFXAnimator::is_clip_playing() const { return is_playing; }
float VFXAnimator::get_playback_time() const { return current_time; }
int VFXAnimator::get_current_clip() const { return current_clip; }

void VFXAnimator::set_current_clip(int idx) {
    if (idx >= 0 && idx < (int)clips.size()) {
        current_clip = idx;
        is_playing = false;
        current_time = 0.0f;
        pingpong_time = 0.0f;
    }
}

float VFXAnimator::sample_scalar(int clip_idx, int curve_idx, float time) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return 0.0f;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return 0.0f;
    return clips[clip_idx].curves[curve_idx].sample_scalar(time);
}

Vector3 VFXAnimator::sample_vector(int clip_idx, int curve_idx, float time) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Vector3();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Vector3();
    return clips[clip_idx].curves[curve_idx].sample_vector(time);
}

Quaternion VFXAnimator::sample_quaternion(int clip_idx, int curve_idx, float time) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return Quaternion();
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return Quaternion();
    return clips[clip_idx].curves[curve_idx].sample_quaternion(time);
}

PackedFloat32Array VFXAnimator::sample_pose(int clip_idx, float time, int bone_count) const {
    PackedFloat32Array pose;
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return pose;

    pose.resize(bone_count * 10);

    for (const auto& curve : clips[clip_idx].curves) {
        if (curve.bone_id < 0 || curve.bone_id >= bone_count) continue;
        int base = curve.bone_id * 10;

        if (curve.is_rotation) {
            Quaternion q = curve.sample_quaternion(time);
            pose[base + 3] = q.x;
            pose[base + 4] = q.y;
            pose[base + 5] = q.z;
            pose[base + 6] = q.w;
        } else if (curve.is_scale) {
            Vector3 s = curve.sample_vector(time);
            pose[base + 7] = s.x;
            pose[base + 8] = s.y;
            pose[base + 9] = s.z;
        } else if (!curve.is_scalar) {
            Vector3 p = curve.sample_vector(time);
            pose[base + 0] = p.x;
            pose[base + 1] = p.y;
            pose[base + 2] = p.z;
        }
    }
    return pose;
}

Array VFXAnimator::sample_node_transform(int clip_idx, int node_id, float time) const {
    Array result;
    result.resize(6);
    result[0] = false; result[1] = Vector3();
    result[2] = false; result[3] = Quaternion();
    result[4] = false; result[5] = Vector3(1, 1, 1);
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return result;

    for (const auto& curve : clips[clip_idx].curves) {
        if (curve.node_id != node_id) continue;
        if (curve.is_rotation) {
            result[2] = true;
            result[3] = curve.sample_quaternion(time);
        } else if (curve.is_scale) {
            result[4] = true;
            result[5] = curve.sample_vector(time);
        } else if (!curve.is_scalar) {
            result[0] = true;
            result[1] = curve.sample_vector(time);
        }
    }
    return result;
}

PackedInt32Array VFXAnimator::get_animated_node_ids(int clip_idx) const {
    PackedInt32Array ids;
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return ids;
    std::set<int> seen;
    for (const auto& curve : clips[clip_idx].curves) {
        if (curve.node_id >= 0 && !seen.count(curve.node_id)) {
            seen.insert(curve.node_id);
            ids.push_back(curve.node_id);
        }
    }
    return ids;
}

PackedVector2Array VFXAnimator::get_curve_samples(int clip_idx, int curve_idx, int component, int resolution) const {
    PackedVector2Array samples;
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return samples;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return samples;
    const AnimationCurve& curve = clips[clip_idx].curves[curve_idx];
    if (curve.keys.empty()) return samples;
    if (resolution < 2) resolution = 2;

    float start = curve.keys.front().time;
    float end = curve.keys.back().time;
    if (end <= start) end = start + 1.0f;

    samples.resize(resolution);
    for (int i = 0; i < resolution; i++) {
        float t = start + (end - start) * (float)i / (resolution - 1);
        float v = 0.0f;

        if (curve.is_rotation) {
            Vector3 euler = curve.sample_vector(t);
            v = (component == 0) ? euler.x : (component == 1) ? euler.y : euler.z;
        } else if (curve.is_scalar) {
            v = curve.sample_scalar(t);
        } else {
            Vector3 vec = curve.sample_vector(t);
            v = (component == 0) ? vec.x : (component == 1) ? vec.y : vec.z;
        }
        samples[i] = Vector2(t, v);
    }
    return samples;
}

void VFXAnimator::mirror_bone_curves(int clip_idx, int source_bone, int target_bone, int mirror_axis) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;

    std::vector<int> source_indices;
    for (int i = 0; i < (int)clips[clip_idx].curves.size(); i++) {
        if (clips[clip_idx].curves[i].bone_id == source_bone) {
            source_indices.push_back(i);
        }
    }

    for (int i = (int)clips[clip_idx].curves.size() - 1; i >= 0; i--) {
        if (clips[clip_idx].curves[i].bone_id == target_bone) {
            clips[clip_idx].curves.erase(clips[clip_idx].curves.begin() + i);
        }
    }

    for (int src_idx : source_indices) {
        AnimationCurve new_curve = clips[clip_idx].curves[src_idx];
        new_curve.bone_id = target_bone;

        for (auto& key : new_curve.keys) {
            if (new_curve.is_rotation) {
                key.vec_value.x = -key.vec_value.x;
                key.vec_value.z = -key.vec_value.z;
                key.vec_in_tangent.x = -key.vec_in_tangent.x;
                key.vec_in_tangent.z = -key.vec_in_tangent.z;
                key.vec_out_tangent.x = -key.vec_out_tangent.x;
                key.vec_out_tangent.z = -key.vec_out_tangent.z;
            } else if (!new_curve.is_scale && !new_curve.is_scalar) {
                key.vec_value.x = -key.vec_value.x;
                key.vec_in_tangent.x = -key.vec_in_tangent.x;
                key.vec_out_tangent.x = -key.vec_out_tangent.x;
            }
        }
        clips[clip_idx].curves.push_back(new_curve);
    }
}

void VFXAnimator::apply_symmetry(int clip_idx, const Ref<VFXSkeleton>& skeleton) {
    if (skeleton.is_null()) return;
    std::set<int> processed;
    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        if (processed.count(i)) continue;
        int sym = skeleton->get_symmetric_bone(i);
        if (sym >= 0 && sym != i && !processed.count(sym)) {
            mirror_bone_curves(clip_idx, i, sym, 0);
            processed.insert(i);
            processed.insert(sym);
        }
    }
}

// ------------------------------------------------------------------
// Serialization — format v3 (rotation curves store euler angles;
// v1/v2 quaternion keys are converted on load)
// ------------------------------------------------------------------
PackedByteArray VFXAnimator::serialize_clip(int idx) const {
    PackedByteArray data;
    if (idx < 0 || idx >= (int)clips.size()) return data;

    const AnimationClip& clip = clips[idx];

    // Magic "VFXA" + version 4 (preserve source quaternions for LINEAR rotation)
    data.append('V'); data.append('F'); data.append('X'); data.append('A');
    data.append(4);

    auto append_u16 = [&](uint16_t v) {
        data.append(v & 0xFF);
        data.append((v >> 8) & 0xFF);
    };
    auto append_u32 = [&](uint32_t v) {
        data.append(v & 0xFF);
        data.append((v >> 8) & 0xFF);
        data.append((v >> 16) & 0xFF);
        data.append((v >> 24) & 0xFF);
    };
    auto append_i32 = [&](int32_t v) { append_u32((uint32_t)v); };
    auto append_f32 = [&](float v) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&v);
        for (int i = 0; i < 4; i++) data.append(b[i]);
    };

    // Clip header
    CharString name_utf8 = clip.name.utf8();
    append_u16((uint16_t)name_utf8.size());
    for (int i = 0; i < name_utf8.size(); i++) data.append(name_utf8.ptr()[i]);

    append_f32(clip.duration);
    append_f32(clip.fps);
    data.append(clip.loop ? 1 : 0);
    data.append((uint8_t)clip.loop_mode);

    append_u32((uint32_t)clip.curves.size());

    for (const auto& curve : clip.curves) {
        CharString cname_utf8 = curve.name.utf8();
        append_u16((uint16_t)cname_utf8.size());
        for (int i = 0; i < cname_utf8.size(); i++) data.append(cname_utf8.ptr()[i]);

        append_i32(curve.bone_id);
        append_i32(curve.node_id); // v2
        uint8_t flags = (curve.is_rotation ? 1 : 0) | (curve.is_scale ? 2 : 0) | (curve.is_scalar ? 4 : 0);
        data.append(flags);
        uint8_t kind = curve.is_rotation ? (uint8_t)CURVE_ROTATION
                       : curve.is_scale ? (uint8_t)CURVE_SCALE
                       : curve.is_scalar ? (uint8_t)CURVE_SCALAR
                       : (uint8_t)CURVE_POSITION;
        data.append(kind); // v2 (redundant, for forward compat)

        append_u32((uint32_t)curve.keys.size());

        for (const auto& key : curve.keys) {
            append_f32(key.time);

            uint8_t val_type = 0; // scalar
            if (!curve.is_scalar) val_type = 1; // vector (rotation curves store euler angles)
            data.append(val_type);

            if (val_type == 0) {
                append_f32(key.value);
            } else {
                append_f32(key.vec_value.x);
                append_f32(key.vec_value.y);
                append_f32(key.vec_value.z);
            }

            append_f32(key.in_tangent);
            append_f32(key.out_tangent);
            append_f32(key.vec_in_tangent.x);
            append_f32(key.vec_in_tangent.y);
            append_f32(key.vec_in_tangent.z);
            append_f32(key.vec_out_tangent.x);
            append_f32(key.vec_out_tangent.y);
            append_f32(key.vec_out_tangent.z);
            data.append((uint8_t)key.interp);
            data.append((uint8_t)key.tangent_mode);
            data.append(key.has_quaternion ? 1 : 0);
            if (key.has_quaternion) {
                append_f32(key.quat_value.x);
                append_f32(key.quat_value.y);
                append_f32(key.quat_value.z);
                append_f32(key.quat_value.w);
            }
        }
    }

    return data;
}

void VFXAnimator::deserialize_clip(const PackedByteArray& data) {
    if (data.size() < 5) return;
    if (data[0] != 'V' || data[1] != 'F' || data[2] != 'X' || data[3] != 'A') return;
    uint8_t version = data[4];
    if (version < 1 || version > 4) return;

    int p = 5;
    auto read_u16 = [&]() -> uint16_t {
        if (p + 2 > data.size()) return 0;
        uint16_t v = data[p] | (data[p + 1] << 8);
        p += 2; return v;
    };
    auto read_u32 = [&]() -> uint32_t {
        if (p + 4 > data.size()) return 0;
        uint32_t v = data[p] | (data[p + 1] << 8) | (data[p + 2] << 16) | (data[p + 3] << 24);
        p += 4; return v;
    };
    auto read_i32 = [&]() -> int32_t { return (int32_t)read_u32(); };
    auto read_f32 = [&]() -> float {
        if (p + 4 > data.size()) return 0.0f;
        float v; uint8_t* b = reinterpret_cast<uint8_t*>(&v);
        for (int i = 0; i < 4; i++) b[i] = data[p + i];
        p += 4; return v;
    };
    auto read_string = [&]() -> String {
        uint16_t len = read_u16();
        if (p + len > data.size()) return "";
        String s;
        s.parse_utf8((const char*)&data[p], len);
        p += len;
        return s;
    };

    AnimationClip clip;
    clip.name = read_string();
    clip.duration = read_f32();
    clip.fps = read_f32();
    if (p >= data.size()) return;
    clip.loop = data[p++] != 0;
    if (p >= data.size()) return;
    clip.loop_mode = data[p++];

    uint32_t curve_count = read_u32();
    for (uint32_t c = 0; c < curve_count; c++) {
        AnimationCurve curve;
        curve.name = read_string();
        curve.bone_id = read_i32();
        if (version >= 2) {
            curve.node_id = read_i32();
        } else {
            curve.node_id = -1;
        }
        if (p >= data.size()) return;
        uint8_t flags = data[p++];
        curve.is_rotation = (flags & 1) != 0;
        curve.is_scale = (flags & 2) != 0;
        curve.is_scalar = (flags & 4) != 0;
        if (version >= 2) {
            if (p >= data.size()) return;
            p++; // kind byte (derived from flags anyway)
        }

        uint32_t key_count = read_u32();
        for (uint32_t k = 0; k < key_count; k++) {
            Keyframe key;
            key.time = read_f32();
            if (p >= data.size()) return;
            uint8_t val_type = data[p++];

            if (val_type == 0) {
                key.value = read_f32();
            } else if (val_type == 1) {
                key.vec_value.x = read_f32();
                key.vec_value.y = read_f32();
                key.vec_value.z = read_f32();
            } else {
                // v1/v2: rotation was stored as a quaternion — convert to euler
                Quaternion q;
                q.x = read_f32(); q.y = read_f32(); q.z = read_f32(); q.w = read_f32();
                Vector3 euler = Basis(q).get_euler();
                if (!curve.keys.empty()) euler = _unwrap_euler_near(euler, curve.keys.back().vec_value);
                key.vec_value = euler;
            }

            key.in_tangent = read_f32();
            key.out_tangent = read_f32();
            key.vec_in_tangent.x = read_f32();
            key.vec_in_tangent.y = read_f32();
            key.vec_in_tangent.z = read_f32();
            key.vec_out_tangent.x = read_f32();
            key.vec_out_tangent.y = read_f32();
            key.vec_out_tangent.z = read_f32();
            if (p >= data.size()) return;
            key.interp = data[p++];
            if (p >= data.size()) return;
            key.tangent_mode = data[p++];
            if (version >= 4) {
                if (p >= data.size()) return;
                key.has_quaternion = data[p++] != 0;
                if (key.has_quaternion) {
                    key.quat_value.x = read_f32();
                    key.quat_value.y = read_f32();
                    key.quat_value.z = read_f32();
                    key.quat_value.w = read_f32();
                    key.quat_value = key.quat_value.normalized();
                }
            } else if (curve.is_rotation) {
                key.quat_value = Basis::from_euler(key.vec_value).get_rotation_quaternion().normalized();
                key.has_quaternion = true;
            }

            curve.keys.push_back(key);
        }
        clip.curves.push_back(curve);
    }

    clips.push_back(clip);
}

void VFXAnimator::from_godot_animation(const Object* anim_obj, int bone_count) {
    const Animation* anim = Object::cast_to<Animation>(anim_obj);
    if (!anim || bone_count <= 0) return;

    String anim_name = anim->get_name();
    if (anim_name.is_empty()) anim_name = "imported";

    int clip_idx = create_clip(anim_name, (float)anim->get_length(), 30.0f);

    int track_count = anim->get_track_count();

    for (int t = 0; t < track_count; t++) {
        Animation::TrackType type = anim->track_get_type(t);
        NodePath path = anim->track_get_path(t);

        String bone_name;
        if (path.get_subname_count() > 0) {
            bone_name = path.get_subname(0);
        } else if (path.get_name_count() > 0) {
            bone_name = path.get_name(path.get_name_count() - 1);
        }

        int bone_id = -1;
        if (bone_name.is_valid_int()) {
            bone_id = bone_name.to_int();
        } else {
            String cleaned = bone_name.replace("position", "").replace("rotation", "").replace("scale", "").strip_edges();
            if (cleaned.is_valid_int()) {
                bone_id = cleaned.to_int();
            }
        }
        if (bone_id < 0) {
            bone_id = t % bone_count; // fallback: round-robin
        }

        bool is_rotation = false;
        bool is_scale = false;
        bool is_position = false;

        if (type == Animation::TYPE_POSITION_3D) {
            is_position = true;
        } else if (type == Animation::TYPE_ROTATION_3D) {
            is_rotation = true;
        } else if (type == Animation::TYPE_SCALE_3D) {
            is_scale = true;
        } else if (type == Animation::TYPE_VALUE) {
            String last_sub = path.get_subname_count() > 0 ? path.get_subname(path.get_subname_count() - 1) : "";
            last_sub = last_sub.to_lower();
            if (last_sub.contains("rot") || last_sub.contains("quat")) {
                is_rotation = true;
            } else if (last_sub.contains("scale")) {
                is_scale = true;
            } else {
                is_position = true;
            }
        } else {
            continue;
        }

        String curve_name = bone_name + (is_rotation ? "_rot" : (is_scale ? "_scale" : "_pos"));
        int curve_idx = add_curve(clip_idx, curve_name, bone_id, is_rotation, is_scale);

        int key_count = anim->track_get_key_count(t);
        for (int k = 0; k < key_count; k++) {
            float time = (float)anim->track_get_key_time(t, k);

            if (is_position) {
                Variant v = anim->track_get_key_value(t, k);
                if (v.get_type() != Variant::VECTOR3) continue;
                Vector3 pos = (Vector3)v;
                add_keyframe_vector(clip_idx, curve_idx, time, pos, INTERP_LINEAR);
            }
            else if (is_rotation) {
                Variant v = anim->track_get_key_value(t, k);
                Quaternion rot;
                if (v.get_type() == Variant::QUATERNION) {
                    rot = (Quaternion)v;
                } else if (v.get_type() == Variant::BASIS) {
                    rot = ((Basis)v).get_rotation_quaternion();
                } else {
                    continue;
                }
                add_keyframe_quaternion(clip_idx, curve_idx, time, rot, INTERP_LINEAR);
            }
            else if (is_scale) {
                Variant v = anim->track_get_key_value(t, k);
                if (v.get_type() != Variant::VECTOR3) continue;
                Vector3 scl = (Vector3)v;
                add_keyframe_vector(clip_idx, curve_idx, time, scl, INTERP_LINEAR);
            }
        }
    }
}