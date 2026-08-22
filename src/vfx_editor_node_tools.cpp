#include "vfx_editor_node.h"
#include "vfx_gltf_exporter.h"
#include "vfx_texture_painter.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ============================================================================
// EXPORT
// ============================================================================
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

// ============================================================================
// DEMO
// ============================================================================
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

// ============================================================================
// MODELING (legacy face-0)
// ============================================================================
void VFXEditorNode::extrude_selected_face(float distance) {
    if (mesh.is_null()) return;
    mesh->extrude_face(0, distance);
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::inset_selected_face(float amount) {
    if (mesh.is_null()) return;
    mesh->inset_face(0, amount);
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::delete_selected_face() {
    if (mesh.is_null()) return;
    mesh->delete_face(0);
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::subdivide_selected_face() {
    if (mesh.is_null()) return;
    mesh->subdivide_face(0);
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::flip_normals() {
    if (mesh.is_null()) return;
    mesh->flip_all_normals();
    if (auto_update) _update_godot_mesh();
}

void VFXEditorNode::mesh_cleanup() {
    if (mesh.is_null()) return;
    mesh->cleanup();
    if (auto_update) _update_godot_mesh();
}

// ============================================================================
// MODELING (selection-aware)
// ============================================================================
void VFXEditorNode::extrude_selection(float distance) {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->extrude_face(selected_face, distance);
        selected_face = -1;
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->bevel_edge(selected_edge, distance);
        selected_edge = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::inset_selection(float amount) {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->inset_face(selected_face, amount);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::delete_selection() {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->delete_face(selected_face);
        selected_face = -1;
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->dissolve_edge(selected_edge);
        selected_edge = -1;
    } else if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        mesh->dissolve_vertex(selected_vertex);
        selected_vertex = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::subdivide_selection() {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_FACE && selected_face >= 0) {
        mesh->subdivide_face(selected_face);
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::bevel_selection(float amount) {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        mesh->bevel_edge(selected_edge, amount);
        selected_edge = -1;
    } else if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        mesh->bevel_vertex(selected_vertex, amount);
        selected_vertex = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

void VFXEditorNode::knife_selection(const Vector3& p0, const Vector3& p1) {
    if (mesh.is_null()) return;
    if (edit_mode == MODE_FACE && selected_face >= 0) {
        Transform3D inv = get_global_transform().affine_inverse();
        mesh->knife_cut_face(selected_face, inv.xform(p0), inv.xform(p1));
        selected_face = -1;
    }
    if (auto_update) _update_godot_mesh();
    _build_selection_mesh();
    _update_gizmo_for_selection();
}

// ============================================================================
// CURVE
// ============================================================================
void VFXEditorNode::create_curve_tube(const PackedVector3Array& points, float radius, int segments, int rings) {
    Ref<VFXCurve> curve;
    curve.instantiate();
    for (int i = 0; i < points.size(); i++) {
        curve->add_point(points[i]);
    }
    Ref<VFXMesh> m = curve->to_tube_mesh(radius, segments, rings);
    set_vfx_mesh(m);
}

void VFXEditorNode::create_curve_ribbon(const PackedVector3Array& points, float width, int segments) {
    Ref<VFXCurve> curve;
    curve.instantiate();
    for (int i = 0; i < points.size(); i++) {
        curve->add_point(points[i]);
    }
    Ref<VFXMesh> m = curve->to_ribbon_mesh(width, segments);
    set_vfx_mesh(m);
}

void VFXEditorNode::set_active_curve(const Ref<VFXCurve>& curve) {
    active_curve = curve;
}

Ref<VFXCurve> VFXEditorNode::get_active_curve() const {
    return active_curve;
}

void VFXEditorNode::curve_to_mesh(float radius, int segments, int rings) {
    if (active_curve.is_null()) return;
    Ref<VFXMesh> m = active_curve->to_tube_mesh(radius, segments, rings);
    set_vfx_mesh(m);
}

// ============================================================================
// TEXTURE PAINTER
// ============================================================================
void VFXEditorNode::set_texture_painter(const Ref<VFXTexturePainter>& p_painter) {
    painter = p_painter;
    if (painter.is_valid() && mesh.is_valid()) {
        painter->set_mesh(mesh);
    }
}

Ref<VFXTexturePainter> VFXEditorNode::get_texture_painter() const {
    return painter;
}

void VFXEditorNode::paint_at(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (painter.is_valid()) {
        painter->paint_at(ray_origin, ray_dir);
    }
}