#include "vfx_editor_node.h"
#include "vfx_gltf_exporter.h"
#include "vfx_texture_painter.h"
#include "vfx_skeleton.h"
#include "vfx_mesh.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_curve.h"
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/immediate_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <algorithm>
#include <vector>

using namespace godot;

// ============================================================================
// STATIC MATH & GEOMETRY HELPERS
// ============================================================================
static bool _ray_vs_segment(const Vector3& ro, const Vector3& rd, const Vector3& p0, const Vector3& p1, float radius, float& out_t) {
    Vector3 u = rd.normalized();
    Vector3 v = p1 - p0;
    Vector3 w = ro - p0;
    float a = u.dot(u);
    float b = u.dot(v);
    float c = v.dot(v);
    float d = u.dot(w);
    float e = v.dot(w);
    float D = a * c - b * b;
    if (D < 0.00001f) return false;
    float sc = (b * e - c * d) / D;
    float tc = (a * e - b * d) / D;
    tc = vfx::clampf(tc, 0.0f, 1.0f);
    Vector3 dP = w + (sc * u) - (tc * v);
    if (dP.length_squared() <= radius * radius) {
        out_t = sc;
        return sc >= 0.0f;
    }
    return false;
}

static bool _ray_vs_plane(const Vector3& ro, const Vector3& rd, const Plane& pl, Vector3& out_hit) {
    float denom = pl.normal.dot(rd);
    if (fabs(denom) < 0.00001f) return false;
    float t = -(pl.normal.dot(ro) - pl.d) / denom;
    if (t < 0.0f) return false;
    out_hit = ro + rd * t;
    return true;
}

static void _append_cylinder(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
                             const Vector3& from, const Vector3& to, float radius, int segs, const Color& col) {
    Vector3 dir = to - from;
    float len = dir.length();
    if (len < 0.0001f) return;
    dir /= len;

    Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = dir.cross(up).normalized();
    up = right.cross(dir).normalized();

    int base = verts.size();
    for (int i = 0; i <= segs; i++) {
        float ang = (float)i / (float)segs * 3.14159265f * 2.0f;
        Vector3 off = right * cosf(ang) * radius + up * sinf(ang) * radius;
        verts.push_back(from + off);
        cols.push_back(col);
        verts.push_back(to + off);
        cols.push_back(col);
    }
    for (int i = 0; i < segs; i++) {
        int b = base + i * 2;
        idx.push_back(b);     idx.push_back(b + 2); idx.push_back(b + 1);
        idx.push_back(b + 1); idx.push_back(b + 2); idx.push_back(b + 3);
    }
}

static void _append_triangle(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
                             const Vector3& a, const Vector3& b, const Vector3& c, const Color& col) {
    int base = verts.size();
    verts.push_back(a); verts.push_back(b); verts.push_back(c);
    cols.push_back(col); cols.push_back(col); cols.push_back(col);
    idx.push_back(base); idx.push_back(base + 1); idx.push_back(base + 2);
}

