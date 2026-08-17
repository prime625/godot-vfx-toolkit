#ifndef VFX_EDITOR_NODE_H
#define VFX_EDITOR_NODE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <vector>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_curve.h"
#include "vfx_texture_painter.h"

using namespace godot;

class VFXEditorNode : public Node3D {
    GDCLASS(VFXEditorNode, Node3D)

public:
    enum GizmoMode {
        GIZMO_TRANSLATE = 0,
        GIZMO_ROTATE = 1,
        GIZMO_SCALE = 2
    };

    enum GizmoAxis {
        GIZMO_NONE = -1,
        GIZMO_X = 0,
        GIZMO_Y,
        GIZMO_Z,
        GIZMO_XY,
        GIZMO_XZ,
        GIZMO_YZ,
        GIZMO_XYZ
    };

    enum EditMode {
        MODE_OBJECT = 0,
        MODE_VERTEX = 1,
        MODE_EDGE = 2,
        MODE_FACE = 3
    };

private:
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;
    Ref<VFXSkin> skin;
    Ref<VFXAnimator> animator;
    Ref<VFXCurve> active_curve;
    Ref<VFXTexturePainter> painter;

    MeshInstance3D* mesh_instance = nullptr;
    MeshInstance3D* brush_cursor = nullptr;

    Ref<StandardMaterial3D> base_material;
    Ref<StandardMaterial3D> weight_material;

    bool show_skeleton = true;
    bool show_weights = false;
    int visualize_bone = 0;
    bool auto_update = true;

    // === SYMMETRY ===
    bool symmetry_enabled = false;
    int symmetry_axis = 0; // 0=X, 1=Y, 2=Z

    // === EDIT MODE & SELECTION ===
    int edit_mode = MODE_OBJECT;
    int selected_face = -1;
    int selected_edge = -1;
    int selected_vertex = -1;
    bool show_wireframe = true;
    MeshInstance3D* selection_visual = nullptr;

    void _ensure_selection_visual();
    void _build_selection_mesh();
    void _update_gizmo_for_selection();

    // Mesh gizmo drag state
    std::vector<int> mesh_edit_verts;
    std::vector<Vector3> mesh_edit_initial_positions;

    // === GIZMO STATE ===
    int gizmo_mode = GIZMO_TRANSLATE;
    int gizmo_hover_axis = GIZMO_NONE;
    int gizmo_drag_axis = GIZMO_NONE;
    Transform3D gizmo_transform;
    float gizmo_screen_scale = 0.5f;

    Vector3 gizmo_drag_start_point;
    Transform3D gizmo_drag_start_transform;
    Plane gizmo_drag_plane;
    Quaternion gizmo_drag_start_rotation;
    Vector3    gizmo_drag_start_scale;

    MeshInstance3D* gizmo_node = nullptr;

    void _ensure_gizmo_node();
    void _build_gizmo_mesh();
    void _update_gizmo_visibility();
    Transform3D _get_visual_gizmo_transform() const;

    // === SKELETON VISUAL ===
    int selected_bone = -1;
    MeshInstance3D* skel_visual = nullptr;

    void _ensure_skeleton_visual();
    void _build_skeleton_mesh();

    // === MATH HELPERS ===
    bool _ray_vs_segment(const Vector3& ro, const Vector3& rd,
                         const Vector3& a, const Vector3& b,
                         float radius, float& out_t) const;
    bool _ray_vs_plane(const Vector3& ro, const Vector3& rd,
                       const Plane& p, Vector3& out_hit) const;

    void _ensure_mesh_instance();
    void _ensure_brush_cursor();
    void _update_godot_mesh();

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    VFXEditorNode();
    ~VFXEditorNode();

    void set_vfx_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_vfx_mesh() const;
    void refresh_mesh();

    void set_vfx_skeleton(const Ref<VFXSkeleton>& p_sk);
    Ref<VFXSkeleton> get_vfx_skeleton() const;
    void create_mixamo_skeleton();

    void set_vfx_skin(const Ref<VFXSkin>& p_skin);
    Ref<VFXSkin> get_vfx_skin() const;
    void auto_weight();

    void set_vfx_animator(const Ref<VFXAnimator>& p_anim);
    Ref<VFXAnimator> get_vfx_animator() const;

    void set_show_skeleton(bool show);
    bool get_show_skeleton() const;
    void set_show_weights(bool show);
    bool get_show_weights() const;
    void set_visualize_bone(int idx);
    int get_visualize_bone() const;
    void set_auto_update(bool auto_up);
    bool get_auto_update() const;

    void set_brush_cursor(const Vector3& world_pos, float radius);
    void clear_brush_cursor();

    Variant raycast_mesh(const Vector3& ray_origin, const Vector3& ray_dir, float max_dist = 1e20f);

    bool export_glb(const String& filepath);
    bool export_glb_animated(const String& filepath, int clip_idx);
    bool export_vat(const String& filepath, int frame_count, float fps);

    void create_demo_cube();
    void create_demo_character();

    // === EDIT MODE ===
    void set_edit_mode(int mode);
    int get_edit_mode() const;
    void set_show_wireframe(bool show);
    bool get_show_wireframe() const;
    void clear_selection();
    int get_selected_face() const;
    int get_selected_edge() const;
    int get_selected_vertex() const;
    int raycast_select(const Vector3& ray_origin, const Vector3& ray_dir);

    // === MODELING (legacy face-0) ===
    void extrude_selected_face(float distance);
    void inset_selected_face(float amount);
    void delete_selected_face();
    void subdivide_selected_face();
    void flip_normals();
    void mesh_cleanup();

    // === MODELING (selection-aware) ===
    void extrude_selection(float distance);
    void inset_selection(float amount);
    void delete_selection();
    void subdivide_selection();
    void bevel_selection(float amount);
    void knife_selection(const Vector3& p0, const Vector3& p1);

    // === CURVE ===
    void create_curve_tube(const PackedVector3Array& points, float radius, int segments, int rings);
    void create_curve_ribbon(const PackedVector3Array& points, float width, int segments);
    void set_active_curve(const Ref<VFXCurve>& curve);
    Ref<VFXCurve> get_active_curve() const;
    void curve_to_mesh(float radius, int segments, int rings);

    // === TEXTURE PAINTER ===
    void set_texture_painter(const Ref<VFXTexturePainter>& p_painter);
    Ref<VFXTexturePainter> get_texture_painter() const;
    void paint_at(const Vector3& ray_origin, const Vector3& ray_dir);

    void set_gizmo_mode(int mode);
    int get_gizmo_mode() const;
    void set_gizmo_transform(const Transform3D& t);
    Transform3D get_gizmo_transform() const;
    int raycast_gizmo(const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_end_drag();
    bool is_gizmo_dragging() const;

    void set_selected_bone(int idx);
    int get_selected_bone() const;
    int raycast_bone(const Vector3& ray_origin, const Vector3& ray_dir) const;

    int on_touch_down(const Vector3& ray_origin, const Vector3& ray_dir);
    void on_touch_up();
    void on_touch_drag(const Vector3& ray_origin, const Vector3& ray_dir);

    // === SYMMETRY ===
    void set_symmetry_enabled(bool enabled);
    bool get_symmetry_enabled() const;
    void set_symmetry_axis(int axis);
    int get_symmetry_axis() const;
};

VARIANT_ENUM_CAST(VFXEditorNode::GizmoMode);
VARIANT_ENUM_CAST(VFXEditorNode::GizmoAxis);
VARIANT_ENUM_CAST(VFXEditorNode::EditMode);

#endif