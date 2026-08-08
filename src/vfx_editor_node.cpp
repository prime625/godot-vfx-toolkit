#include "vfx_editor_node.h"
#include "vfx_gltf_exporter.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void VFXEditorNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_vfx_mesh", "mesh"), &VFXEditorNode::set_vfx_mesh);
    ClassDB::bind_method(D_METHOD("get_vfx_mesh"), &VFXEditorNode::get_vfx_mesh);
    ClassDB::bind_method(D_METHOD("refresh_mesh"), &VFXEditorNode::refresh_mesh);

    ClassDB::bind_method(D_METHOD("set_vfx_skeleton", "sk"), &VFXEditorNode::set_vfx_skeleton);
    ClassDB::bind_method(D_METHOD("get_vfx_skeleton"), &VFXEditorNode::get_vfx_skeleton);
    ClassDB::bind_method(D_METHOD("create_mixamo_skeleton"), &VFXEditorNode::create_mixamo_skeleton);

    ClassDB::bind_method(D_METHOD("set_vfx_skin", "skin"), &VFXEditorNode::set_vfx_skin);
    ClassDB::bind_method(D_METHOD("get_vfx_skin"), &VFXEditorNode::get_vfx_skin);
    ClassDB::bind_method(D_METHOD("auto_weight"), &VFXEditorNode::auto_weight);

    ClassDB::bind_method(D_METHOD("set_vfx_animator", "anim"), &VFXEditorNode::set_vfx_animator);
    ClassDB::bind_method(D_METHOD("get_vfx_animator"), &VFXEditorNode::get_vfx_animator);

    ClassDB::bind_method(D_METHOD("set_show_skeleton", "show"), &VFXEditorNode::set_show_skeleton);
    ClassDB::bind_method(D_METHOD("get_show_skeleton"), &VFXEditorNode::get_show_skeleton);
    ClassDB::bind_method(D_METHOD("set_show_weights", "show"), &VFXEditorNode::set_show_weights);
    ClassDB::bind_method(D_METHOD("get_show_weights"), &VFXEditorNode::get_show_weights);
    ClassDB::bind_method(D_METHOD("set_visualize_bone", "idx"), &VFXEditorNode::set_visualize_bone);
    ClassDB::bind_method(D_METHOD("get_visualize_bone"), &VFXEditorNode::get_visualize_bone);
    ClassDB::bind_method(D_METHOD("set_auto_update", "auto_up"), &VFXEditorNode::set_auto_update);
    ClassDB::bind_method(D_METHOD("get_auto_update"), &VFXEditorNode::get_auto_update);

    ClassDB::bind_method(D_METHOD("set_brush_cursor", "world_pos", "radius"), &VFXEditorNode::set_brush_cursor);
    ClassDB::bind_method(D_METHOD("clear_brush_cursor"), &VFXEditorNode::clear_brush_cursor);
    ClassDB::bind_method(D_METHOD("raycast_mesh", "ray_origin", "ray_dir", "max_dist"), &VFXEditorNode::raycast_mesh);

    ClassDB::bind_method(D_METHOD("export_glb", "filepath"), &VFXEditorNode::export_glb);
    ClassDB::bind_method(D_METHOD("export_glb_animated", "filepath", "clip_idx"), &VFXEditorNode::export_glb_animated);
    ClassDB::bind_method(D_METHOD("export_vat", "filepath", "frame_count", "fps"), &VFXEditorNode::export_vat);

    ClassDB::bind_method(D_METHOD("create_demo_cube"), &VFXEditorNode::create_demo_cube);
    ClassDB::bind_method(D_METHOD("create_demo_character"), &VFXEditorNode::create_demo_character);
}

VFXEditorNode::VFXEditorNode() {
    base_material.instantiate();
    base_material->set_albedo(Color(0.8f, 0.8f, 0.8f, 1.0f));
    base_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);

    weight_material.instantiate();
    weight_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    weight_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
}

VFXEditorNode::~VFXEditorNode() {}

void VFXEditorNode::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        _ensure_mesh_instance();
        _ensure_brush_cursor();
    }
    if (p_what == NOTIFICATION_PROCESS) {
        if (animator.is_valid() && animator->is_clip_playing()) {
            animator->advance(get_process_delta_time());
        }
        if (show_skeleton && skeleton.is_valid()) {
            _draw_skeleton_gizmos();
        }
    }
}