static void _append_ring(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
                         const Vector3& center, const Vector3& normal, float radius, int segs, float tube_radius, const Color& col) {
    Vector3 up = fabs(normal.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = normal.cross(up).normalized();
    up = right.cross(normal).normalized();

    Vector3 prev_pos;
    bool has_prev = false;

    for (int i = 0; i <= segs; i++) {
        float ang = (float)i / (float)segs * 3.14159265f * 2.0f;
        Vector3 pos = center + right * cosf(ang) * radius + up * sinf(ang) * radius;
        if (has_prev) {
            _append_cylinder(verts, cols, idx, prev_pos, pos, tube_radius, 4, col);
        }
        prev_pos = pos;
        has_prev = true;
    }
}

static void _append_box(PackedVector3Array& verts, PackedColorArray& cols, PackedInt32Array& idx,
                        const Vector3& center, float size, const Color& col) {
    float h = size * 0.5f;
    int base = verts.size();

    verts.push_back(center + Vector3(-h, -h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3( h, -h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3( h,  h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h,  h, -h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h, -h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3( h, -h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3( h,  h,  h)); cols.push_back(col);
    verts.push_back(center + Vector3(-h,  h,  h)); cols.push_back(col);

    const int faces[12][3] = {
        {0,1,2}, {0,2,3}, {4,6,5}, {4,7,6},
        {0,5,1}, {0,4,5}, {2,7,3}, {2,6,7},
        {0,3,7}, {0,7,4}, {1,6,2}, {1,5,6}
    };
    for (int i = 0; i < 12; i++) {
        idx.push_back(base + faces[i][0]);
        idx.push_back(base + faces[i][1]);
        idx.push_back(base + faces[i][2]);
    }
}

// ============================================================================
// BINDINGS
// ============================================================================
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

    // MODELING
    ClassDB::bind_method(D_METHOD("extrude_selected_face", "distance"), &VFXEditorNode::extrude_selected_face);
    ClassDB::bind_method(D_METHOD("inset_selected_face", "amount"), &VFXEditorNode::inset_selected_face);
    ClassDB::bind_method(D_METHOD("delete_selected_face"), &VFXEditorNode::delete_selected_face);
    ClassDB::bind_method(D_METHOD("subdivide_selected_face"), &VFXEditorNode::subdivide_selected_face);
    ClassDB::bind_method(D_METHOD("flip_normals"), &VFXEditorNode::flip_normals);
    ClassDB::bind_method(D_METHOD("mesh_cleanup"), &VFXEditorNode::mesh_cleanup);

    // CURVE
    ClassDB::bind_method(D_METHOD("create_curve_tube", "points", "radius", "segments", "rings"), &VFXEditorNode::create_curve_tube);
    ClassDB::bind_method(D_METHOD("create_curve_ribbon", "points", "width", "segments"), &VFXEditorNode::create_curve_ribbon);
    ClassDB::bind_method(D_METHOD("set_active_curve", "curve"), &VFXEditorNode::set_active_curve);
    ClassDB::bind_method(D_METHOD("get_active_curve"), &VFXEditorNode::get_active_curve);
    ClassDB::bind_method(D_METHOD("curve_to_mesh", "radius", "segments", "rings"), &VFXEditorNode::curve_to_mesh);

    // GIZMO
    ClassDB::bind_method(D_METHOD("set_gizmo_mode", "mode"), &VFXEditorNode::set_gizmo_mode);
    ClassDB::bind_method(D_METHOD("get_gizmo_mode"), &VFXEditorNode::get_gizmo_mode);
    ClassDB::bind_method(D_METHOD("set_gizmo_transform", "t"), &VFXEditorNode::set_gizmo_transform);
    ClassDB::bind_method(D_METHOD("get_gizmo_transform"), &VFXEditorNode::get_gizmo_transform);
    ClassDB::bind_method(D_METHOD("raycast_gizmo", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_gizmo);
    ClassDB::bind_method(D_METHOD("gizmo_begin_drag", "axis", "ray_origin", "ray_dir"), &VFXEditorNode::gizmo_begin_drag);
    ClassDB::bind_method(D_METHOD("gizmo_drag", "ray_origin", "ray_dir"), &VFXEditorNode::gizmo_drag);
    ClassDB::bind_method(D_METHOD("gizmo_end_drag"), &VFXEditorNode::gizmo_end_drag);
    ClassDB::bind_method(D_METHOD("is_gizmo_dragging"), &VFXEditorNode::is_gizmo_dragging);

    ClassDB::bind_method(D_METHOD("set_selected_bone", "idx"), &VFXEditorNode::set_selected_bone);
    ClassDB::bind_method(D_METHOD("get_selected_bone"), &VFXEditorNode::get_selected_bone);
    ClassDB::bind_method(D_METHOD("raycast_bone", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_bone);

    ClassDB::bind_method(D_METHOD("on_touch_down", "ray_origin", "ray_dir"), &VFXEditorNode::on_touch_down);
    ClassDB::bind_method(D_METHOD("on_touch_up"), &VFXEditorNode::on_touch_up);
    ClassDB::bind_method(D_METHOD("on_touch_drag", "ray_origin", "ray_dir"), &VFXEditorNode::on_touch_drag);

    ClassDB::bind_integer_constant(get_class_static(), "", "GIZMO_TRANSLATE", GIZMO_TRANSLATE);
    ClassDB::bind_integer_constant(get_class_static(), "", "GIZMO_ROTATE", GIZMO_ROTATE);
    ClassDB::bind_integer_constant(get_class_static(), "", "GIZMO_SCALE", GIZMO_SCALE);

    // EDIT MODE
    ClassDB::bind_method(D_METHOD("set_edit_mode", "mode"), &VFXEditorNode::set_edit_mode);
    ClassDB::bind_method(D_METHOD("get_edit_mode"), &VFXEditorNode::get_edit_mode);
    ClassDB::bind_method(D_METHOD("set_show_wireframe", "show"), &VFXEditorNode::set_show_wireframe);
    ClassDB::bind_method(D_METHOD("get_show_wireframe"), &VFXEditorNode::get_show_wireframe);
    ClassDB::bind_method(D_METHOD("clear_selection"), &VFXEditorNode::clear_selection);
    ClassDB::bind_method(D_METHOD("get_selected_face"), &VFXEditorNode::get_selected_face);
    ClassDB::bind_method(D_METHOD("get_selected_edge"), &VFXEditorNode::get_selected_edge);
    ClassDB::bind_method(D_METHOD("get_selected_vertex"), &VFXEditorNode::get_selected_vertex);
    ClassDB::bind_method(D_METHOD("raycast_select", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_select);

    // MODELING (selection-aware)
    ClassDB::bind_method(D_METHOD("extrude_selection", "distance"), &VFXEditorNode::extrude_selection);
    ClassDB::bind_method(D_METHOD("inset_selection", "amount"), &VFXEditorNode::inset_selection);
    ClassDB::bind_method(D_METHOD("delete_selection"), &VFXEditorNode::delete_selection);
    ClassDB::bind_method(D_METHOD("subdivide_selection"), &VFXEditorNode::subdivide_selection);
    ClassDB::bind_method(D_METHOD("bevel_selection", "amount"), &VFXEditorNode::bevel_selection);
    ClassDB::bind_method(D_METHOD("knife_selection", "p0", "p1"), &VFXEditorNode::knife_selection);

    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_OBJECT", MODE_OBJECT);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_VERTEX", MODE_VERTEX);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_EDGE", MODE_EDGE);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_FACE", MODE_FACE);

    // TEXTURE PAINTER
    ClassDB::bind_method(D_METHOD("set_texture_painter", "p"), &VFXEditorNode::set_texture_painter);
    ClassDB::bind_method(D_METHOD("get_texture_painter"), &VFXEditorNode::get_texture_painter);
    ClassDB::bind_method(D_METHOD("paint_at", "ray_origin", "ray_dir"), &VFXEditorNode::paint_at);

    // SYMMETRY
    ClassDB::bind_method(D_METHOD("set_symmetry_enabled", "enabled"), &VFXEditorNode::set_symmetry_enabled);
    ClassDB::bind_method(D_METHOD("get_symmetry_enabled"), &VFXEditorNode::get_symmetry_enabled);
    ClassDB::bind_method(D_METHOD("set_symmetry_axis", "axis"), &VFXEditorNode::set_symmetry_axis);
    ClassDB::bind_method(D_METHOD("get_symmetry_axis"), &VFXEditorNode::get_symmetry_axis);
}

// ============================================================================
// LIFECYCLE
// ============================================================================
VFXEditorNode::VFXEditorNode() {
    base_material.instantiate();
    base_material->set_albedo(Color(0.8f, 0.8f, 0.8f, 1.0f));
    base_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
    base_material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    base_material->set_metallic(0.0f);
    base_material->set_roughness(0.5f);
    base_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, false);

    weight_material.instantiate();
    weight_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    weight_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
    weight_material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
}

VFXEditorNode::~VFXEditorNode() {}

void VFXEditorNode::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        _ensure_mesh_instance();
        _ensure_brush_cursor();
        _ensure_gizmo_node();
        _ensure_skeleton_visual();
        _ensure_selection_visual();
    }
    if (p_what == NOTIFICATION_PROCESS) {
        if (animator.is_valid() && animator->is_clip_playing()) {
            animator->advance(get_process_delta_time());
        }

        if (mesh.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0 && !show_weights) {
            _update_godot_mesh();
        }

        if (show_skeleton && skeleton.is_valid() && skeleton->get_bone_count() > 0 && is_visible_in_tree()) {
            _build_skeleton_mesh();
            if (skel_visual) skel_visual->set_visible(true);
        } else if (skel_visual) {
            skel_visual->set_visible(false);
        }

        if (mesh.is_valid() && show_wireframe && is_visible_in_tree()) {
            _build_selection_mesh();
            if (selection_visual) selection_visual->set_visible(true);
        } else if (selection_visual) {
            selection_visual->set_visible(false);
        }
    }
}

// ============================================================================
// MESH / CURSOR / VISUALS
// ============================================================================
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

void VFXEditorNode::_ensure_skeleton_visual() {
    if (!skel_visual) {
        skel_visual = memnew(MeshInstance3D);
        skel_visual->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        skel_visual->set_visible(false);
        add_child(skel_visual);
        skel_visual->set_owner(this);
    }
}

void VFXEditorNode::_build_skeleton_mesh() {
    if (!skel_visual) _ensure_skeleton_visual();
    if (skeleton.is_null() || skeleton->get_bone_count() == 0 || !show_skeleton || !is_visible_in_tree()) {
        skel_visual->set_mesh(Ref<Mesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    int bone_count = skeleton->get_bone_count();
    for (int i = 0; i < bone_count; i++) {
        Transform3D g_trans = skeleton->get_bone_global_transform(i);
        Vector3 pos = g_trans.get_origin();
        bool is_sel = (selected_bone == i);
        Color col = is_sel ? Color(1.0f, 0.8f, 0.0f) : Color(0.1f, 0.9f, 0.2f);

        _append_box(verts, cols, idx, pos, is_sel ? 0.06f : 0.04f, col);

        int parent = skeleton->get_bone_parent(i);
        if (parent >= 0) {
            Vector3 p_pos = skeleton->get_bone_global_transform(parent).get_origin();
            _append_cylinder(verts, cols, idx, p_pos, pos, 0.015f, 6, col);
        }
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    skel_visual->set_material_override(mat);
    skel_visual->set_mesh(am);
}

void VFXEditorNode::_update_godot_mesh() {
    _ensure_mesh_instance();
    if (mesh.is_null()) return;

    Ref<ArrayMesh> am;
    am.instantiate();

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    PackedVector3Array positions;
    if (skin.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0 && !show_weights) {
        skeleton->update_transforms();
        if (skin.is_valid()) skin->set_mesh_transform(get_global_transform());
        positions = skin->compute_skinned_positions();
    } else {
        positions = mesh->get_positions();
    }

    arrays[Mesh::ARRAY_VERTEX] = positions;

    PackedVector3Array normals = mesh->get_normals();
    for (int i = 0; i < normals.size(); i++) {
        if (normals[i].length_squared() < 0.0001f) normals[i] = Vector3(0, 1, 0);
        else normals[i] = normals[i].normalized();
    }
    arrays[Mesh::ARRAY_NORMAL] = normals;

    arrays[Mesh::ARRAY_TEX_UV] = mesh->get_uvs();
    arrays[Mesh::ARRAY_COLOR] = mesh->get_colors();
    arrays[Mesh::ARRAY_INDEX] = mesh->get_indices();

    if (!show_weights && skin.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
        // pre-skinned GPU bypass
    } else {
        int vc = positions.size();
        if (vc > 0) {
            PackedInt32Array bones;
            PackedFloat32Array weights;
            bones.resize(vc * 4);
            weights.resize(vc * 4);
            for (int i = 0; i < vc; i++) {
                int b[4];
                float w[4];
                mesh->get_vertex_skinning(i, b, w);
                for (int j = 0; j < 4; j++) {
                    bones[i * 4 + j] = b[j];
                    weights[i * 4 + j] = w[j];
                }
            }
            arrays[Mesh::ARRAY_BONES] = bones;
            arrays[Mesh::ARRAY_WEIGHTS] = weights;
        }
    }

    am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

    if (show_weights && skin.is_valid()) {
        if (am->get_surface_count() > 0) {
            PackedColorArray colors = skin->get_weight_visualization(visualize_bone);
            Array a = am->surface_get_arrays(0);
            PackedVector3Array verts = a[Mesh::ARRAY_VERTEX];
            if (colors.size() == verts.size()) {
                a[Mesh::ARRAY_COLOR] = colors;
                am->clear_surfaces();
                am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);
            }
        }
        mesh_instance->set_surface_override_material(0, weight_material);
    } else {
        mesh_instance->set_surface_override_material(0, base_material);
    }

    mesh_instance->set_mesh(am);
}

void VFXEditorNode::_ensure_selection_visual() {
    if (!selection_visual) {
        selection_visual = memnew(MeshInstance3D);
        selection_visual->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        add_child(selection_visual);
        selection_visual->set_owner(this);
    }
}

void VFXEditorNode::_build_selection_mesh() {
    if (!selection_visual) _ensure_selection_visual();
    if (mesh.is_null() || !show_wireframe || !is_visible_in_tree()) {
        selection_visual->set_mesh(Ref<Mesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool is_sel = (edit_mode == MODE_EDGE && selected_edge == (int)e->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.4f, 0.4f, 0.4f, 0.4f);
        float r = is_sel ? 0.012f : 0.004f;
        _append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        bool is_sel = (edit_mode == MODE_VERTEX && selected_vertex == (int)v->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.7f, 0.7f, 0.7f, 0.8f);
        float s = is_sel ? 0.035f : 0.018f;
        _append_box(verts, cols, idx, v->position, s, col);
    }

    for (auto* f : mesh->get_faces()) {
        if (f->deleted || !f->halfedge) continue;
        Vector3 c = mesh->get_face_center(f->id);
        bool is_sel = (edit_mode == MODE_FACE && selected_face == (int)f->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.3f, 0.6f, 1.0f, 0.6f);
        float s = is_sel ? 0.045f : 0.028f;
        _append_box(verts, cols, idx, c, s, col);
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    selection_visual->set_material_override(mat);
    selection_visual->set_mesh(am);
}

// ============================================================================
// GIZMO
// ============================================================================
Transform3D VFXEditorNode::_get_visual_gizmo_transform() const {
    if (gizmo_mode == GIZMO_SCALE) {
        return gizmo_transform;
    }
    Transform3D visual = gizmo_transform;
    Basis b = visual.get_basis();
    b.set_column(0, b.get_column(0).normalized());
    b.set_column(1, b.get_column(1).normalized());
    b.set_column(2, b.get_column(2).normalized());
    visual.set_basis(b);
    return visual;
}

void VFXEditorNode::_ensure_gizmo_node() {
    if (!gizmo_node) {
        gizmo_node = memnew(MeshInstance3D);
        gizmo_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        gizmo_node->set_visible(false);
        add_child(gizmo_node);
        gizmo_node->set_owner(this);
    }
}

void VFXEditorNode::_update_gizmo_visibility() {
    if (!gizmo_node) return;
    if (!is_visible_in_tree()) {
        gizmo_node->set_visible(false);
        return;
    }
    bool show = false;
    if (edit_mode == MODE_OBJECT) {
        show = (selected_bone >= 0 && show_skeleton);
    } else {
        show = (selected_vertex >= 0 || selected_edge >= 0 || selected_face >= 0);
    }
    gizmo_node->set_visible(show);
}

void VFXEditorNode::_build_gizmo_mesh() {
    if (!gizmo_node) _ensure_gizmo_node();

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    float s = gizmo_screen_scale;
    float r = s * 0.025f;

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);

        _append_cylinder(verts, cols, idx, Vector3(), Vector3(s, 0, 0), r, 8, cx);
        _append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s, 0), r, 8, cy);
        _append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s), r, 8, cz);

        float p1 = s * 0.18f;
        float p2 = s * 0.38f;

        Color cxy = (gizmo_hover_axis == GIZMO_XY) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(1.0f, 1.0f, 0.0f, 0.35f);
        _append_triangle(verts, cols, idx, Vector3(p1, p2, 0), Vector3(p2, p1, 0), Vector3(p1, p1, 0), cxy);

        Color cxz = (gizmo_hover_axis == GIZMO_XZ) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(1.0f, 0.0f, 1.0f, 0.35f);
        _append_triangle(verts, cols, idx, Vector3(p1, 0, p2), Vector3(p2, 0, p1), Vector3(p1, 0, p1), cxz);

        Color cyz = (gizmo_hover_axis == GIZMO_YZ) ? Color(1.0f, 1.0f, 0.0f, 0.8f) : Color(0.0f, 1.0f, 1.0f, 0.35f);
        _append_triangle(verts, cols, idx, Vector3(0, p1, p2), Vector3(0, p2, p1), Vector3(0, p1, p1), cyz);
    } else if (gizmo_mode == GIZMO_ROTATE) {
        float ring_r = s * 0.85f;
        float tube = s * 0.02f;

        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);

        _append_ring(verts, cols, idx, Vector3(), Vector3(1,0,0), ring_r, 32, tube, cx);
        _append_ring(verts, cols, idx, Vector3(), Vector3(0,1,0), ring_r, 32, tube, cy);
        _append_ring(verts, cols, idx, Vector3(), Vector3(0,0,1), ring_r, 32, tube, cz);
    } else if (gizmo_mode == GIZMO_SCALE) {
        Color cx = (gizmo_hover_axis == GIZMO_X) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 0.0f, 0.0f);
        Color cy = (gizmo_hover_axis == GIZMO_Y) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 1.0f, 0.0f);
        Color cz = (gizmo_hover_axis == GIZMO_Z) ? Color(1.0f, 1.0f, 0.0f) : Color(0.0f, 0.0f, 1.0f);
        Color cxyz = (gizmo_hover_axis == GIZMO_XYZ) ? Color(1.0f, 1.0f, 0.0f) : Color(1.0f, 1.0f, 1.0f, 0.8f);

        _append_cylinder(verts, cols, idx, Vector3(), Vector3(s, 0, 0), r, 8, cx);
        _append_cylinder(verts, cols, idx, Vector3(), Vector3(0, s, 0), r, 8, cy);
        _append_cylinder(verts, cols, idx, Vector3(), Vector3(0, 0, s), r, 8, cz);

        float box = s * 0.08f;
        _append_box(verts, cols, idx, Vector3(s, 0, 0), box, cx);
        _append_box(verts, cols, idx, Vector3(0, s, 0), box, cy);
        _append_box(verts, cols, idx, Vector3(0, 0, s), box, cz);
        _append_box(verts, cols, idx, Vector3(), box * 0.8f, cxyz);
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    gizmo_node->set_material_override(mat);
    gizmo_node->set_mesh(am);
    gizmo_node->set_transform(_get_visual_gizmo_transform());
}

