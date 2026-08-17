#include "vfx_animator.h"
#include "vfx_skeleton.h"
#include "vfx_math.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/animation.hpp>
#include <algorithm>
#include <cmath>
#include <set>

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
    if (keys.size() == 1) return keys[0].quat_value;

    size_t i = 0;
    for (; i < keys.size() - 1; i++) {
        if (time >= keys[i].time && time <= keys[i+1].time) break;
    }
    if (i >= keys.size() - 1) return keys.back().quat_value;

    const Keyframe& k0 = keys[i];
    const Keyframe& k1 = keys[i+1];
    float t = (k1.time - k0.time < 0.0001f) ? 0.0f : (time - k0.time) / (k1.time - k0.time);

    if (k0.interp == VFXAnimator::INTERP_STEP) return k0.quat_value;
    return k0.quat_value.slerp(k1.quat_value, t);
}

void AnimationCurve::auto_compute_tangents() {
    if (keys.size() < 2) return;
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].tangent_mode != VFXAnimator::TANGENT_AUTO) continue;

        Vector3 prev_val = (i > 0) ? keys[i-1].vec_value : keys[0].vec_value;
        Vector3 next_val = (i + 1 < keys.size()) ? keys[i+1].vec_value : keys.back().vec_value;
        float prev_t = (i > 0) ? keys[i-1].time : keys[0].time;
        float next_t = (i + 1 < keys.size()) ? keys[i+1].time : keys.back().time;

        float dt = next_t - prev_t;
        if (dt < 0.0001f) dt = 1.0f;

        Vector3 slope = (next_val - prev_val) / dt;
        float dt_out = next_t - keys[i].time;
        float dt_in = keys[i].time - prev_t;

        keys[i].vec_out_tangent = slope * dt_out * 0.3333f;
        keys[i].vec_in_tangent = slope * dt_in * 0.3333f;
    }
}

void VFXAnimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_clip", "name", "duration", "fps"), &VFXAnimator::create_clip);
    ClassDB::bind_method(D_METHOD("delete_clip", "idx"), &VFXAnimator::delete_clip);
    ClassDB::bind_method(D_METHOD("get_clip_count"), &VFXAnimator::get_clip_count);
    ClassDB::bind_method(D_METHOD("get_clip_name", "idx"), &VFXAnimator::get_clip_name);
    ClassDB::bind_method(D_METHOD("get_clip_duration", "idx"), &VFXAnimator::get_clip_duration);
    ClassDB::bind_method(D_METHOD("set_clip_loop", "idx", "loop"), &VFXAnimator::set_clip_loop);
    ClassDB::bind_method(D_METHOD("get_clip_loop", "idx"), &VFXAnimator::get_clip_loop);
    ClassDB::bind_method(D_METHOD("set_clip_loop_mode", "idx", "mode"), &VFXAnimator::set_clip_loop_mode);
    ClassDB::bind_method(D_METHOD("get_clip_loop_mode", "idx"), &VFXAnimator::get_clip_loop_mode);

    ClassDB::bind_method(D_METHOD("add_curve", "clip_idx", "name", "bone_id", "is_rotation", "is_scale"), &VFXAnimator::add_curve);
    ClassDB::bind_method(D_METHOD("delete_curve", "clip_idx", "curve_idx"), &VFXAnimator::delete_curve);
    ClassDB::bind_method(D_METHOD("get_curve_count", "clip_idx"), &VFXAnimator::get_curve_count);
    ClassDB::bind_method(D_METHOD("get_curve_name", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_name);
    ClassDB::bind_method(D_METHOD("get_curve_bone_id", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_bone_id);
    ClassDB::bind_method(D_METHOD("get_curve_is_rotation", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_is_rotation);

    ClassDB::bind_method(D_METHOD("add_keyframe_scalar", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_scalar);
    ClassDB::bind_method(D_METHOD("add_keyframe_vector", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_vector);
    ClassDB::bind_method(D_METHOD("add_keyframe_quaternion", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_quaternion);
    ClassDB::bind_method(D_METHOD("delete_keyframe", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::delete_keyframe);
    ClassDB::bind_method(D_METHOD("get_keyframe_count", "clip_idx", "curve_idx"), &VFXAnimator::get_keyframe_count);
    ClassDB::bind_method(D_METHOD("get_keyframe_time", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_time);

    ClassDB::bind_method(D_METHOD("set_keyframe_scalar_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_scalar_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_vector_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_vector_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_quaternion_value", "clip_idx", "curve_idx", "key_idx", "value"), &VFXAnimator::set_keyframe_quaternion_value);
    ClassDB::bind_method(D_METHOD("set_keyframe_time", "clip_idx", "curve_idx", "key_idx", "time"), &VFXAnimator::set_keyframe_time);

    ClassDB::bind_method(D_METHOD("set_keyframe_tangent_mode", "clip_idx", "curve_idx", "key_idx", "mode"), &VFXAnimator::set_keyframe_tangent_mode);
    ClassDB::bind_method(D_METHOD("get_keyframe_tangent_mode", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_tangent_mode);
    ClassDB::bind_method(D_METHOD("set_keyframe_scalar_tangents", "clip_idx", "curve_idx", "key_idx", "in_tan", "out_tan"), &VFXAnimator::set_keyframe_scalar_tangents);
    ClassDB::bind_method(D_METHOD("set_keyframe_vector_tangents", "clip_idx", "curve_idx", "key_idx", "in_tan", "out_tan"), &VFXAnimator::set_keyframe_vector_tangents);
    ClassDB::bind_method(D_METHOD("auto_compute_tangents", "clip_idx", "curve_idx"), &VFXAnimator::auto_compute_tangents);

    ClassDB::bind_method(D_METHOD("play", "clip_idx"), &VFXAnimator::play);
    ClassDB::bind_method(D_METHOD("pause"), &VFXAnimator::pause);
    ClassDB::bind_method(D_METHOD("stop"), &VFXAnimator::stop);
    ClassDB::bind_method(D_METHOD("seek", "time"), &VFXAnimator::seek);
    ClassDB::bind_method(D_METHOD("advance", "delta"), &VFXAnimator::advance);
    ClassDB::bind_method(D_METHOD("is_clip_playing"), &VFXAnimator::is_clip_playing);
    ClassDB::bind_method(D_METHOD("get_playback_time"), &VFXAnimator::get_playback_time);

    ClassDB::bind_method(D_METHOD("sample_scalar", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_scalar);
    ClassDB::bind_method(D_METHOD("sample_vector", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_vector);
    ClassDB::bind_method(D_METHOD("sample_quaternion", "clip_idx", "curve_idx", "time"), &VFXAnimator::sample_quaternion);
    ClassDB::bind_method(D_METHOD("sample_pose", "clip_idx", "time", "bone_count"), &VFXAnimator::sample_pose);

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
float VFXAnimator::get_clip_duration(int idx) const {
    if (idx >= 0 && idx < (int)clips.size()) return clips[idx].duration;
    return 0.0f;
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

bool VFXAnimator::get_curve_is_rotation(int clip_idx, int curve_idx) const {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return false;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return false;
    return clips[clip_idx].curves[curve_idx].is_rotation;
}

void VFXAnimator::add_keyframe_scalar(int clip_idx, int curve_idx, float time, float value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    if (clips[clip_idx].curves[curve_idx].keys.empty()) {
        clips[clip_idx].curves[curve_idx].is_scalar = true;
    }
    Keyframe k;
    k.time = time;
    k.value = value;
    k.interp = interp;
    clips[clip_idx].curves[curve_idx].keys.push_back(k);
    clips[clip_idx].curves[curve_idx].sort_keys();
}

void VFXAnimator::add_keyframe_vector(int clip_idx, int curve_idx, float time, const Vector3& value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    clips[clip_idx].curves[curve_idx].is_scalar = false;
    Keyframe k;
    k.time = time;
    k.vec_value = value;
    k.interp = interp;
    clips[clip_idx].curves[curve_idx].keys.push_back(k);
    clips[clip_idx].curves[curve_idx].sort_keys();
}

void VFXAnimator::add_keyframe_quaternion(int clip_idx, int curve_idx, float time, const Quaternion& value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    clips[clip_idx].curves[curve_idx].is_scalar = false;
    Keyframe k;
    k.time = time;
    k.quat_value = value;
    k.interp = interp;
    clips[clip_idx].curves[curve_idx].keys.push_back(k);
    clips[clip_idx].curves[curve_idx].sort_keys();
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
}

void VFXAnimator::set_keyframe_quaternion_value(int clip_idx, int curve_idx, int key_idx, const Quaternion& value) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].quat_value = value;
}

void VFXAnimator::set_keyframe_time(int clip_idx, int curve_idx, int key_idx, float time) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].time = time;
    clips[clip_idx].curves[curve_idx].sort_keys();
}

void VFXAnimator::set_keyframe_tangent_mode(int clip_idx, int curve_idx, int key_idx, int mode) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    auto& keys = clips[clip_idx].curves[curve_idx].keys;
    if (key_idx < 0 || key_idx >= (int)keys.size()) return;
    keys[key_idx].tangent_mode = mode;
    if (mode == TANGENT_AUTO) {
        clips[clip_idx].curves[curve_idx].auto_compute_tangents();
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

void VFXAnimator::auto_compute_tangents(int clip_idx, int curve_idx) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
    clips[clip_idx].curves[curve_idx].auto_compute_tangents();
}

void VFXAnimator::play(int clip_idx) {
    if (clip_idx >= 0 && clip_idx < (int)clips.size()) {
        current_clip = clip_idx;
        current_time = 0.0f;
        is_playing = true;
    }
}

void VFXAnimator::pause() { is_playing = false; }
void VFXAnimator::stop() { is_playing = false; current_time = 0.0f; }

void VFXAnimator::seek(float time) {
    if (current_clip < 0 || current_clip >= (int)clips.size()) return;
    const AnimationClip& clip = clips[current_clip];
    switch (clip.loop_mode) {
        case LOOP_CLAMP:
            current_time = fmin(fmax(time, 0.0f), clip.duration);
            break;
        case LOOP_PING_PONG: {
            float d = clip.duration * 2.0f;
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
    seek(current_time + delta);
}

bool VFXAnimator::is_clip_playing() const { return is_playing; }
float VFXAnimator::get_playback_time() const { return current_time; }

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
        } else {
            Vector3 p = curve.sample_vector(time);
            pose[base + 0] = p.x;
            pose[base + 1] = p.y;
            pose[base + 2] = p.z;
        }
    }
    return pose;
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
            Quaternion q = curve.sample_quaternion(t);
            Basis b(q);
            Vector3 euler = b.get_euler();
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
                Basis b(key.quat_value);
                Vector3 euler = b.get_euler();
                euler.x = -euler.x;
                euler.z = -euler.z;
                key.quat_value = Basis::from_euler(euler).get_rotation_quaternion();
            } else if (!new_curve.is_scale) {
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

PackedByteArray VFXAnimator::serialize_clip(int idx) const {
    PackedByteArray data;
    if (idx < 0 || idx >= (int)clips.size()) return data;

    const AnimationClip& clip = clips[idx];

    // Magic "VFXA" + version 1
    data.append('V'); data.append('F'); data.append('X'); data.append('A');
    data.append(1);

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
    for (int i = 0; i < name_utf8.size(); i++) data.append(name_utf8[i]);

    append_f32(clip.duration);
    append_f32(clip.fps);
    data.append(clip.loop ? 1 : 0);
    data.append((uint8_t)clip.loop_mode);

    append_u32((uint32_t)clip.curves.size());

    for (const auto& curve : clip.curves) {
        CharString cname_utf8 = curve.name.utf8();
        append_u16((uint16_t)cname_utf8.size());
        for (int i = 0; i < cname_utf8.size(); i++) data.append(cname_utf8[i]);

        append_i32(curve.bone_id);
        uint8_t flags = (curve.is_rotation ? 1 : 0) | (curve.is_scale ? 2 : 0) | (curve.is_scalar ? 4 : 0);
        data.append(flags);

        append_u32((uint32_t)curve.keys.size());

        for (const auto& key : curve.keys) {
            append_f32(key.time);

            uint8_t val_type = 0; // scalar
            if (!curve.is_scalar) {
                val_type = curve.is_rotation ? 2 : 1; // quaternion : vector
            }
            data.append(val_type);

            if (val_type == 0) {
                append_f32(key.value);
            } else if (val_type == 1) {
                append_f32(key.vec_value.x);
                append_f32(key.vec_value.y);
                append_f32(key.vec_value.z);
            } else {
                append_f32(key.quat_value.x);
                append_f32(key.quat_value.y);
                append_f32(key.quat_value.z);
                append_f32(key.quat_value.w);
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
        }
    }

    return data;
}

void VFXAnimator::deserialize_clip(const PackedByteArray& data) {
    if (data.size() < 5) return;
    if (data[0] != 'V' || data[1] != 'F' || data[2] != 'X' || data[3] != 'A') return;
    if (data[4] != 1) return;

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
        if (p >= data.size()) return;
        uint8_t flags = data[p++];
        curve.is_rotation = (flags & 1) != 0;
        curve.is_scale = (flags & 2) != 0;
        curve.is_scalar = (flags & 4) != 0;

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
                key.quat_value.x = read_f32();
                key.quat_value.y = read_f32();
                key.quat_value.z = read_f32();
                key.quat_value.w = read_f32();
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

        // Extract bone name from NodePath subnames (e.g. Skeleton3D:bone_name/property)
        String bone_name;
        if (path.get_subname_count() > 0) {
            bone_name = path.get_subname(0);
        } else if (path.get_name_count() > 0) {
            bone_name = path.get_name(path.get_name_count() - 1);
        }

        // Try to parse bone index directly from subname (e.g. "bones/0/position")
        int bone_id = -1;
        if (bone_name.is_valid_int()) {
            bone_id = bone_name.to_int();
        } else {
            // Strip common property suffixes and try integer fallback
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
            // Infer from last subname or path string
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
            continue; // skip other track types
        }

        String curve_name = bone_name + (is_rotation ? "_rot" : (is_scale ? "_scale" : "_pos"));
        int curve_idx = add_curve(clip_idx, curve_name, bone_id, is_rotation, is_scale);

        int key_count = anim->track_get_key_count(t);
        for (int k = 0; k < key_count; k++) {
            float time = (float)anim->track_get_key_time(t, k);

            if (is_position) {
                Vector3 pos;
                if (type == Animation::TYPE_POSITION_3D) {
                    pos = anim->position_track_get_key(t, k);
                } else {
                    Variant v = anim->track_get_key_value(t, k);
                    if (v.get_type() == Variant::VECTOR3) pos = (Vector3)v;
                    else continue;
                }
                add_keyframe_vector(clip_idx, curve_idx, time, pos, INTERP_LINEAR);
            }
            else if (is_rotation) {
                Quaternion rot;
                if (type == Animation::TYPE_ROTATION_3D) {
                    rot = anim->rotation_track_get_key(t, k);
                } else {
                    Variant v = anim->track_get_key_value(t, k);
                    if (v.get_type() == Variant::QUATERNION) rot = (Quaternion)v;
                    else if (v.get_type() == Variant::BASIS) rot = ((Basis)v).get_rotation_quaternion();
                    else continue;
                }
                add_keyframe_quaternion(clip_idx, curve_idx, time, rot, INTERP_LINEAR);
            }
            else if (is_scale) {
                Vector3 scl;
                if (type == Animation::TYPE_SCALE_3D) {
                    scl = anim->scale_track_get_key(t, k);
                } else {
                    Variant v = anim->track_get_key_value(t, k);
                    if (v.get_type() == Variant::VECTOR3) scl = (Vector3)v;
                    else continue;
                }
                add_keyframe_vector(clip_idx, curve_idx, time, scl, INTERP_LINEAR);
            }
        }
    }
}