void VFXEditorNode::_ensure_mesh_instance() {
    if (!mesh_instance) {
        mesh_instance = memnew(MeshInstance3D);
        add_child(mesh_instance);
        mesh_instance->set_owner(this);
    }
}

void VFXEditorNode::_ensure_brush_cursor() {
    if (!brush_cursor) {
        brush_cursor = memnew(MeshInstance3D);
        Ref<SphereMesh> sm;
        sm.instantiate();
        sm->set_radius(1.0f);
        sm->set_height(2.0f);
        brush_cursor->set_mesh(sm);

        Ref<StandardMaterial3D> mat;
        mat.instantiate();
        mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
        mat->set_albedo(Color(0.2f, 0.8f, 1.0f, 0.3f));
        mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
        mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
        brush_cursor->set_material_override(mat);
        brush_cursor->set_visible(false);

        add_child(brush_cursor);
        brush_cursor->set_owner(this);
    }
}

void VFXEditorNode::_update_godot_mesh() {
    _ensure_mesh_instance();
    if (mesh.is_null()) return;

    Ref<Mesh> gm = mesh->to_godot_mesh();
    if (gm.is_valid()) {
        if (show_weights && skin.is_valid()) {
            Ref<ArrayMesh> am = gm;
            if (am.is_valid() && am->get_surface_count() > 0) {
                PackedColorArray colors = skin->get_weight_visualization(visualize_bone);
                Array arrays = am->surface_get_arrays(0);
                if (colors.size() == arrays[Mesh::ARRAY_VERTEX].operator PackedVector3Array().size()) {
                    arrays[Mesh::ARRAY_COLOR] = colors;
                    am->clear_surfaces();
                    am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
                }
            }
            // FIX: always assign weight_material when in weight mode
            mesh_instance->set_surface_override_material(0, weight_material);
        } else {
            mesh_instance->set_surface_override_material(0, base_material);
        }
        mesh_instance->set_mesh(gm);
    }
}

void VFXEditorNode::_draw_skeleton_gizmos() {
    if (skeleton.is_null()) return;
}

void VFXEditorNode::set_vfx_mesh(const Ref<VFXMesh>& p_mesh) {
    mesh = p_mesh;
    if (skin.is_valid()) skin->set_mesh(mesh);
    if (auto_update) _update_godot_mesh();
}

Ref<VFXMesh> VFXEditorNode::get_vfx_mesh() const { return mesh; }

void VFXEditorNode::refresh_mesh() {
    _update_godot_mesh();
}

void VFXEditorNode::set_vfx_skeleton(const Ref<VFXSkeleton>& p_sk) {
    skeleton = p_sk;
    if (skin.is_valid()) skin->set_skeleton(skeleton);
}

Ref<VFXSkeleton> VFXEditorNode::get_vfx_skeleton() const { return skeleton; }

void VFXEditorNode::create_mixamo_skeleton() {
    Ref<VFXSkeleton> sk;
    sk.instantiate();
    sk->create_mixamo_skeleton();
    set_vfx_skeleton(sk);
}

void VFXEditorNode::set_vfx_skin(const Ref<VFXSkin>& p_skin) {
    skin = p_skin;
    if (mesh.is_valid()) skin->set_mesh(mesh);
    if (skeleton.is_valid()) skin->set_skeleton(skeleton);
    if (skin.is_valid()) skin->set_mesh_transform(get_global_transform());
}

Ref<VFXSkin> VFXEditorNode::get_vfx_skin() const { return skin; }

