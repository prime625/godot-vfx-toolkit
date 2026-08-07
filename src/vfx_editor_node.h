#ifndef VFX_EDITOR_NODE_H
#define VFX_EDITOR_NODE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"

using namespace godot;

class VFXEditorNode : public Node3D {
    GDCLASS(VFXEditorNode, Node3D)

private:
    Ref<VFXMesh> mesh;
    Ref<VFXSkeleton> skeleton;
    Ref<VFXSkin> skin;
    Ref<VFXAnimator> animator;

    MeshInstance3D* mesh_instance = nullptr;
    MeshInstance3D* brush_cursor = nullptr;

    Ref<StandardMaterial3D> base_material;
    Ref<StandardMaterial3D> weight_material;

    bool show_skeleton = true;
    bool show_weights = false;
    int visualize_bone = 0;
    bool auto_update = true;

    void _ensure_mesh_instance();
    void _ensure_brush_cursor();
    void _update_godot_mesh();
    void _draw_skeleton_gizmos();

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

    // Brush cursor
    void set_brush_cursor(const Vector3& world_pos, float radius);
    void clear_brush_cursor();

    // Raycast into mesh for painting
    bool raycast_mesh(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, float max_dist = 1e20f);

    bool export_glb(const String& filepath);
    bool export_glb_animated(const String& filepath, int clip_idx);
    bool export_vat(const String& filepath, int frame_count, float fps);

    void create_demo_cube();
    void create_demo_character();
};

#endif
