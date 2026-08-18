#include "vfx_editor_node.h"
#include "vfx_gltf_exporter.h"
#include "vfx_texture_painter.h"
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
// STATIC HELPERS — GIZMO GEOMETRY
// ============================================================================
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

    // === MODELING ===
    ClassDB::bind_method(D_METHOD("extrude_selected_face", "distance"), &VFXEditorNode::extrude_selected_face);
    ClassDB::bind_method(D_METHOD("inset_selected_face", "amount"), &VFXEditorNode::inset_selected_face);
    ClassDB::bind_method(D_METHOD("delete_selected_face"), &VFXEditorNode::delete_selected_face);
    ClassDB::bind_method(D_METHOD("subdivide_selected_face"), &VFXEditorNode::subdivide_selected_face);
    ClassDB::bind_method(D_METHOD("flip_normals"), &VFXEditorNode::flip_normals);
    ClassDB::bind_method(D_METHOD("mesh_cleanup"), &VFXEditorNode::mesh_cleanup);

    // === CURVE ===
    ClassDB::bind_method(D_METHOD("create_curve_tube", "points", "radius", "segments", "rings"), &VFXEditorNode::create_curve_tube);
    ClassDB::bind_method(D_METHOD("create_curve_ribbon", "points", "width", "segments"), &VFXEditorNode::create_curve_ribbon);
    ClassDB::bind_method(D_METHOD("set_active_curve", "curve"), &VFXEditorNode::set_active_curve);
    ClassDB::bind_method(D_METHOD("get_active_curve"), &VFXEditorNode::get_active_curve);
    ClassDB::bind_method(D_METHOD("curve_to_mesh", "radius", "segments", "rings"), &VFXEditorNode::curve_to_mesh);

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

    // === EDIT MODE ===
    ClassDB::bind_method(D_METHOD("set_edit_mode", "mode"), &VFXEditorNode::set_edit_mode);
    ClassDB::bind_method(D_METHOD("get_edit_mode"), &VFXEditorNode::get_edit_mode);
    ClassDB::bind_method(D_METHOD("set_show_wireframe", "show"), &VFXEditorNode::set_show_wireframe);
    ClassDB::bind_method(D_METHOD("get_show_wireframe"), &VFXEditorNode::get_show_wireframe);
    ClassDB::bind_method(D_METHOD("clear_selection"), &VFXEditorNode::clear_selection);
    ClassDB::bind_method(D_METHOD("get_selected_face"), &VFXEditorNode::get_selected_face);
    ClassDB::bind_method(D_METHOD("get_selected_edge"), &VFXEditorNode::get_selected_edge);
    ClassDB::bind_method(D_METHOD("get_selected_vertex"), &VFXEditorNode::get_selected_vertex);
    ClassDB::bind_method(D_METHOD("raycast_select", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_select);

    // === MODELING (selection-aware) ===
    ClassDB::bind_method(D_METHOD("extrude_selection", "distance"), &VFXEditorNode::extrude_selection);
    ClassDB::bind_method(D_METHOD("inset_selection", "amount"), &VFXEditorNode::inset_selection);
    ClassDB::bind_method(D_METHOD("delete_selection"), &VFXEditorNode::delete_selection);
    ClassDB::bind_method(D_METHOD("subdivide_selection"), &VFXEditorNode::subdivide_selection);
    ClassDB::bind_method(D_METHOD("bevel_selection", "amount"), &VFXEditorNode::bevel_selection);
    ClassDB::bind_method(D_METHOD("knife_selection", "p0", "p1"), &VFXEditorNode::knife_selection);

    // === ENUMS ===
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_OBJECT", MODE_OBJECT);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_VERTEX", MODE_VERTEX);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_EDGE", MODE_EDGE);
    ClassDB::bind_integer_constant(get_class_static(), "", "MODE_FACE", MODE_FACE);

    // === TEXTURE PAINTER ===
    ClassDB::bind_method(D_METHOD("set_texture_painter", "p"), &VFXEditorNode::set_texture_painter);
    ClassDB::bind_method(D_METHOD("get_texture_painter"), &VFXEditorNode::get_texture_painter);
    ClassDB::bind_method(D_METHOD("paint_at", "ray_origin", "ray_dir"), &VFXEditorNode::paint_at);

    // === SYMMETRY ===
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
    base_material->set_cull_mode(StandardMaterial3D::CULL_DISABLED); // FIX: see both sides
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
        _ensure_selection_visual(); // NEW
    }
    if (p_what == NOTIFICATION_PROCESS) {
        if (animator.is_valid() && animator->is_clip_playing()) {
            animator->advance(get_process_delta_time());
        }

        // Refresh skinned mesh when skeleton is posed/animated
        if (mesh.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0 && !show_weights) {
            _update_godot_mesh();
        }

        if (show_skeleton && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
            _build_skeleton_mesh();
            if (skel_visual) skel_visual->set_visible(true);
        } else if (skel_visual) {
            skel_visual->set_visible(false);
        }

        // NEW: selection overlay
        if (mesh.is_valid() && show_wireframe) {
            _build_selection_mesh();
            if (selection_visual) selection_visual->set_visible(true);
        } else if (selection_visual) {
            selection_visual->set_visible(false);
        }
    }
}

