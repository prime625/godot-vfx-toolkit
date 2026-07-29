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

    if (k0.interp == VFXAnimator::INTERP_STEP) return k0.vec_value;
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

void VFXAnimator::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_clip", "name", "duration", "fps"), &VFXAnimator::create_clip);
    ClassDB::bind_method(D_METHOD("delete_clip", "idx"), &VFXAnimator::delete_clip);
    ClassDB::bind_method(D_METHOD("get_clip_count"), &VFXAnimator::get_clip_count);
    ClassDB::bind_method(D_METHOD("get_clip_name", "idx"), &VFXAnimator::get_clip_name);
    ClassDB::bind_method(D_METHOD("get_clip_duration", "idx"), &VFXAnimator::get_clip_duration);
    ClassDB::bind_method(D_METHOD("set_clip_loop", "idx", "loop"), &VFXAnimator::set_clip_loop);
    ClassDB::bind_method(D_METHOD("get_clip_loop", "idx"), &VFXAnimator::get_clip_loop);

    ClassDB::bind_method(D_METHOD("add_curve", "clip_idx", "name", "bone_id", "is_rotation", "is_scale"), &VFXAnimator::add_curve);
    ClassDB::bind_method(D_METHOD("delete_curve", "clip_idx", "curve_idx"), &VFXAnimator::delete_curve);
    ClassDB::bind_method(D_METHOD("get_curve_count", "clip_idx"), &VFXAnimator::get_curve_count);
    ClassDB::bind_method(D_METHOD("get_curve_name", "clip_idx", "curve_idx"), &VFXAnimator::get_curve_name);

    ClassDB::bind_method(D_METHOD("add_keyframe_scalar", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_scalar);
    ClassDB::bind_method(D_METHOD("add_keyframe_vector", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_vector);
    ClassDB::bind_method(D_METHOD("add_keyframe_quaternion", "clip_idx", "curve_idx", "time", "value", "interp"), &VFXAnimator::add_keyframe_quaternion);
    ClassDB::bind_method(D_METHOD("delete_keyframe", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::delete_keyframe);
    ClassDB::bind_method(D_METHOD("get_keyframe_count", "clip_idx", "curve_idx"), &VFXAnimator::get_keyframe_count);
    ClassDB::bind_method(D_METHOD("get_keyframe_time", "clip_idx", "curve_idx", "key_idx"), &VFXAnimator::get_keyframe_time);

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

    ClassDB::bind_method(D_METHOD("serialize_clip", "idx"), &VFXAnimator::serialize_clip);
    ClassDB::bind_method(D_METHOD("deserialize_clip", "data"), &VFXAnimator::deserialize_clip);

    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_LINEAR", INTERP_LINEAR);
    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_BEZIER", INTERP_BEZIER);
    ClassDB::bind_integer_constant(get_class_static(), "", "INTERP_STEP", INTERP_STEP);
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

void VFXAnimator::add_keyframe_scalar(int clip_idx, int curve_idx, float time, float value, int interp) {
    if (clip_idx < 0 || clip_idx >= (int)clips.size()) return;
    if (curve_idx < 0 || curve_idx >= (int)clips[clip_idx].curves.size()) return;
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
    current_time = time;
    if (clips[current_clip].loop) {
        current_time = fmod(current_time, clips[current_clip].duration);
        if (current_time < 0) current_time += clips[current_clip].duration;
    } else {
        current_time = fmin(fmax(current_time, 0.0f), clips[current_clip].duration);
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

PackedByteArray VFXAnimator::serialize_clip(int idx) const {
    PackedByteArray data;
    data.append(0x56); data.append(0x46); data.append(0x58); data.append(0x41);
    return data;
}

void VFXAnimator::deserialize_clip(const PackedByteArray& data) {
    // TODO
}

void VFXAnimator::from_godot_animation(const Object* anim, int bone_count) {
    // TODO: Import from Godot Animation resource
}
