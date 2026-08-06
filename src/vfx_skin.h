#ifndef VFX_SKIN_H
#define VFX_SKIN_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <vector>
#include <unordered_map>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"

using namespace godot;

// Brush operation modes
enum class VFXBrushMode {
    ADD = 0,
    SUBTRACT = 1,
    REPLACE = 2,
    SMOOTH = 3,
    BLUR = 4
};

// Spatial hash grid for fast radius queries on mobile
struct SpatialGrid {
    float cell_size = 0.0f;
    std::unordered_map<int64_t, std::vector<int>> cells;
    std::vector<Vector3> positions;

    void build(const std::vector<Vector3>& verts, float cell_sz);
    void clear();
    void query_sphere(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_dists) const;

private:
    static inline int64_t hash(int x, int y, int z) {
        // Simple 3D hash key
        int64_t h = ((int64_t)(x) * 73856093) ^ ((int64_t)(y) * 19349663) ^ ((int64_t)(z) * 83492791);
        return h;
    }
};

// One stroke = one undo step. Stores pre-state of touched vertices only.
struct WeightStroke {
    std::vector<int> vertices;
    std::vector<int> old_bones[4];   // per-vertex, 4 bones
    std::vector<float> old_weights[4];
};

class VFXSkin : public RefCounted {
    GDCLASS(VFXSkin, RefCounted)

private:
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;

    // Brush
    float brush_radius = 0.15f;
    float brush_strength = 1.0f;
    int brush_falloff = 1;      // 0=linear, 1=smooth, 2=spherical
    int brush_bone = 0;
    int brush_mode = 0;         // VFXBrushMode
    bool brush_normalize = true;

    // Mirror
    bool mirror_x = false;
    float mirror_plane_x = 0.0f;

    // Bone locking (locked bones cannot be painted)
    std::vector<bool> bone_locked;

    // Spatial acceleration
    SpatialGrid grid;
    bool grid_dirty = true;
    void _rebuild_grid();

    // Undo / Redo
    std::vector<WeightStroke> undo_stack;
    std::vector<WeightStroke> redo_stack;
    static constexpr size_t MAX_UNDO = 32;

    void _push_undo(const WeightStroke& stroke);
    float _brush_falloff(float dist, float radius, int type) const;
    void _find_vertices_brush(const Vector3& center, float radius, std::vector<int>& out_indices, std::vector<float>& out_dists);
    void _apply_weight(int vidx, float delta, int bone);
    void _apply_replace(int vidx, float target, float falloff, int bone);
    void _apply_smooth(int vidx, float strength);
    void _apply_mirror(int vidx, const int bones[4], const float weights[4]);

protected:
    static void _bind_methods();

public:
    VFXSkin();
    ~VFXSkin();

    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;
    void set_skeleton(const Ref<VFXSkeleton>& p_sk);
    Ref<VFXSkeleton> get_skeleton() const;

    // Brush
    void set_brush_radius(float r);
    float get_brush_radius() const;
    void set_brush_strength(float s);
    float get_brush_strength() const;
    void set_brush_falloff(int type);
    int get_brush_falloff() const;
    void set_brush_bone(int idx);
    int get_brush_bone() const;
    void set_brush_mode(int mode);
    int get_brush_mode() const;
    void set_brush_normalize(bool n);
    bool get_brush_normalize() const;

    // Mirror
    void set_mirror_x(bool enabled);
    bool get_mirror_x() const;
    void set_mirror_plane_x(float x);
    float get_mirror_plane_x() const;

    // Bone locking
    void set_bone_locked(int bone_idx, bool locked);
    bool get_bone_locked(int bone_idx) const;

    // Painting
    void stroke_begin();
    void paint_at(const Vector3& world_pos, float delta_time);
    void stroke_end();

    // Undo / Redo
    bool can_undo() const;
    bool can_redo() const;
    void undo();
    void redo();
    void clear_history();

    // Bulk ops
    void flood_fill_bone(int bone_idx, float weight);
    void auto_weight_from_bones(int max_bones_per_vertex = 4);
    void clear_weights();

    // Queries
    float get_vertex_bone_weight(int vidx, int bone_idx) const;
    PackedColorArray get_weight_visualization(int bone_idx) const;

    // Skinning preview
    PackedVector3Array compute_skinned_positions() const;
    PackedFloat32Array bake_vertex_animation(int frame_count, float fps) const;
};

#endif