void VFXEditorNode::auto_weight() {
    if (skin.is_null()) {
        skin.instantiate();
        skin->set_mesh(mesh);
        skin->set_skeleton(skeleton);
    }
    skin->auto_weight_from_bones(4);
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::set_vfx_animator(const Ref<VFXAnimator>& p_anim) {
    animator = p_anim;
}

Ref<VFXAnimator> VFXEditorNode::get_vfx_animator() const { return animator; }

void VFXEditorNode::set_show_skeleton(bool show) { show_skeleton = show; }
bool VFXEditorNode::get_show_skeleton() const { return show_skeleton; }
void VFXEditorNode::set_show_weights(bool show) { show_weights = show; if (auto_update) _update_godot_mesh(); }
bool VFXEditorNode::get_show_weights() const { return show_weights; }
void VFXEditorNode::set_visualize_bone(int idx) { visualize_bone = idx; if (show_weights && auto_update) _update_godot_mesh(); }
int VFXEditorNode::get_visualize_bone() const { return visualize_bone; }
void VFXEditorNode::set_auto_update(bool auto_up) { auto_update = auto_up; }
bool VFXEditorNode::get_auto_update() const { return auto_update; }

void VFXEditorNode::set_brush_cursor(const Vector3& world_pos, float radius) {
    _ensure_brush_cursor();
    if (brush_cursor) {
        brush_cursor->set_visible(true);
        brush_cursor->set_position(world_pos);
        brush_cursor->set_scale(Vector3(radius, radius, radius));
    }
}

void VFXEditorNode::clear_brush_cursor() {
    if (brush_cursor) brush_cursor->set_visible(false);
}

Variant VFXEditorNode::raycast_mesh(const Vector3& ray_origin, const Vector3& ray_dir, float max_dist) {
    if (mesh.is_null()) return Variant();
    Transform3D inv = get_global_transform().affine_inverse();
    Vector3 local_origin = inv.xform(ray_origin);
    Vector3 local_dir = inv.basis.xform(ray_dir).normalized();
    Vector3 hit;
    if (mesh->raycast(local_origin, local_dir, hit, max_dist)) {
        return hit;
    }
    return Variant();
}

bool VFXEditorNode::export_glb(const String& filepath) {
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_glb(mesh, skeleton, filepath);
}

bool VFXEditorNode::export_glb_animated(const String& filepath, int clip_idx) {
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_glb_animated(mesh, skeleton, animator, clip_idx, filepath);
}

bool VFXEditorNode::export_vat(const String& filepath, int frame_count, float fps) {
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_vat_glb(mesh, skin, frame_count, fps, filepath);
}

void VFXEditorNode::create_demo_cube() {
    Ref<VFXMesh> m;
    m.instantiate();

    float s = 0.5f;
    int v[8];
    v[0] = m->add_vertex(Vector3(-s, -s, -s), Vector2(0, 0));
    v[1] = m->add_vertex(Vector3( s, -s, -s), Vector2(1, 0));
    v[2] = m->add_vertex(Vector3( s,  s, -s), Vector2(1, 1));
    v[3] = m->add_vertex(Vector3(-s,  s, -s), Vector2(0, 1));
    v[4] = m->add_vertex(Vector3(-s, -s,  s), Vector2(0, 0));
    v[5] = m->add_vertex(Vector3( s, -s,  s), Vector2(1, 0));
    v[6] = m->add_vertex(Vector3( s,  s,  s), Vector2(1, 1));
    v[7] = m->add_vertex(Vector3(-s,  s,  s), Vector2(0, 1));

    m->add_triangle(v[0], v[1], v[2]);
    m->add_triangle(v[0], v[2], v[3]);
    m->add_triangle(v[5], v[4], v[7]);
    m->add_triangle(v[5], v[7], v[6]);
    m->add_triangle(v[3], v[2], v[6]);
    m->add_triangle(v[3], v[6], v[7]);
    m->add_triangle(v[4], v[5], v[1]);
    m->add_triangle(v[4], v[1], v[0]);
    m->add_triangle(v[1], v[5], v[6]);
    m->add_triangle(v[1], v[6], v[2]);
    m->add_triangle(v[4], v[0], v[3]);
    m->add_triangle(v[4], v[3], v[7]);

    m->recalculate_normals();
    m->link_twins();
    set_vfx_mesh(m);
}

void VFXEditorNode::create_demo_character() {
    create_demo_cube();
    create_mixamo_skeleton();
    auto_weight();

    Ref<VFXAnimator> anim;
    anim.instantiate();
    int clip = anim->create_clip("idle", 2.0f, 30.0f);

    int hips_curve = anim->add_curve(clip, "hips_pos", 0, false, false);
    anim->add_keyframe_vector(clip, hips_curve, 0.0f, Vector3(0, 1.0f, 0), VFXAnimator::INTERP_LINEAR);
    anim->add_keyframe_vector(clip, hips_curve, 1.0f, Vector3(0, 1.05f, 0), VFXAnimator::INTERP_LINEAR);
    anim->add_keyframe_vector(clip, hips_curve, 2.0f, Vector3(0, 1.0f, 0), VFXAnimator::INTERP_LINEAR);

    set_vfx_animator(anim);
}