void VFXEditorNode::set_gizmo_mode(int mode) {
    gizmo_mode = mode;
    gizmo_hover_axis = GIZMO_NONE;
    if (gizmo_node && gizmo_node->is_visible()) {
        _build_gizmo_mesh();
    }
}

int VFXEditorNode::get_gizmo_mode() const { return gizmo_mode; }

void VFXEditorNode::set_gizmo_transform(const Transform3D& t) {
    gizmo_transform = t;
    if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
}

Transform3D VFXEditorNode::get_gizmo_transform() const { return gizmo_transform; }

int VFXEditorNode::raycast_gizmo(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!gizmo_node || !gizmo_node->is_visible() || !is_visible_in_tree()) return GIZMO_NONE;

    Transform3D inv = _get_visual_gizmo_transform().affine_inverse();
    Vector3 ro = inv.xform(ray_origin);
    Vector3 rd = inv.basis.xform(ray_dir).normalized();

    float s = gizmo_screen_scale;
    float best_t = 1e20f;
    int best = GIZMO_NONE;

    if (gizmo_mode == GIZMO_TRANSLATE) {
        auto test_axis = [&](int axis, const Vector3& dir) {
            float t;
            if (_ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.18f, t)) {
                if (t < best_t) { best_t = t; best = axis; }
            }
        };

        test_axis(GIZMO_X, Vector3(1, 0, 0));
        test_axis(GIZMO_Y, Vector3(0, 1, 0));
        test_axis(GIZMO_Z, Vector3(0, 0, 1));

        auto test_plane_axis = [&](int axis, const Plane& pl, float c1min, float c1max, float c2min, float c2max, int c1_axis, int c2_axis) {
            Vector3 hit;
            if (_ray_vs_plane(ro, rd, pl, hit)) {
                float c1 = (c1_axis == 0) ? hit.x : (c1_axis == 1) ? hit.y : hit.z;
                float c2 = (c2_axis == 0) ? hit.x : (c2_axis == 1) ? hit.y : hit.z;
                if (c1 >= c1min && c1 <= c1max && c2 >= c2min && c2 <= c2max) {
                    float t = (hit - ro).dot(rd);
                    if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
                }
            }
        };

        float p1 = s * 0.08f;
        float p2 = s * 0.28f;
        test_plane_axis(GIZMO_XY, Plane(Vector3(0, 0, 1), 0), p1, p2, p1, p2, 0, 1);
        test_plane_axis(GIZMO_XZ, Plane(Vector3(0, 1, 0), 0), p1, p2, p1, p2, 0, 2);
        test_plane_axis(GIZMO_YZ, Plane(Vector3(1, 0, 0), 0), p1, p2, p1, p2, 1, 2);
    } else if (gizmo_mode == GIZMO_ROTATE) {
        auto test_ring = [&](int axis, const Vector3& normal, float radius) {
            Plane pl(normal, 0.0f);
            Vector3 hit;
            if (!_ray_vs_plane(ro, rd, pl, hit)) return;
            float dist = hit.length();
            float tol = s * 0.12f;
            if (fabs(dist - radius) < tol) {
                float t = (hit - ro).dot(rd);
                if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
            }
        };
        float ring_r = s * 0.85f;
        test_ring(GIZMO_X, Vector3(1,0,0), ring_r);
        test_ring(GIZMO_Y, Vector3(0,1,0), ring_r);
        test_ring(GIZMO_Z, Vector3(0,0,1), ring_r);
    } else if (gizmo_mode == GIZMO_SCALE) {
        auto test_axis = [&](int axis, const Vector3& dir) {
            float t;
            if (_ray_vs_segment(ro, rd, Vector3(), dir * s, s * 0.18f, t)) {
                if (t < best_t) { best_t = t; best = axis; }
            }
        };
        test_axis(GIZMO_X, Vector3(1, 0, 0));
        test_axis(GIZMO_Y, Vector3(0, 1, 0));
        test_axis(GIZMO_Z, Vector3(0, 0, 1));

        float box = s * 0.08f;
        float h = box * 0.5f;
        auto test_box = [&](int axis, const Vector3& center) {
            for (int f = 0; f < 6; f++) {
                Vector3 n;
                switch (f) {
                    case 0: n = Vector3( 1, 0, 0); break;
                    case 1: n = Vector3(-1, 0, 0); break;
                    case 2: n = Vector3(0,  1, 0); break;
                    case 3: n = Vector3(0, -1, 0); break;
                    case 4: n = Vector3(0, 0,  1); break;
                    case 5: n = Vector3(0, 0, -1); break;
                }
                Plane pl(n, -n.dot(center));
                Vector3 hit;
                if (!_ray_vs_plane(ro, rd, pl, hit)) continue;
                Vector3 local = hit - center;
                if (fabs(local.x) <= h && fabs(local.y) <= h && fabs(local.z) <= h) {
                    float t = (hit - ro).dot(rd);
                    if (t >= 0.0f && t < best_t) { best_t = t; best = axis; }
                }
            }
        };
        test_box(GIZMO_XYZ, Vector3());
    }

    if (best != gizmo_hover_axis) {
        gizmo_hover_axis = best;
        _build_gizmo_mesh();
    }
    return best;
}

