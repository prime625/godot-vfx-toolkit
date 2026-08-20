#ifndef VFX_EDITOR_NODE_H
#define VFX_EDITOR_NODE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <unordered_set>

#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_curve.h"
#include "vfx_texture_painter.h"
#include "vfx_scene.h"
#include "vfx_scene_node.h"

// Forward declare to avoid circular include in header
class VFXSceneTreePanel;

using namespace godot;

class VFXEditorNode : public Node3D {
    GDCLASS(VFXEditorNode, Node3D)

public:
    enum GizmoMode {
        GIZMO_TRANSLATE = 0,
        GIZMO_ROTATE = 1,
        GIZMO_SCALE = 2,
    };

    enum GizmoAxis {
        GIZMO_NONE = -1,
        GIZMO_X = 0,
        GIZMO_Y = 1,
        GIZMO_Z = 2,
    };

    enum EditMode {
        MODE_OBJECT = 0,
        MODE_VERTEX = 1,
        MODE_EDGE = 2,
        MODE_FACE = 3,
    };

    enum HitType {
        SCENE_NODE_HIT = -3,
    };

protected:
    static void _bind_methods();
    void _notification(int p_what);

private:
    // === COMPONENTS ===
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;
    Ref<VFXSkin> skin;
    Ref<VFXAnimator> animator;
    Ref<VFXCurve> active_curve;
    Ref<VFXTexturePainter> painter;
    Ref<VFXScene> scene;
    Ref<VFXSceneNode> active_scene_node;

    // === GODOT NODES ===
    MeshInstance3D* mesh_instance = nullptr;
    MeshInstance3D* brush_cursor = nullptr;
    MeshInstance3D* gizmo_node = nullptr;
    MeshInstance3D* selection_visual = nullptr;
    MeshInstance3D* skel_visual = nullptr;
    Node3D* scene_container = nullptr;
    VFXSceneTreePanel* scene_tree_panel = nullptr;
    Camera3D* camera = nullptr;

    // === MATERIALS ===
    Ref<StandardMaterial3D> base_material;
    Ref<StandardMaterial3D> weight_material;

    // === STATE ===
    bool show_skeleton = false;
    bool show_weights = false;
    bool auto_update = true;
    bool show_wireframe = false;
    bool symmetry_enabled = false;
    int visualize_bone = -1;
    int symmetry_axis = 0; // 0=x, 1=y, 2=z
    int edit_mode = MODE_OBJECT;
    int gizmo_mode = GIZMO_TRANSLATE;
    int gizmo_hover_axis = GIZMO_NONE;
    int selected_bone = -1;
    int selected_face = -1;
    int selected_edge = -1;
    int selected_vertex = -1;
    float select_pixel_tolerance = 8.0f;

    // === GIZMO DRAG STATE ===
    bool gizmo_dragging = false;
    int gizmo_drag_axis = GIZMO_NONE;
    Vector3 gizmo_drag_start_pos;
    Vector3 gizmo_drag_plane_normal;
    float gizmo_drag_plane_d = 0.0f;
    Transform3D gizmo_transform;
    Transform3D gizmo_drag_start_transform;
    Quaternion gizmo_drag_start_rotation;
    Vector3 gizmo_drag_start_scale;

    // === SCENE VISUALS ===
    HashMap<uint64_t, MeshInstance3D*> scene_visuals;
    bool scene_visuals_dirty = true;

    // === INTERNAL HELPERS ===
    void _ensure_mesh_instance();
    void _ensure_brush_cursor();
    void _ensure_gizmo_node();
    void _ensure_selection_visual();
    void _ensure_skeleton_visual();
    void _ensure_scene_container();

    void _update_godot_mesh();
    void _build_gizmo_mesh();
    void _build_skeleton_mesh();
    void _build_selection_mesh();
    void _update_gizmo_for_selection();
    void _update_gizmo_visibility();
    Transform3D _get_visual_gizmo_transform() const;

    // Scene tree integration
    void _on_scene_node_selected(Ref<VFXSceneNode> p_node);
    void mark_scene_dirty();

    // Scene visuals
    MeshInstance3D* _get_scene_visual(uint64_t node_id);
    void _clear_scene_visuals();
    Ref<ArrayMesh> _build_array_mesh_for_node(const Ref<VFXMesh>& p_mesh, const Ref<VFXSkeleton>& p_sk, const Ref<VFXSkin>& p_skin, bool p_show_weights, int p_viz_bone);
    void _sync_scene_visuals();
    void _sync_node_visual_recursive(const Ref<VFXSceneNode>& p_node, std::unordered_set<uint64_t>& r_used);

