#ifndef VFX_FALLOFF_H
#define VFX_FALLOFF_H

#include <godot_cpp/variant/vector3.hpp>

namespace vfx {

enum ProportionalFalloff {
    FALLOFF_SMOOTH = 0,
    FALLOFF_SPHERE = 1,
    FALLOFF_ROOT = 2,
    FALLOFF_INVERSE_SQUARE = 3,
    FALLOFF_SHARP = 4,
    FALLOFF_LINEAR = 5,
    FALLOFF_CONSTANT = 6,
    FALLOFF_RANDOM = 7,
    FALLOFF_COUNT = 8
};

inline float evaluate_falloff(float t, int mode) {
    // t = distance / radius, expected in [0, 1]
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

    switch (mode) {
        case FALLOFF_SMOOTH:
            // Cosine falloff — gentle S-curve, most natural
            return cosf(t * 1.57079637f); // cos(t * PI/2)

        case FALLOFF_SPHERE:
            // Hemisphere / circular profile
            return sqrtf(1.0f - t * t);

        case FALLOFF_ROOT:
            // Quick influence at center, long tail
            return 1.0f - sqrtf(t);

        case FALLOFF_INVERSE_SQUARE:
            // Parabolic — strong near center, drops off fast
            return 1.0f - t * t;

        case FALLOFF_SHARP:
            // Very strong near center, cliff-like drop
            return (1.0f - t) * (1.0f - t);

        case FALLOFF_LINEAR:
            // Straight line — predictable, uniform
            return 1.0f - t;

        case FALLOFF_CONSTANT:
            // Everything inside radius moves equally
            return 1.0f;

        case FALLOFF_RANDOM:
            // Noisy falloff — organic, jittery deformation
            // Use a simple hash of t to keep it stable per stroke
            {
                unsigned int h = *(unsigned int*)&t;
                h = (h ^ 61) ^ (h >> 16);
                h = h + (h << 3);
                h = h ^ (h >> 4);
                h = h * 0x27d4eb2d;
                h = h ^ (h >> 15);
                float noise = (h & 0x7FFFFFFF) / 2147483647.0f;
                return (1.0f - t) * noise;
            }

        default:
            return 1.0f - t;
    }
}

} // namespace vfx

#endif