void VFXEditorNode::gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir) {
    gizmo_drag_axis = axis;
    gizmo_drag_start_transform = gizmo_transform;

    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();
    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
            mesh_edit_verts.push_back(selected_vertex);
            mesh_edit_initial_positions.push_back(mesh->get_vertex_position(selected_vertex));
        } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
            int v0, v1;
            mesh->get_edge_endpoints(selected_edge, v0, v1);
            if (v0 >= 0) { mesh_edit_verts.push_back(v0); mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v0)); }
            if (v1 >= 0) { mesh_edit_verts.push_back(v1); mesh_edit_initial_positions.push_back(mesh->get_vertex_position(v1)); }
        } else if (edit_mode == MODE_FACE && selected_face >= 0) {
            std::vector<vfx::HEVertex*> verts;
            mesh->get_face_vertices(selected_face, verts);
            for (auto* v : verts) {
                mesh_edit_verts.push_back((int)v->id);
                mesh_edit_initial_positions.push_back(v->position);
            }
        }
    }

    Vector3 origin = gizmo_transform.get_origin();

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Vector3 normal;
        if (axis <= GIZMO_Z) {
            Vector3 cam_dir = ray_dir.normalized();
            Vector3 axis_vec = gizmo_transform.basis.get_column(axis).normalized();
            if (fabs(axis_vec.dot(cam_dir)) < 0.3f) {
                normal = cam_dir;
            } else {
                Vector3 alt = (fabs(axis_vec.dot(Vector3(0, 1, 0))) < 0.9f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
                normal = axis_vec.cross(alt).normalized();
                if (fabs(normal.dot(cam_dir)) < 0.1f) normal = cam_dir;
            }
        } else if (axis == GIZMO_XY) {
            normal = gizmo_transform.basis.xform(Vector3(0, 0, 1)).normalized();
        } else if (axis == GIZMO_XZ) {
            normal = gizmo_transform.basis.xform(Vector3(0, 1, 0)).normalized();
        } else {
            normal = gizmo_transform.basis.xform(Vector3(1, 0, 0)).normalized();
        }
        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit))
            gizmo_drag_start_point = hit;
    } else if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 normal;
        if (axis == GIZMO_X) normal = gizmo_transform.basis.xform(Vector3(1,0,0)).normalized();
        else if (axis == GIZMO_Y) normal = gizmo_transform.basis.xform(Vector3(0,1,0)).normalized();
        else normal = gizmo_transform.basis.xform(Vector3(0,0,1)).normalized();

        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
            gizmo_drag_start_point = hit;
            gizmo_drag_start_rotation = gizmo_transform.basis.get_rotation_quaternion();
        }
    } else if (gizmo_mode == GIZMO_SCALE) {
        Vector3 cam_dir = ray_dir.normalized();
        Vector3 normal = cam_dir;
        gizmo_drag_plane = Plane(normal, origin);
        Vector3 hit;
        if (_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
            gizmo_drag_start_point = hit;
            gizmo_drag_start_scale = gizmo_transform.basis.get_scale();
            if (axis <= GIZMO_Z) {
                Vector3 world_axis = gizmo_transform.basis.get_column(axis).normalized();
                gizmo_drag_start_point = Vector3((hit - origin).dot(world_axis), 0, 0);
            } else {
                gizmo_drag_start_point = Vector3((hit - origin).length(), 0, 0);
            }
        }
    }
}

