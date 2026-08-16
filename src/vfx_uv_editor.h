#ifndef VFX_UV_EDITOR_H
#define VFX_UV_EDITOR_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <vector>
#include <unordered_map>
#include "vfx_mesh.h"

using namespace godot;

class VFXUVEditor : public RefCounted {
    GDCLASS(VFXUVEditor, RefCounted)

public:
    enum UVSelectMode {
        UV_SELECT_VERTEX = 0,
        UV_SELECT_EDGE = 1,
        UV_SELECT_FACE = 2,
        UV_SELECT_ISLAND = 3
    };

private:
    Ref<VFXMesh> mesh;
    int active_layer = 0;

    std::vector<bool> sel_verts;
    std::vector<bool> sel_faces;

    std::vector<int> island_map;
    std::vector<Rect2> island_bounds;
    bool islands_dirty = true;

    void _ensure_selection_size();
    void _rebuild_islands();
    void _deselect_all();

    static float _point_segment_dist(const Vector2& p, const Vector2& a, const Vector2& b);
    static bool _point_in_polygon(const Vector2& p, const PackedVector2Array& poly);

protected:
    static void _bind_methods();

public:
    VFXUVEditor();
    ~VFXUVEditor();

    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;

    void set_active_layer(int layer);
    int get_active_layer() const;

    int get_uv_vert_count() const;
    Vector2 get_uv_vert(int uv_idx) const;
    void set_uv_vert(int uv_idx, const Vector2& uv);

    PackedVector2Array get_face_uv_polygon(int face_idx) const;
    PackedInt32Array get_face_uv_indices(int face_idx) const;
    PackedInt32Array get_face_uv_edges(int face_idx) const;
    PackedInt32Array get_uv_vert_linked_faces(int uv_idx) const;

    void set_select_mode(int mode);
    int get_select_mode() const;

    void select_all();
    void deselect_all();
    void invert_selection();

    void select_at(const Vector2& uv_pos, float radius, bool add);
    void select_edge_at(const Vector2& uv_pos, float radius, bool add);
    void select_face_at(const Vector2& uv_pos, bool add);
    void select_island_at(const Vector2& uv_pos);

    void select_box(const Rect2& box, bool add);
    void select_island_box(const Rect2& box, bool add);

    void select_island(int island_idx);
    void deselect_island(int island_idx);

    PackedInt32Array get_selected_verts() const;
    PackedInt32Array get_selected_faces() const;
    bool is_uv_selected(int uv_idx) const;
    bool is_face_selected(int face_idx) const;

    void translate_selected(const Vector2& delta);
    void rotate_selected(float angle_rad, const Vector2& pivot);
    void scale_selected(const Vector2& scale, const Vector2& pivot);
    void mirror_selected_horizontal(float pivot_x = 0.5f);
    void mirror_selected_vertical(float pivot_y = 0.5f);

    void project_planar(const Vector3& normal);
    void project_cylindrical(const Vector3& axis);
    void project_spherical(const Vector3& axis);
    void project_box();

    void unwrap_selected_faces();
    void unwrap_island(int island_idx);

    void split_selected_verts();
    void weld_selected_verts();
    void stitch_selected();

    int get_island_count() const;
    PackedInt32Array get_island_faces(int island_idx) const;
    Rect2 get_island_bounds(int island_idx) const;
    void pack_islands(float padding = 0.01f);

    void align_selected_left();
    void align_selected_right();
    void align_selected_top();
    void align_selected_bottom();
    void align_selected_center_horizontal();
    void align_selected_center_vertical();
    void distribute_selected_horizontal();
    void distribute_selected_vertical();

    Rect2 get_selection_bounds() const;
    Rect2 get_total_bounds() const;
    void normalize_uvs();
    void snap_selected_to_grid(float grid_size);
    void copy_layer(int from_layer, int to_layer);

    Color get_uv_vert_color(int uv_idx) const;
    Color get_face_wire_color(int face_idx) const;
};

VARIANT_ENUM_CAST(VFXUVEditor::UVSelectMode);

#endif