    // Gizmo raycast / drag
    float _ray_plane_intersect(const Vector3& ro, const Vector3& rd, const Vector3& pn, float pd) const;
    float _ray_sphere_intersect(const Vector3& ro, const Vector3& rd, const Vector3& sc, float sr) const;

    // Screen-space selection helpers
    float _point_segment_dist_sq_2d(const Vector2& p, const Vector2& a, const Vector2& b);
    bool _point_in_polygon_2d(const Vector2& p, const PackedVector2Array& poly);

public:
    VFXEditorNode();
    ~VFXEditorNode();

    // === MESH ===
    void set_vfx_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_vfx_mesh() const;
    void refresh_mesh();

    // === SKELETON ===
    void set_vfx_skeleton(const Ref<VFXSkeleton>& p_sk);
    Ref<VFXSkeleton> get_vfx_skeleton() const;
    void create_mixamo_skeleton();

    // === SKIN ===
    void set_vfx_skin(const Ref<VFXSkin>& p_skin);
    Ref<VFXSkin> get_vfx_skin() const;
    void auto_weight();

    // === ANIMATOR ===
    void set_vfx_animator(const Ref<VFXAnimator>& p_anim);
    Ref<VFXAnimator> get_vfx_animator() const;

    // === VISIBILITY ===
    void set_show_skeleton(bool show);
    bool get_show_skeleton() const;
    void set_show_weights(bool show);
    bool get_show_weights() const;
    void set_visualize_bone(int idx);
    int get_visualize_bone() const;
    void set_auto_update(bool auto_up);
    bool get_auto_update() const;

    // === BRUSH ===
    void set_brush_cursor(const Vector3& world_pos, float radius);
    void clear_brush_cursor();
    Variant raycast_mesh(const Vector3& ray_origin, const Vector3& ray_dir, float max_dist);

    // === EXPORT ===
    bool export_glb(const String& filepath);
    bool export_glb_animated(const String& filepath, int clip_idx);
    bool export_vat(const String& filepath, int frame_count, float fps);

    // === DEMO ===
    void create_demo_cube();
    void create_demo_character();

    // === MODELING ===
    void extrude_selected_face(float distance);
    void inset_selected_face(float amount);
    void delete_selected_face();
    void subdivide_selected_face();
    void flip_normals();
    void mesh_cleanup();

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

    // === GIZMO ===
    void set_gizmo_mode(int mode);
    int get_gizmo_mode() const;
    void set_gizmo_transform(const Transform3D& t);
    Transform3D get_gizmo_transform() const;
    int raycast_gizmo(const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir);
    void gizmo_end_drag();
    bool is_gizmo_dragging() const;

    // === BONE ===
    void set_selected_bone(int idx);
    int get_selected_bone() const;
    int raycast_bone(const Vector3& ray_origin, const Vector3& ray_dir) const;

    // === CAMERA ===
    void set_camera(Camera3D* p_camera);
    Camera3D* get_camera() const;
    void set_select_pixel_tolerance(float px);
    float get_select_pixel_tolerance() const;

    // === SCREEN SELECT ===
    int screen_select_vertex(const Vector2& screen_pos);
    int screen_select_edge(const Vector2& screen_pos);
    int screen_select_face(const Vector2& screen_pos);
    int screen_raycast_gizmo(const Vector2& screen_pos);

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

    // === TEXTURE PAINTER ===
    void set_texture_painter(const Ref<VFXTexturePainter>& p);
    Ref<VFXTexturePainter> get_texture_painter() const;
    void paint_at(const Vector3& ray_origin, const Vector3& ray_dir);

    // === SYMMETRY ===
    void set_symmetry_enabled(bool enabled);
    bool get_symmetry_enabled() const;
    void set_symmetry_axis(int axis);
    int get_symmetry_axis() const;

    // === SCENE ===
    void set_scene(const Ref<VFXScene>& p_scene);
    Ref<VFXScene> get_scene() const;
    void set_active_scene_node(const Ref<VFXSceneNode>& p_node);
    Ref<VFXSceneNode> get_active_scene_node() const;
    bool import_model(const String& filepath, const Ref<VFXSceneNode>& parent);
    Ref<VFXSceneNode> raycast_scene_node(const Vector3& ray_origin, const Vector3& ray_dir);

    // === UNIFIED TOUCH ===
    int on_touch_down(const Vector3& ray_origin, const Vector3& ray_dir, const Vector2& screen_pos);
    void on_touch_up();
    void on_touch_drag(const Vector3& ray_origin, const Vector3& ray_dir);
};

VARIANT_ENUM_CAST(VFXEditorNode::GizmoMode);
VARIANT_ENUM_CAST(VFXEditorNode::GizmoAxis);
VARIANT_ENUM_CAST(VFXEditorNode::EditMode);

#endif