void VFXEditorNode::gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (gizmo_drag_axis == GIZMO_NONE) return;
    Vector3 hit;
    if (!_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) return;

    Vector3 origin = gizmo_transform.get_origin();

    if (gizmo_mode == GIZMO_TRANSLATE) {
        Vector3 delta = hit - gizmo_drag_start_point;
        Transform3D t = gizmo_drag_start_transform;
        if (gizmo_drag_axis <= GIZMO_Z) {
            Vector3 axis = t.basis.get_column(gizmo_drag_axis).normalized();
            float proj = delta.dot(axis);
            t.set_origin(t.get_origin() + axis * proj);
        } else if (gizmo_drag_axis == GIZMO_XY || gizmo_drag_axis == GIZMO_XZ || gizmo_drag_axis == GIZMO_YZ) {
            t.set_origin(t.get_origin() + delta);
        }
        gizmo_transform = t;
    } else if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 v0 = (gizmo_drag_start_point - origin).normalized();
        Vector3 v1 = (hit - origin).normalized();
        if (v0.length_squared() < 0.0001f || v1.length_squared() < 0.0001f) return;
        float dot = v0.dot(v1);
        dot = vfx::clampf(dot, -1.0f, 1.0f);
        float angle = acosf(dot);
        Vector3 cross = v0.cross(v1);
        if (cross.dot(gizmo_drag_plane.normal) < 0.0f) angle = -angle;
        Vector3 axis_local;
        if (gizmo_drag_axis == GIZMO_X) axis_local = Vector3(1,0,0);
        else if (gizmo_drag_axis == GIZMO_Y) axis_local = Vector3(0,1,0);
        else axis_local = Vector3(0,0,1);
        Quaternion rot(axis_local, angle);
        Basis new_basis = Basis(rot) * gizmo_drag_start_transform.basis;
        gizmo_transform.set_basis(new_basis);
    } else if (gizmo_mode == GIZMO_SCALE) {
        Vector3 scale = gizmo_drag_start_scale;
        if (gizmo_drag_axis <= GIZMO_Z) {
            Vector3 world_axis = gizmo_drag_start_transform.basis.get_column(gizmo_drag_axis).normalized();
            float current_proj = (hit - origin).dot(world_axis);
            float start_proj = gizmo_drag_start_point.x;
            if (fabs(start_proj) > 0.0001f) {
                float ratio = current_proj / start_proj;
                ratio = vfx::clampf(ratio, 0.001f, 1000.0f);
                scale[gizmo_drag_axis] = gizmo_drag_start_scale[gizmo_drag_axis] * ratio;
            }
        } else if (gizmo_drag_axis == GIZMO_XYZ) {
            float current_dist = (hit - origin).length();
            float start_dist = gizmo_drag_start_point.x;
            if (start_dist > 0.0001f) {
                float ratio = current_dist / start_dist;
                ratio = vfx::clampf(ratio, 0.001f, 1000.0f);
                scale = gizmo_drag_start_scale * ratio;
            }
        }
        gizmo_transform.set_basis(gizmo_drag_start_transform.basis.scaled(scale));
    }

    if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());

    if (edit_mode == MODE_OBJECT && selected_bone >= 0 && skeleton.is_valid() && show_skeleton) {
        skeleton->set_bone_global_transform(selected_bone, gizmo_transform);
        _build_skeleton_mesh();
        _update_godot_mesh();
    } else if (edit_mode != MODE_OBJECT && mesh.is_valid() && !mesh_edit_verts.empty()) {
        Transform3D delta_t = gizmo_transform * gizmo_drag_start_transform.affine_inverse();
        for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
            Vector3 new_pos = delta_t.xform(mesh_edit_initial_positions[i]);
            mesh->set_vertex_position(mesh_edit_verts[i], new_pos);
        }
        refresh_mesh();
    }
}