// ============================================================================
// INTERNAL — MESH / CURSOR
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

        // DEBUG: verify skinning is actually producing different positions
        PackedVector3Array bind_pos = mesh->get_positions();
        if (bind_pos.size() > 0 && positions.size() > 0) {
            float diff = bind_pos[0].distance_to(positions[0]);
            UtilityFunctions::print("Skinned pos[0] diff: ", diff,
                " bone0_pos: ", skeleton->get_bone_model_transform(0).get_origin(),
                " bind0_pos: ", skeleton->get_bone_bind_pose(0).get_origin());
        }
    } else {
        positions = mesh->get_positions();
    }

    arrays[Mesh::ARRAY_VERTEX] = positions;

    // FIX: validate normals so mesh never goes black
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
        // pre-skinned: omit bones/weights
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
    if (mesh.is_null() || !show_wireframe) {
        selection_visual->set_mesh(Ref<Mesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    // Wireframe edges
    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool is_sel = (edit_mode == MODE_EDGE && selected_edge == (int)e->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.4f, 0.4f, 0.4f, 0.4f);
        float r = is_sel ? 0.012f : 0.004f;
        _append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices
    for (auto* v : mesh->get_vertices()) {
        if (v->deleted) continue;
        bool is_sel = (edit_mode == MODE_VERTEX && selected_vertex == (int)v->id);
        Color col = is_sel ? Color(1.0f, 0.5f, 0.0f, 1.0f) : Color(0.7f, 0.7f, 0.7f, 0.8f);
        float s = is_sel ? 0.035f : 0.018f;
        _append_box(verts, cols, idx, v->position, s, col);
    }

    // Face centers
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
// GIZMO — VISUAL TRANSFORM
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

// ============================================================================
// GIZMO
// ============================================================================
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
    if (!gizmo_node || !gizmo_node->is_visible()) return GIZMO_NONE;

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

// ============================================================================
// GIZMO DRAG BEGIN
// ============================================================================
void VFXEditorNode::gizmo_begin_drag(int axis, const Vector3& ray_origin, const Vector3& ray_dir) {
    gizmo_drag_axis = axis;
    gizmo_drag_start_transform = gizmo_transform;

    // NEW: cache mesh element positions for T/R/S
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

// ============================================================================
// GIZMO DRAG
// ============================================================================
void VFXEditorNode::gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (gizmo_drag_axis == GIZMO_NONE) return;
    Vector3 hit;
    if (!_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) return;

    Vector3 origin = gizmo_transform.get_origin();

    // === EXISTING TRANSLATE / ROTATE / SCALE LOGIC ===
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
        scale.x = vfx::clampf(scale.x, 0.001f, 1000.0f);
        scale.y = vfx::clampf(scale.y, 0.001f, 1000.0f);
        scale.z = vfx::clampf(scale.z, 0.001f, 1000.0f);
        Basis b = gizmo_drag_start_transform.basis;
        b.set_column(0, b.get_column(0).normalized() * scale.x);
        b.set_column(1, b.get_column(1).normalized() * scale.y);
        b.set_column(2, b.get_column(2).normalized() * scale.z);
        gizmo_transform.set_basis(b);
    }

    if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());

    // === NEW: APPLY TO MESH ELEMENTS ===
    if (edit_mode != MODE_OBJECT && !mesh_edit_verts.empty() && mesh.is_valid()) {
        Transform3D delta = gizmo_transform * gizmo_drag_start_transform.affine_inverse();
        for (size_t i = 0; i < mesh_edit_verts.size(); i++) {
            Vector3 new_pos = delta.xform(mesh_edit_initial_positions[i]);
            mesh->set_vertex_position(mesh_edit_verts[i], new_pos);
        }
        if (auto_update) _update_godot_mesh();
        _build_selection_mesh();
        _update_gizmo_for_selection();
        return;
    }

    // === BONE GIZMO LOGIC WITH SYMMETRY ===
    if (selected_bone >= 0 && skeleton.is_valid()) {
        int parent = skeleton->get_bone_parent(selected_bone);
        Transform3D parent_world = (parent >= 0) ? skeleton->get_bone_model_transform(parent) : Transform3D();
        Transform3D local = parent_world.affine_inverse() * gizmo_transform;
        skeleton->set_bone_pose(selected_bone, local);

        if (symmetry_enabled) {
            int sym_bone = skeleton->get_symmetric_bone(selected_bone);
            if (sym_bone >= 0) {
                Transform3D mirrored_local = local;
                Vector3 pos = mirrored_local.get_origin();
                if (symmetry_axis == 0) pos.x = -pos.x;
                else if (symmetry_axis == 1) pos.y = -pos.y;
                else if (symmetry_axis == 2) pos.z = -pos.z;
                mirrored_local.set_origin(pos);

                if (symmetry_axis == 0) {
                    Basis b = mirrored_local.get_basis();
                    Vector3 euler = b.get_euler();
                    euler.x = -euler.x;
                    euler.z = -euler.z;
                    mirrored_local.set_basis(Basis::from_euler(euler));
                }

                skeleton->set_bone_pose(sym_bone, mirrored_local);
            }
        }

        skeleton->update_transforms();
        gizmo_transform = skeleton->get_bone_model_transform(selected_bone);
        if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
        _build_skeleton_mesh();

        // DEBUG: verify bone actually moved
        Vector3 bone_pos = skeleton->get_bone_model_transform(selected_bone).get_origin();
        UtilityFunctions::print("Bone ", selected_bone, " pos: ", bone_pos);
        _update_godot_mesh();
    }
}

