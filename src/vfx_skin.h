#ifndef VFX_SKIN_H
#define VFX_SKIN_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <vector>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"

using namespace godot;

// Weight painting brush settings
struct BrushSettings {
    float radius = 0.1f;
    float strength = 1.0f;
    float falloff = 1.0f;  // 0=linear, 1=smooth, 2=spherical
    int bone_index = 0;
    bool add_mode = true;  // true=add, false=subtract
    bool normalize = true;
};

class VFXSkin : public RefCounted {
    GDCLASS(VFXSkin, RefCounted)

private:
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;

    BrushSettings brush;

    // Spatial acceleration for weight painting
    // Simple: iterate all vertices (good enough for <50k verts on mobile)
    // For larger meshes, switch to BVH later

    float _brush_falloff(float dist, float radius, int type) const;
    void _find_vertices_in_radius(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_distances) const;

protected:
    static void _bind_methods();

public:
    VFXSkin();
    ~VFXSkin();

    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;
    void set_skeleton(const Ref<VFXSkeleton>& p_sk);
    Ref<VFXSkeleton> get_skeleton() const;

    // Brush settings
    void set_brush_radius(float r);
    float get_brush_radius() const;
    void set_brush_strength(float s);
    float get_brush_strength() const;
    void set_brush_falloff(int type);
    int get_brush_falloff() const;
    void set_brush_bone(int idx);
    int get_brush_bone() const;
    void set_brush_add_mode(bool add);
    bool get_brush_add_mode() const;
    void set_brush_normalize(bool n);
    bool get_brush_normalize() const;

    // Paint weight at world position
    void paint_at(const Vector3& world_pos, float delta_time);

    // Flood fill bone weight
    void flood_fill_bone(int bone_idx, float weight);

    // Auto-weight from bone proximity (bind pose)
    void auto_weight_from_bones(int max_bones_per_vertex = 4);

    // Clear all weights
    void clear_weights();

    // Get weight of bone at vertex (for visualization)
    float get_vertex_bone_weight(int vidx, int bone_idx) const;

    // Generate vertex colors from bone weights (for debug viz)
    PackedColorArray get_weight_visualization(int bone_idx) const;

    // Compute skinned vertex positions (CPU skinning for preview)
    PackedVector3Array compute_skinned_positions() const;

    // VAT export data: [frame0_pos, frame0_normal, frame1_pos, ...]
    PackedFloat32Array bake_vertex_animation(int frame_count, float fps) const;
};

#endif