void VFXEditorNode::gizmo_end_drag() {
    gizmo_drag_axis = GIZMO_NONE;
    mesh_edit_verts.clear();
    mesh_edit_initial_positions.clear();
}

bool VFXEditorNode::is_gizmo_dragging() const {
    return gizmo_drag_axis != GIZMO_NONE;
}

// ============================================================================
// SKELETON SELECTION & RAYCASTING (GUARDED WHEN HIDDEN)
// ============================================================================
void VFXEditorNode::set_selected_bone(int idx) {
    if (!show_skeleton && idx >= 0) {
        selected_bone = -1;
        _update_gizmo_visibility();
        return;
    }
    selected_bone = idx;
    if (selected_bone >= 0 && skeleton.is_valid()) {
        Transform3D t = skeleton->get_bone_global_transform(selected_bone);
        set_gizmo_transform(t);
    }
    _update_gizmo_visibility();
}

int VFXEditorNode::get_selected_bone() const { return selected_bone; }

int VFXEditorNode::raycast_bone(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!show_skeleton || !is_visible_in_tree() || skeleton.is_null() || skeleton->get_bone_count() == 0) {
        return -1;
    }

    float best_t = 1e20f;
    int best_bone = -1;

    int bone_count = skeleton->get_bone_count();
    for (int i = 0; i < bone_count; i++) {
        Vector3 pos = skeleton->get_bone_global_transform(i).get_origin();
        float t;
        if (_ray_vs_segment(ray_origin, ray_dir, pos, pos, 0.08f, t)) {
            if (t < best_t) {
                best_t = t;
                best_bone = i;
            }
        }
        int parent = skeleton->get_bone_parent(i);
        if (parent >= 0) {
            Vector3 p_pos = skeleton->get_bone_global_transform(parent).get_origin();
            if (_ray_vs_segment(ray_origin, ray_dir, p_pos, pos, 0.04f, t)) {
                if (t < best_t) {
                    best_t = t;
                    best_bone = i;
                }
            }
        }
    }

    return best_bone;
}

