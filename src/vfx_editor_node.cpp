#include "vfx_editor_node.h"
#include "vfx_gltf_exporter.h"
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

// Ring / torus for rotation gizmo
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

// Cube handle for scale gizmo
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
}

// ============================================================================
// LIFECYCLE
// ============================================================================
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
        _ensure_gizmo_node();
        _ensure_skeleton_visual();
    }
    if (p_what == NOTIFICATION_PROCESS) {
        if (animator.is_valid() && animator->is_clip_playing()) {
            animator->advance(get_process_delta_time());
        }
        if (show_skeleton && skeleton.is_valid()) {
            _build_skeleton_mesh();
            if (skel_visual) skel_visual->set_visible(true);
        } else if (skel_visual) {
            skel_visual->set_visible(false);
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
        positions = skin->compute_skinned_positions();
    } else {
        positions = mesh->get_positions();
    }

    arrays[Mesh::ARRAY_VERTEX] = positions;
    arrays[Mesh::ARRAY_NORMAL] = mesh->get_normals();
    arrays[Mesh::ARRAY_TEX_UV] = mesh->get_uvs();
    arrays[Mesh::ARRAY_COLOR] = mesh->get_colors();
    arrays[Mesh::ARRAY_INDEX] = mesh->get_indices();

    if (!show_weights && skin.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
        // pre-skinned: omit bones/weights
    } else {
        int vc = mesh->get_vertex_count();
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

// ============================================================================
// GIZMO — VISUAL TRANSFORM (strips bone scale so handles stay fixed size)
// ============================================================================
Transform3D VFXEditorNode::_get_visual_gizmo_transform() const {
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
    if (gizmo_node) {
        gizmo_node->set_visible(selected_bone >= 0);
    }
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

        // Uniform scale box at origin
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
        if (fabs(cam_dir.dot(Vector3(0,1,0))) < 0.9f)
            gizmo_drag_plane = Plane(Vector3(0,1,0), origin);
        else
            gizmo_drag_plane = Plane(Vector3(1,0,0), origin);
        Vector3 hit;
        if (_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) {
            gizmo_drag_start_point = hit;
            gizmo_drag_start_scale = gizmo_transform.basis.get_scale();
        }
    }
}

void VFXEditorNode::gizmo_drag(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (gizmo_drag_axis == GIZMO_NONE) return;
    Vector3 hit;
    if (!_ray_vs_plane(ray_origin, ray_dir, gizmo_drag_plane, hit)) return;

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
        if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
    } else if (gizmo_mode == GIZMO_ROTATE) {
        Vector3 origin = gizmo_transform.get_origin();
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
        if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
    } else if (gizmo_mode == GIZMO_SCALE) {
        Vector3 delta = hit - gizmo_drag_start_point;
        Transform3D t = gizmo_drag_start_transform;
        Vector3 scale = gizmo_drag_start_scale;

        if (gizmo_drag_axis <= GIZMO_Z) {
            Vector3 world_axis = t.basis.get_column(gizmo_drag_axis).normalized();
            float proj = delta.dot(world_axis);
            float base_len = t.basis.get_column(gizmo_drag_axis).length();
            if (base_len > 0.0001f) {
                float factor = 1.0f + proj / base_len;
                factor = fmaxf(factor, 0.01f);
                scale[gizmo_drag_axis] = gizmo_drag_start_scale[gizmo_drag_axis] * factor;
            }
        } else if (gizmo_drag_axis == GIZMO_XYZ) {
            float proj = delta.length();
            float sign = (delta.dot(ray_dir.normalized()) > 0.0f) ? 1.0f : -1.0f;
            float factor = 1.0f + sign * proj / gizmo_screen_scale;
            factor = fmaxf(factor, 0.01f);
            scale = gizmo_drag_start_scale * factor;
        }

        Basis b = t.basis;
        b.set_column(0, b.get_column(0).normalized() * scale.x);
        b.set_column(1, b.get_column(1).normalized() * scale.y);
        b.set_column(2, b.get_column(2).normalized() * scale.z);
        gizmo_transform.set_basis(b);
        if (gizmo_node) gizmo_node->set_transform(_get_visual_gizmo_transform());
    }

    if (selected_bone >= 0 && skeleton.is_valid()) {
        int parent = skeleton->get_bone_parent(selected_bone);
        Transform3D parent_world = (parent >= 0) ? skeleton->get_bone_model_transform(parent) : Transform3D();
        Transform3D local = parent_world.affine_inverse() * gizmo_transform;
        skeleton->set_bone_pose(selected_bone, local);
        skeleton->update_transforms();
        _build_skeleton_mesh();
        _update_godot_mesh();
    }
}

void VFXEditorNode::gizmo_end_drag() {
    gizmo_drag_axis = GIZMO_NONE;
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

    skeleton->update_transforms();

    for (int i = 0; i < skeleton->get_bone_count(); i++) {
        int parent = skeleton->get_bone_parent(i);
        Vector3 head = (parent >= 0)
            ? skeleton->get_bone_model_transform(parent).get_origin()
            : skeleton->get_bone_model_transform(i).get_origin();
        Vector3 tail = skeleton->get_bone_model_transform(i).get_origin();

        bool is_selected = (i == selected_bone);
        Color col = is_selected ? Color(1.0f, 0.6f, 0.0f) : Color(0.25f, 0.55f, 0.85f);
        if (i == 0) col = Color(0.6f, 0.6f, 0.6f);

        add_octa(head, tail, col);
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
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    skel_visual->set_material_override(mat);
    skel_visual->set_mesh(am);
}

// ============================================================================
// MATH HELPERS — CORRECTED ray-vs-segment (capsule)
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
void VFXEditorNode::set_show_skeleton(bool show) { show_skeleton = show; }
bool VFXEditorNode::get_show_skeleton() const { return show_skeleton; }
void VFXEditorNode::set_show_weights(bool show) { show_weights = show; if (auto_update) _update_godot_mesh(); }
bool VFXEditorNode::get_show_weights() const { return show_weights; }
void VFXEditorNode::set_visualize_bone(int idx) { visualize_bone = idx; if (show_weights && auto_update) _update_godot_mesh(); }
int VFXEditorNode::get_visualize_bone() const { return visualize_bone; }
void VFXEditorNode::set_auto_update(bool auto_up) { auto_update = auto_up; }
bool VFXEditorNode::get_auto_update() const { return auto_update; }

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
// SKELETON INTERACTION
// ============================================================================
void VFXEditorNode::set_selected_bone(int idx) {
    selected_bone = idx;
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
