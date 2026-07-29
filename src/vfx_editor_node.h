#ifndef VFX_EDITOR_NODE_H
#define VFX_EDITOR_NODE_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
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
    Ref<StandardMaterial3D> base_material;
    Ref<StandardMaterial3D> weight_material;

    bool show_skeleton = false;
    bool show_weights = false;
    int visualize_bone = 0;
    bool auto_update = true;

    void _ensure_mesh_instance();
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

    bool export_glb(const String& filepath);
    bool export_glb_animated(const String& filepath, int clip_idx);
    bool export_vat(const String& filepath, int frame_count, float fps);

    void create_demo_cube();
    void create_demo_character();
};

#endif