// ============================================================================
// TOUCH / INPUT INTERACTION
// ============================================================================
bool VFXEditorNode::on_touch_down(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!is_visible_in_tree()) return false;

    if (gizmo_node && gizmo_node->is_visible()) {
        int g_axis = raycast_gizmo(ray_origin, ray_dir);
        if (g_axis != GIZMO_NONE) {
            gizmo_begin_drag(g_axis, ray_origin, ray_dir);
            return true;
        }
    }

    if (show_skeleton && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
        int b_idx = raycast_bone(ray_origin, ray_dir);
        if (b_idx >= 0) {
            set_selected_bone(b_idx);
            return true;
        }
    }

    if (edit_mode != MODE_OBJECT) {
        return raycast_select(ray_origin, ray_dir);
    }

    return false;
}

void VFXEditorNode::on_touch_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (!is_visible_in_tree()) return;
    if (is_gizmo_dragging()) {
        gizmo_drag(ray_origin, ray_dir);
    }
}

void VFXEditorNode::on_touch_up() {
    gizmo_end_drag();
}

// ============================================================================
// SETTERS & GETTERS
// ============================================================================
void VFXEditorNode::set_vfx_mesh(const Ref<VFXMesh>& p_mesh) {
    mesh = p_mesh;
    refresh_mesh();
}
Ref<VFXMesh> VFXEditorNode::get_vfx_mesh() const { return mesh; }

void VFXEditorNode::refresh_mesh() {
    if (mesh.is_valid()) {
        mesh->recalculate_normals();
    }
    _update_godot_mesh();
    _build_selection_mesh();
}

void VFXEditorNode::set_vfx_skeleton(const Ref<VFXSkeleton>& p_skel) {
    skeleton = p_skel;
    if (skeleton.is_valid()) {
        _build_skeleton_mesh();
    }
}
Ref<VFXSkeleton> VFXEditorNode::get_vfx_skeleton() const { return skeleton; }

void VFXEditorNode::create_mixamo_skeleton() {
    skeleton.instantiate();
    skeleton->build_mixamo_rig();
    set_vfx_skeleton(skeleton);
}

void VFXEditorNode::set_vfx_skin(const Ref<VFXSkin>& p_skin) {
    skin = p_skin;
    if (skin.is_valid() && mesh.is_valid() && skeleton.is_valid()) {
        skin->setup(mesh, skeleton);
    }
}
Ref<VFXSkin> VFXEditorNode::get_vfx_skin() const { return skin; }

void VFXEditorNode::auto_weight() {
    if (skin.is_valid()) {
        skin->auto_weight_automatic();
        _update_godot_mesh();
    }
}

void VFXEditorNode::set_vfx_animator(const Ref<VFXAnimator>& p_anim) { animator = p_anim; }
Ref<VFXAnimator> VFXEditorNode::get_vfx_animator() const { return animator; }

void VFXEditorNode::set_show_skeleton(bool show) {
    show_skeleton = show;
    if (!show_skeleton) {
        selected_bone = -1;
        _update_gizmo_visibility();
    }
    if (skel_visual) {
        skel_visual->set_visible(show_skeleton);
    }
}
bool VFXEditorNode::get_show_skeleton() const { return show_skeleton; }

void VFXEditorNode::set_show_weights(bool show) {
    show_weights = show;
    _update_godot_mesh();
}
bool VFXEditorNode::get_show_weights() const { return show_weights; }

void VFXEditorNode::set_visualize_bone(int idx) {
    visualize_bone = idx;
    if (show_weights) _update_godot_mesh();
}
int VFXEditorNode::get_visualize_bone() const { return visualize_bone; }

void VFXEditorNode::set_auto_update(bool auto_up) { auto_update = auto_up; }
bool VFXEditorNode::get_auto_update() const { return auto_update; }

// ============================================================================
// BRUSH CURSOR & RAYCAST MESH
// ============================================================================
void VFXEditorNode::set_brush_cursor(const Vector3& world_pos, float radius) {
    _ensure_brush_cursor();
    if (brush_cursor) {
        brush_cursor->set_global_position(world_pos);
        brush_cursor->set_scale(Vector3(radius, radius, radius));
        brush_cursor->set_visible(true);
    }
}

void VFXEditorNode::clear_brush_cursor() {
    if (brush_cursor) brush_cursor->set_visible(false);
}

Vector3 VFXEditorNode::raycast_mesh(const Vector3& ray_origin, const Vector3& ray_dir, float max_dist) {
    if (mesh.is_null()) return Vector3();
    Vector3 hit_pos, hit_norm;
    int hit_face = -1;
    if (mesh->raycast(ray_origin, ray_dir, hit_pos, hit_norm, hit_face)) {
        return hit_pos;
    }
    return Vector3();
}

// ============================================================================
// EXPORT & DEMOS
// ============================================================================
bool VFXEditorNode::export_glb(const String& filepath) {
    if (mesh.is_null()) return false;
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_glb(filepath, mesh, skeleton, skin, animator);
}

bool VFXEditorNode::export_glb_animated(const String& filepath, int clip_idx) {
    if (mesh.is_null()) return false;
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_glb_animated(filepath, mesh, skeleton, skin, animator, clip_idx);
}

bool VFXEditorNode::export_vat(const String& filepath, int frame_count, float fps) {
    if (mesh.is_null()) return false;
    Ref<VFXGLTFExporter> exporter;
    exporter.instantiate();
    return exporter->export_vat(filepath, mesh, animator, frame_count, fps);
}

void VFXEditorNode::create_demo_cube() {
    mesh.instantiate();
    mesh->create_cube(Vector3(1, 1, 1));
    set_vfx_mesh(mesh);
}

void VFXEditorNode::create_demo_character() {
    mesh.instantiate();
    mesh->create_cylinder(0.4f, 1.8f, 12, 6);
    set_vfx_mesh(mesh);
    create_mixamo_skeleton();
    skin.instantiate();
    set_vfx_skin(skin);
    auto_weight();
}

// ============================================================================
// EDIT MODE & SELECTION
// ============================================================================
void VFXEditorNode::set_edit_mode(int mode) {
    edit_mode = mode;
    clear_selection();
    _update_gizmo_visibility();
    _build_selection_mesh();
}
int VFXEditorNode::get_edit_mode() const { return edit_mode; }

void VFXEditorNode::set_show_wireframe(bool show) {
    show_wireframe = show;
    _build_selection_mesh();
}
bool VFXEditorNode::get_show_wireframe() const { return show_wireframe; }