void VFXEditorNode::gizmo_end_drag() {
    gizmo_drag_axis = GIZMO_NONE;
    // Snap gizmo back to axis-aligned (keep position, drop accumulated rotation)
    gizmo_transform.basis = Basis();
    if (gizmo_node) {
        gizmo_node->set_transform(_get_visual_gizmo_transform());
        _build_gizmo_mesh();
    }
}

bool VFXEditorNode::is_gizmo_dragging() const {
    return gizmo_drag_axis != GIZMO_NONE;
}

// ============================================================================
// SKELETON VISUAL — BLENDER-STYLE OCTAHEDRAL
// ============================================================================
void VFXEditorNode::_ensure_skeleton_visual() {
    if (!skel_visual) {
        skel_visual = memnew(MeshInstance3D);
        skel_visual->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        add_child(skel_visual);
        skel_visual->set_owner(this);
    }
}

void VFXEditorNode::_build_skeleton_mesh() {
    if (!skel_visual) _ensure_skeleton_visual();
    if (skeleton.is_null() || skeleton->get_bone_count() == 0) {
        skel_visual->set_mesh(Ref<Mesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray colors;
    PackedInt32Array indices;

    auto add_octa = [&](const Vector3& head, const Vector3& tail, const Color& col) {
        Vector3 dir = tail - head;
        float len = dir.length();
        if (len < 0.0001f) return;
        dir /= len;

        Vector3 up = fabs(dir.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
        Vector3 right = dir.cross(up).normalized();
        up = right.cross(dir).normalized();

        float width = len * 0.18f;
        Vector3 center = (head + tail) * 0.5f;

        int base = verts.size();
        verts.push_back(head);
        verts.push_back(tail);
        verts.push_back(center + right * width);
        verts.push_back(center - right * width);
        verts.push_back(center + up * width);
        verts.push_back(center - up * width);

        for (int i = 0; i < 6; i++) colors.push_back(col);

        const int tri[] = {
            0, 2, 4,  0, 4, 3,  0, 3, 5,  0, 5, 2,
            1, 4, 2,  1, 3, 4,  1, 5, 3,  1, 2, 5
        };
        for (int i = 0; i < 24; i++) indices.push_back(base + tri[i]);
    };

    // === NEW: Joint sphere helper ===
    auto add_sphere = [&](const Vector3& center, float radius, int segs, int rings, const Color& col) {
        int base = verts.size();

        for (int r = 0; r <= rings; r++) {
            float phi = (float)r / rings * 3.14159265f;
            for (int s = 0; s <= segs; s++) {
                float theta = (float)s / segs * 3.14159265f * 2.0f;
                Vector3 p(
                    sinf(phi) * cosf(theta) * radius,
                    cosf(phi) * radius,
                    sinf(phi) * sinf(theta) * radius
                );
                verts.push_back(center + p);
                colors.push_back(col);
            }
        }

        for (int r = 0; r < rings; r++) {
            for (int s = 0; s < segs; s++) {
                int a = base + r * (segs + 1) + s;
                int b = base + (r + 1) * (segs + 1) + s;
                int c = base + (r + 1) * (segs + 1) + (s + 1);
                int d = base + r * (segs + 1) + (s + 1);

                indices.push_back(a); indices.push_back(b); indices.push_back(d);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
    };

    skeleton->update_transforms();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        bool is_selected = (i == selected_bone);
        Color col = is_selected ? Color(1.0f, 0.6f, 0.0f) : Color(0.25f, 0.55f, 0.85f);
        if (i == 0) col = Color(0.6f, 0.6f, 0.6f); // root is gray

        add_octa(head, tail, col);

        // === NEW: Add sphere at joint (head position) ===
        float joint_radius = (tail - head).length() * 0.22f;
        if (joint_radius < 0.04f) joint_radius = 0.04f;
        if (joint_radius > 0.12f) joint_radius = 0.12f;

        Color joint_col = is_selected ? Color(1.0f, 0.8f, 0.2f) : Color(0.35f, 0.65f, 0.95f);
        if (i == 0) joint_col = Color(0.7f, 0.7f, 0.7f);

        add_sphere(head, joint_radius, 8, 6, joint_col);

        // === NEW: Add smaller sphere at tail (bone tip) for leaf bones ===
        if (skeleton->get_bone_children(i).size() == 0) {
            add_sphere(tail, joint_radius * 0.6f, 6, 4, col);
        }
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = colors;
        arrays[Mesh::ARRAY_INDEX] = indices;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_PER_PIXEL);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    skel_visual->set_material_override(mat);
    skel_visual->set_mesh(am);
}


// ============================================================================
// MATH HELPERS
// ============================================================================
bool VFXEditorNode::_ray_vs_segment(const Vector3& ro, const Vector3& rd,
                                    const Vector3& a, const Vector3& b,
                                    float radius, float& out_t) const {
    Vector3 u = rd.normalized();
    Vector3 v = b - a;
    Vector3 w0 = ro - a;

    float uv = u.dot(v);
    float vv = v.length_squared();
    float uw0 = u.dot(w0);
    float vw0 = v.dot(w0);

    if (vv < 0.0001f) {
        Vector3 oc = ro - a;
        float b_ = u.dot(oc);
        float c = oc.dot(oc) - radius * radius;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) return false;
        float t = -b_ - sqrt(disc);
        if (t < 0.0f) t = -b_ + sqrt(disc);
        if (t < 0.0f) return false;
        out_t = t;
        return true;
    }

    float det = uv * uv - vv;
    float t, s;

    if (fabs(det) < 0.0001f) {
        s = vfx::clampf(vw0 / vv, 0.0f, 1.0f);
        Vector3 closest_seg = a + v * s;
        t = u.dot(closest_seg - ro);
    } else {
        t = (uw0 * vv - uv * vw0) / det;
        s = (uv * uw0 - vw0) / det;
    }

    if (s >= 0.0f && s <= 1.0f && t >= 0.0f) {
        Vector3 closest_seg = a + v * s;
        Vector3 closest_ray = ro + u * t;
        if ((closest_seg - closest_ray).length_squared() < radius * radius) {
            out_t = t;
            return true;
        }
    }

    if (s >= 0.0f && s <= 1.0f && t < 0.0f) {
        Vector3 closest_seg = a + v * s;
        if ((closest_seg - ro).length_squared() < radius * radius) {
            out_t = 0.0f;
            return true;
        }
    }

    auto sphere_test = [&](const Vector3& center) -> bool {
        Vector3 oc = ro - center;
        float b_ = u.dot(oc);
        float c = oc.dot(oc) - radius * radius;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) return false;
        float tt = -b_ - sqrt(disc);
        if (tt < 0.0f) tt = -b_ + sqrt(disc);
        if (tt < 0.0f) return false;
        out_t = tt;
        return true;
    };

    if (s < 0.0f) {
        if (sphere_test(a)) return true;
    }
    if (s > 1.0f) {
        if (sphere_test(b)) return true;
    }

    return false;
}

bool VFXEditorNode::_ray_vs_plane(const Vector3& ro, const Vector3& rd,
                                  const Plane& p, Vector3& out_hit) const {
    float denom = p.normal.dot(rd);
    if (fabs(denom) < 0.0001f) return false;
    float t = -(p.normal.dot(ro) + p.d) / denom;
    if (t < 0.0f) return false;
    out_hit = ro + rd * t;
    return true;
}

// ============================================================================
// MESH / SKELETON / SKIN / ANIMATOR
// ============================================================================
void VFXEditorNode::set_vfx_mesh(const Ref<VFXMesh>& p_mesh) {
    mesh = p_mesh;
    if (skin.is_valid()) skin->set_mesh(mesh);
    if (painter.is_valid()) painter->set_mesh(mesh);
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

// ============================================================================
// VISIBILITY / STATE
// ============================================================================
void VFXEditorNode::set_show_skeleton(bool show) { 
    show_skeleton = show; 
    if (skel_visual) {
        skel_visual->set_visible(show_skeleton && skeleton.is_valid() && skeleton->get_bone_count() > 0);
    }
    if (show_skeleton && skeleton.is_valid()) {
        _build_skeleton_mesh();
    }
    _update_gizmo_visibility();
}
bool VFXEditorNode::get_show_skeleton() const { return show_skeleton; }
void VFXEditorNode::set_show_weights(bool show) { show_weights = show; if (auto_update) _update_godot_mesh(); }
bool VFXEditorNode::get_show_weights() const { return show_weights; }
void VFXEditorNode::set_visualize_bone(int idx) { visualize_bone = idx; if (show_weights && auto_update) _update_godot_mesh(); }
int VFXEditorNode::get_visualize_bone() const { return visualize_bone; }
void VFXEditorNode::set_auto_update(bool auto_up) { auto_update = auto_up; }
bool VFXEditorNode::get_auto_update() const { return auto_update; }

void VFXEditorNode::set_edit_mode(int mode) {
    edit_mode = mode;
    clear_selection();
    _build_selection_mesh();
}

int VFXEditorNode::get_edit_mode() const { return edit_mode; }

void VFXEditorNode::set_show_wireframe(bool show) {
    show_wireframe = show;
    if (!show_wireframe && selection_visual) selection_visual->set_visible(false);
}

bool VFXEditorNode::get_show_wireframe() const { return show_wireframe; }

void VFXEditorNode::clear_selection() {
    selected_face = -1;
    selected_edge = -1;
    selected_vertex = -1;
    selected_bone = -1;
    _update_gizmo_for_selection();
    if (selection_visual) _build_selection_mesh();
    _build_skeleton_mesh();
}

int VFXEditorNode::get_selected_face() const { return selected_face; }
int VFXEditorNode::get_selected_edge() const { return selected_edge; }
int VFXEditorNode::get_selected_vertex() const { return selected_vertex; }

int VFXEditorNode::raycast_select(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (mesh.is_null()) return -1;
    Transform3D inv = get_global_transform().affine_inverse();
    Vector3 ro = inv.xform(ray_origin);
    Vector3 rd = inv.basis.xform(ray_dir).normalized();

    if (edit_mode == MODE_FACE) {
        Vector3 hit;
        int face_idx;
        if (mesh->raycast_select_face(ro, rd, hit, face_idx)) {
            selected_face = face_idx;
            selected_edge = -1;
            selected_vertex = -1;
            _build_selection_mesh();
            _update_gizmo_for_selection();
            return face_idx;
        }
    } else if (edit_mode == MODE_EDGE) {
        int edge_idx;
        if (mesh->raycast_select_edge(ro, rd, edge_idx)) {
            selected_edge = edge_idx;
            selected_face = -1;
            selected_vertex = -1;
            _build_selection_mesh();
            _update_gizmo_for_selection();
            return edge_idx;
        }
    } else if (edit_mode == MODE_VERTEX) {
        int vert_idx;
        if (mesh->raycast_select_vertex(ro, rd, vert_idx)) {
            selected_vertex = vert_idx;
            selected_face = -1;
            selected_edge = -1;
            _build_selection_mesh();
            _update_gizmo_for_selection();
            return vert_idx;
        }
    }
    return -1;
}

void VFXEditorNode::_update_gizmo_for_selection() {
    Vector3 center;
    bool has = false;
    if (edit_mode == MODE_VERTEX && selected_vertex >= 0) {
        center = mesh->get_vertex_position(selected_vertex);
        has = true;
    } else if (edit_mode == MODE_EDGE && selected_edge >= 0) {
        center = mesh->get_edge_midpoint(selected_edge);
        has = true;
    } else if (edit_mode == MODE_FACE && selected_face >= 0) {
        center = mesh->get_face_center(selected_face);
        has = true;
    }

    if (has) {
        gizmo_transform.set_origin(center);
        gizmo_transform.basis = Basis();
        if (gizmo_node) {
            gizmo_node->set_transform(_get_visual_gizmo_transform());
            gizmo_node->set_visible(true);
            _build_gizmo_mesh();
        }
    } else {
        if (gizmo_node) gizmo_node->set_visible(false);
    }
    _update_gizmo_visibility();
}

// ============================================================================
// BRUSH CURSOR
// ============================================================================
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
// MODELING
// ============================================================================
void VFXEditorNode::extrude_selected_face(float distance) {
    // TODO: implement face selection tracking; for now extrudes face 0
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

// === MODELING (selection-aware) ===
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
// SKELETON INTERACTION
// ============================================================================
void VFXEditorNode::set_selected_bone(int idx) {
    selected_bone = idx;
    // Clear mesh selection when selecting a bone
    selected_face = -1;
    selected_edge = -1;
    selected_vertex = -1;
    if (selection_visual) _build_selection_mesh();

    _update_gizmo_visibility();
    if (skeleton.is_valid() && idx >= 0) {
        gizmo_transform = skeleton->get_bone_model_transform(idx);
        if (gizmo_node) {
            gizmo_node->set_transform(_get_visual_gizmo_transform());
            _build_gizmo_mesh();
        }
    } else if (gizmo_node) {
        gizmo_node->set_visible(false);
    }
    _build_skeleton_mesh();
}

int VFXEditorNode::get_selected_bone() const {
    return selected_bone;
}

int VFXEditorNode::raycast_bone(const Vector3& ray_origin, const Vector3& ray_dir) const {
    if (skeleton.is_null()) return -1;
    skeleton->update_transforms();

    int best = -1;
    float best_t = 1e20f;
    Vector3 rd = ray_dir.normalized();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        float t;
        float radius = (tail - head).length() * 0.35f;
        if (radius < 0.08f) radius = 0.08f;

        if (_ray_vs_segment(ray_origin, rd, head, tail, radius, t)) {
            if (t < best_t) {
                best_t = t;
                best = i;
            }
        }
    }
    return best;
}

// ============================================================================
// UNIFIED TOUCH API
// ============================================================================
int VFXEditorNode::on_touch_down(const Vector3& ray_origin, const Vector3& ray_dir) {
    // === MESH EDIT MODE (priority when active) ===
    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        // Gizmo takes precedence if active
        if ((selected_vertex >= 0 || selected_edge >= 0 || selected_face >= 0) && gizmo_node && gizmo_node->is_visible()) {
            int axis = raycast_gizmo(ray_origin, ray_dir);
            if (axis >= 0) {
                gizmo_begin_drag(axis, ray_origin, ray_dir);
                return -2;
            }
        }
        int hit = raycast_select(ray_origin, ray_dir);
        if (hit >= 0) return hit;
        clear_selection();
        return -1;
    }

    // === SKELETON MODE (existing logic) ===
    if (show_skeleton) {
        if (skeleton.is_valid() && selected_bone >= 0) {
            int axis = raycast_gizmo(ray_origin, ray_dir);
            if (axis >= 0) {
                gizmo_begin_drag(axis, ray_origin, ray_dir);
                return -2;
            }
        }
        if (skeleton.is_valid() && skeleton->get_bone_count() > 0) {
            int bone = raycast_bone(ray_origin, ray_dir);
            if (bone >= 0) {
                set_selected_bone(bone);
                return bone;
            }
        }
    }
    return -1;
}

void VFXEditorNode::on_touch_up() {
    if (is_gizmo_dragging()) {
        gizmo_end_drag();
    }
}

void VFXEditorNode::on_touch_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (is_gizmo_dragging()) {
        gizmo_drag(ray_origin, ray_dir);
    }
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

// ============================================================================
// SYMMETRY
// ============================================================================
void VFXEditorNode::set_symmetry_enabled(bool enabled) { symmetry_enabled = enabled; }
bool VFXEditorNode::get_symmetry_enabled() const { return symmetry_enabled; }
void VFXEditorNode::set_symmetry_axis(int axis) { symmetry_axis = axis; }
int VFXEditorNode::get_symmetry_axis() const { return symmetry_axis; }