void VFXEditorNode::clear_selection() {
    selected_vertex = -1;
    selected_edge = -1;
    selected_face = -1;
    _update_gizmo_visibility();
    _build_selection_mesh();
}

int VFXEditorNode::get_selected_face() const { return selected_face; }
int VFXEditorNode::get_selected_edge() const { return selected_edge; }
int VFXEditorNode::get_selected_vertex() const { return selected_vertex; }

bool VFXEditorNode::raycast_select(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (mesh.is_null() || !is_visible_in_tree()) return false;

    if (edit_mode == MODE_VERTEX) {
        float best_t = 1e20f;
        int best_v = -1;
        for (auto* v : mesh->get_vertices()) {
            if (v->deleted) continue;
            float t;
            if (_ray_vs_segment(ray_origin, ray_dir, v->position, v->position, 0.05f, t)) {
                if (t < best_t) { best_t = t; best_v = v->id; }
            }
        }
        if (best_v >= 0) {
            selected_vertex = best_v;
            set_gizmo_transform(Transform3D(Basis(), mesh->get_vertex_position(selected_vertex)));
            _update_gizmo_visibility();
            _build_selection_mesh();
            return true;
        }
    } else if (edit_mode == MODE_EDGE) {
        float best_t = 1e20f;
        int best_e = -1;
        for (auto* e : mesh->get_edges()) {
            if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
            float t;
            if (_ray_vs_segment(ray_origin, ray_dir, e->next->vertex->position, e->vertex->position, 0.04f, t)) {
                if (t < best_t) { best_t = t; best_e = e->id; }
            }
        }
        if (best_e >= 0) {
            selected_edge = best_e;
            int v0, v1;
            mesh->get_edge_endpoints(selected_edge, v0, v1);
            Vector3 mid = (mesh->get_vertex_position(v0) + mesh->get_vertex_position(v1)) * 0.5f;
            set_gizmo_transform(Transform3D(Basis(), mid));
            _update_gizmo_visibility();
            _build_selection_mesh();
            return true;
        }
    } else if (edit_mode == MODE_FACE) {
        Vector3 hit_pos, hit_norm;
        int hit_face = -1;
        if (mesh->raycast(ray_origin, ray_dir, hit_pos, hit_norm, hit_face)) {
            selected_face = hit_face;
            Vector3 center = mesh->get_face_center(selected_face);
            set_gizmo_transform(Transform3D(Basis(), center));
            _update_gizmo_visibility();
            _build_selection_mesh();
            return true;
        }
    }
    return false;
}

// ============================================================================
// MODELING OPERATORS
// ============================================================================
void VFXEditorNode::extrude_selected_face(float distance) {
    if (mesh.is_valid() && selected_face >= 0) {
        mesh->extrude_face(selected_face, distance);
        refresh_mesh();
    }
}

void VFXEditorNode::inset_selected_face(float amount) {
    if (mesh.is_valid() && selected_face >= 0) {
        mesh->inset_face(selected_face, amount);
        refresh_mesh();
    }
}

void VFXEditorNode::delete_selected_face() {
    if (mesh.is_valid() && selected_face >= 0) {
        mesh->delete_face(selected_face);
        selected_face = -1;
        refresh_mesh();
    }
}

void VFXEditorNode::subdivide_selected_face() {
    if (mesh.is_valid() && selected_face >= 0) {
        mesh->subdivide_face(selected_face);
        refresh_mesh();
    }
}

void VFXEditorNode::flip_normals() {
    if (mesh.is_valid()) {
        mesh->flip_normals();
        refresh_mesh();
    }
}

void VFXEditorNode::mesh_cleanup() {
    if (mesh.is_valid()) {
        mesh->cleanup();
        refresh_mesh();
    }
}

void VFXEditorNode::extrude_selection(float distance) { extrude_selected_face(distance); }
void VFXEditorNode::inset_selection(float amount) { inset_selected_face(amount); }
void VFXEditorNode::delete_selection() {
    if (edit_mode == MODE_FACE) delete_selected_face();
    else if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        mesh->delete_vertex(selected_vertex);
        selected_vertex = -1;
        refresh_mesh();
    }
}
void VFXEditorNode::subdivide_selection() { subdivide_selected_face(); }

void VFXEditorNode::bevel_selection(float amount) {
    if (mesh.is_valid() && selected_edge >= 0) {
        mesh->bevel_edge(selected_edge, amount);
        refresh_mesh();
    }
}

void VFXEditorNode::knife_selection(const Vector3& p0, const Vector3& p1) {
    if (mesh.is_valid()) {
        mesh->knife_cut(p0, p1);
        refresh_mesh();
    }
}

// ============================================================================
// CURVES, TEXTURE PAINTER & SYMMETRY
// ============================================================================
void VFXEditorNode::create_curve_tube(const Array& points, float radius, int segments, int rings) {
    mesh.instantiate();
    mesh->create_tube_from_curve(points, radius, segments, rings);
    set_vfx_mesh(mesh);
}

void VFXEditorNode::create_curve_ribbon(const Array& points, float width, int segments) {
    mesh.instantiate();
    mesh->create_ribbon_from_curve(points, width, segments);
    set_vfx_mesh(mesh);
}

void VFXEditorNode::set_active_curve(const Ref<VFXCurve>& curve) { active_curve = curve; }
Ref<VFXCurve> VFXEditorNode::get_active_curve() const { return active_curve; }

void VFXEditorNode::curve_to_mesh(float radius, int segments, int rings) {
    if (active_curve.is_valid()) {
        Array pts = active_curve->get_sampled_points();
        create_curve_tube(pts, radius, segments, rings);
    }
}

void VFXEditorNode::set_texture_painter(const Ref<VFXTexturePainter>& p) { texture_painter = p; }
Ref<VFXTexturePainter> VFXEditorNode::get_texture_painter() const { return texture_painter; }

void VFXEditorNode::paint_at(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (texture_painter.is_valid() && mesh.is_valid()) {
        texture_painter->paint_raycast(mesh, ray_origin, ray_dir);
    }
}

void VFXEditorNode::set_symmetry_enabled(bool enabled) { symmetry_enabled = enabled; }
bool VFXEditorNode::get_symmetry_enabled() const { return symmetry_enabled; }

void VFXEditorNode::set_symmetry_axis(int axis) { symmetry_axis = axis; }
int VFXEditorNode::get_symmetry_axis() const { return symmetry_axis; }