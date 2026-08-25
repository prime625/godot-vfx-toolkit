#include "vfx_editor_node.h"
#include "vfx_editor_utils.h"
#include "vfx_gltf_exporter.h"
#include "vfx_texture_painter.h"
#include "vfx_scene_tree_panel.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/geometry_instance3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>
#include <unordered_set>

using namespace godot;

// ============================================================================
// BINDINGS
// ============================================================================
void VFXEditorNode::_bind_methods() {
    ClassDB::bind_method(D_METHOD("refresh_scene"), &VFXEditorNode::refresh_scene);
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
    ClassDB::bind_method(D_METHOD("set_gizmo_locked", "locked"), &VFXEditorNode::set_gizmo_locked);
    ClassDB::bind_method(D_METHOD("get_gizmo_locked"), &VFXEditorNode::get_gizmo_locked);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "gizmo_locked"), "set_gizmo_locked", "get_gizmo_locked");
    

    ClassDB::bind_method(D_METHOD("set_selected_bone", "idx"), &VFXEditorNode::set_selected_bone);
    ClassDB::bind_method(D_METHOD("get_selected_bone"), &VFXEditorNode::get_selected_bone);
    ClassDB::bind_method(D_METHOD("raycast_bone", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_bone);

    ClassDB::bind_method(D_METHOD("set_camera", "camera"), &VFXEditorNode::set_camera);
    ClassDB::bind_method(D_METHOD("get_camera"), &VFXEditorNode::get_camera);
    ClassDB::bind_method(D_METHOD("set_select_pixel_tolerance", "px"), &VFXEditorNode::set_select_pixel_tolerance);
    ClassDB::bind_method(D_METHOD("get_select_pixel_tolerance"), &VFXEditorNode::get_select_pixel_tolerance);
    ClassDB::bind_method(D_METHOD("set_gizmo_select_pixel_tolerance", "px"), &VFXEditorNode::set_gizmo_select_pixel_tolerance);
    ClassDB::bind_method(D_METHOD("get_gizmo_select_pixel_tolerance"), &VFXEditorNode::get_gizmo_select_pixel_tolerance);
    ClassDB::bind_method(D_METHOD("screen_select_vertex", "screen_pos"), &VFXEditorNode::screen_select_vertex);
    ClassDB::bind_method(D_METHOD("screen_select_edge", "screen_pos"), &VFXEditorNode::screen_select_edge);
    ClassDB::bind_method(D_METHOD("screen_select_face", "screen_pos"), &VFXEditorNode::screen_select_face);
    ClassDB::bind_method(D_METHOD("screen_select_box", "screen_rect"), &VFXEditorNode::screen_select_box);
    ClassDB::bind_method(D_METHOD("box_select", "screen_rect"), &VFXEditorNode::box_select);
    ClassDB::bind_method(D_METHOD("screen_raycast_gizmo", "screen_pos"), &VFXEditorNode::screen_raycast_gizmo);

    ClassDB::bind_method(D_METHOD("on_touch_down", "ray_origin", "ray_dir", "screen_pos"), &VFXEditorNode::on_touch_down);
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

    // === SELECTION MODE ===
    ClassDB::bind_method(D_METHOD("set_selection_mode", "mode"), &VFXEditorNode::set_selection_mode);
    ClassDB::bind_method(D_METHOD("get_selection_mode"), &VFXEditorNode::get_selection_mode);
    ClassDB::bind_method(D_METHOD("is_face_selected", "idx"), &VFXEditorNode::is_face_selected);
    ClassDB::bind_method(D_METHOD("is_edge_selected", "idx"), &VFXEditorNode::is_edge_selected);
    ClassDB::bind_method(D_METHOD("is_vertex_selected", "idx"), &VFXEditorNode::is_vertex_selected);
    ClassDB::bind_method(D_METHOD("get_selected_faces"), &VFXEditorNode::get_selected_faces);
    ClassDB::bind_method(D_METHOD("get_selected_edges"), &VFXEditorNode::get_selected_edges);
    ClassDB::bind_method(D_METHOD("get_selected_vertices"), &VFXEditorNode::get_selected_vertices);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "selection_mode", PROPERTY_HINT_ENUM, "Single,Multi,Loop"), "set_selection_mode", "get_selection_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gizmo_select_pixel_tolerance"), "set_gizmo_select_pixel_tolerance", "get_gizmo_select_pixel_tolerance");

    ClassDB::bind_method(D_METHOD("set_proportional_editing_enabled", "enabled"), &VFXEditorNode::set_proportional_editing_enabled);
    ClassDB::bind_method(D_METHOD("get_proportional_editing_enabled"), &VFXEditorNode::get_proportional_editing_enabled);
    ClassDB::bind_method(D_METHOD("set_proportional_radius", "radius"), &VFXEditorNode::set_proportional_radius);
    ClassDB::bind_method(D_METHOD("get_proportional_radius"), &VFXEditorNode::get_proportional_radius);
    ClassDB::bind_method(D_METHOD("set_falloff_mode", "mode"), &VFXEditorNode::set_falloff_mode);
    ClassDB::bind_method(D_METHOD("get_falloff_mode"), &VFXEditorNode::get_falloff_mode);

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "proportional_editing"), "set_proportional_editing_enabled", "get_proportional_editing_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "proportional_radius"), "set_proportional_radius", "get_proportional_radius");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "falloff_mode", PROPERTY_HINT_ENUM, "Smooth,Sphere,Root,Inverse Square,Sharp,Linear,Constant,Random"), "set_falloff_mode", "get_falloff_mode");

    BIND_ENUM_CONSTANT(FALLOFF_SMOOTH);
    BIND_ENUM_CONSTANT(FALLOFF_SPHERE);
    BIND_ENUM_CONSTANT(FALLOFF_ROOT);
    BIND_ENUM_CONSTANT(FALLOFF_INVERSE_SQUARE);
    BIND_ENUM_CONSTANT(FALLOFF_SHARP);
    BIND_ENUM_CONSTANT(FALLOFF_LINEAR);
    BIND_ENUM_CONSTANT(FALLOFF_CONSTANT);
    BIND_ENUM_CONSTANT(FALLOFF_RANDOM);

    BIND_ENUM_CONSTANT(SELECTION_MODE_SINGLE);
    BIND_ENUM_CONSTANT(SELECTION_MODE_MULTI);
    BIND_ENUM_CONSTANT(SELECTION_MODE_LOOP);

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

    // === SCENE TREE ===
    ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VFXEditorNode::set_scene);
    ClassDB::bind_method(D_METHOD("get_scene"), &VFXEditorNode::get_scene);
    ClassDB::bind_method(D_METHOD("set_active_scene_node", "node"), &VFXEditorNode::set_active_scene_node);
    ClassDB::bind_method(D_METHOD("get_active_scene_node"), &VFXEditorNode::get_active_scene_node);
    ClassDB::bind_method(D_METHOD("import_model", "filepath", "parent"), &VFXEditorNode::import_model, DEFVAL(Ref<VFXSceneNode>()));
    ClassDB::bind_method(D_METHOD("raycast_scene_node", "ray_origin", "ray_dir"), &VFXEditorNode::raycast_scene_node);

    ClassDB::bind_integer_constant(get_class_static(), "", "SCENE_NODE_HIT", SCENE_NODE_HIT);
    ClassDB::bind_integer_constant(get_class_static(), "", "MESH_ELEMENT_HIT", MESH_ELEMENT_HIT);
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
        set_process(true);
        _ensure_mesh_instance();
        _ensure_brush_cursor();
        _ensure_gizmo_node();
        _ensure_skeleton_visual();
        _ensure_selection_visual();
        _ensure_scene_container();

        // Create scene tree panel (hidden by default, UI script toggles visibility)
        if (!scene_tree_panel) {
            scene_tree_panel = memnew(VFXSceneTreePanel);
            scene_tree_panel->set_name("SceneTreePanel");
            scene_tree_panel->set_visible(false); // UI layer controls this
            scene_tree_panel->connect("node_selected", callable_mp(this, &VFXEditorNode::_on_scene_node_selected));
            add_child(scene_tree_panel);
            scene_tree_panel->set_owner(this);
        }
    }
    if (p_what == NOTIFICATION_PROCESS) {
        if (animator.is_valid() && animator->is_clip_playing()) {
            animator->advance(get_process_delta_time());
        }

        // Dirty-flagged scene sync — only rebuilds when data changes
        if (scene.is_valid() && scene_visuals_dirty) {
            scene_visuals_dirty = false;
            _sync_scene_visuals();
        } else if (mesh.is_valid() && skeleton.is_valid() && skeleton->get_bone_count() > 0 && !show_weights && !scene.is_valid()) {
            _update_godot_mesh();
        }

        if (show_skeleton && skeleton.is_valid() && skeleton->get_bone_count() > 0) {
            _build_skeleton_mesh();
            if (skel_visual) skel_visual->set_visible(true);
        } else if (skel_visual) {
            skel_visual->set_visible(false);
        }

        if (mesh.is_valid() && show_wireframe) {
            _build_selection_mesh();
            if (selection_visual) selection_visual->set_visible(true);
        } else if (selection_visual) {
            selection_visual->set_visible(false);
        }
    }
}




void VFXEditorNode::set_gizmo_locked(bool locked) {
    gizmo_locked = locked;
    _update_gizmo_visibility();
}
bool VFXEditorNode::get_gizmo_locked() const {
    return gizmo_locked;
}

// ============================================================================
// INTERNAL — ENSURE CHILD NODES
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

void VFXEditorNode::_ensure_gizmo_node() {
    if (!gizmo_node) {
        gizmo_node = memnew(MeshInstance3D);
        gizmo_node->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        gizmo_node->set_visible(false);
        add_child(gizmo_node);
        gizmo_node->set_owner(this);
    }
}

void VFXEditorNode::_ensure_selection_visual() {
    if (!selection_visual) {
        selection_visual = memnew(MeshInstance3D);
        selection_visual->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        add_child(selection_visual);
        selection_visual->set_owner(this);
    }
}

void VFXEditorNode::_ensure_skeleton_visual() {
    if (!skel_visual) {
        skel_visual = memnew(MeshInstance3D);
        skel_visual->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
        add_child(skel_visual);
        skel_visual->set_owner(this);
    }
}

void VFXEditorNode::_ensure_scene_container() {
    if (!scene_container) {
        scene_container = memnew(Node3D);
        scene_container->set_name("SceneContainer");
        add_child(scene_container);
        scene_container->set_owner(this);
    }
}

// ============================================================================
// SCENE TREE INTEGRATION
// ============================================================================
void VFXEditorNode::_on_scene_node_selected(Ref<VFXSceneNode> p_node) {
    set_active_scene_node(p_node);
}

void VFXEditorNode::mark_scene_dirty() {
    scene_visuals_dirty = true;
    if (scene_tree_panel) {
        scene_tree_panel->mark_dirty();
    }
}

// ============================================================================
// GIZMO VISUAL TRANSFORM
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
// GIZMO VISIBILITY
// ============================================================================
void VFXEditorNode::_update_gizmo_visibility() {
    if (!gizmo_node) return;
    if (gizmo_locked) {
        gizmo_node->set_visible(false);
        return;
    }
    bool show = false;
    if (edit_mode == MODE_OBJECT) {
        if (active_scene_node.is_valid()) {
            show = true;
        } else if (selected_bone >= 0 && show_skeleton) {
            show = true;
        }
    } else {
        show = (selected_vertex >= 0 || selected_edge >= 0 || selected_face >= 0);
    }
    gizmo_node->set_visible(show);
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
    if (skin.is_null()) return;
    if (mesh.is_valid()) skin->set_mesh(mesh);
    if (skeleton.is_valid()) skin->set_skeleton(skeleton);
    skin->set_mesh_transform(get_global_transform());
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
    _update_proportional_cursor();
}

int VFXEditorNode::get_edit_mode() const { return edit_mode; }

void VFXEditorNode::set_show_wireframe(bool show) {
    show_wireframe = show;
    if (!show_wireframe && selection_visual) selection_visual->set_visible(false);
}

bool VFXEditorNode::get_show_wireframe() const { return show_wireframe; }

void VFXEditorNode::clear_selection() {
    selected_faces.clear();
    selected_edges.clear();
    selected_vertices.clear();
    selected_face = -1;
    selected_edge = -1;
    selected_vertex = -1;
    selected_bone = -1;
    last_loop_type = 0;
    _update_gizmo_for_selection();
    if (selection_visual) _build_selection_mesh();
    _build_skeleton_mesh();
}

int VFXEditorNode::get_selected_face() const { return selected_face; }
int VFXEditorNode::get_selected_edge() const { return selected_edge; }
int VFXEditorNode::get_selected_vertex() const { return selected_vertex; }

// ============================================================================
// SELECTION MODE
// ============================================================================
void VFXEditorNode::set_selection_mode(int mode) {
    if (mode < SELECTION_MODE_SINGLE || mode > SELECTION_MODE_LOOP) return;
    selection_mode = mode;
}

int VFXEditorNode::get_selection_mode() const {
    return selection_mode;
}

bool VFXEditorNode::is_face_selected(int idx) const {
    return selected_faces.find(idx) != selected_faces.end();
}

bool VFXEditorNode::is_edge_selected(int idx) const {
    return selected_edges.find(idx) != selected_edges.end();
}

bool VFXEditorNode::is_vertex_selected(int idx) const {
    return selected_vertices.find(idx) != selected_vertices.end();
}

PackedInt32Array VFXEditorNode::get_selected_faces() const {
    PackedInt32Array arr;
    for (int f : selected_faces) arr.push_back(f);
    return arr;
}

PackedInt32Array VFXEditorNode::get_selected_edges() const {
    PackedInt32Array arr;
    for (int e : selected_edges) arr.push_back(e);
    return arr;
}

PackedInt32Array VFXEditorNode::get_selected_vertices() const {
    PackedInt32Array arr;
    for (int v : selected_vertices) arr.push_back(v);
    return arr;
}

void VFXEditorNode::_handle_element_selection(int hit) {
    switch (edit_mode) {
        case MODE_VERTEX: _handle_vertex_selection(hit); break;
        case MODE_EDGE:   _handle_edge_selection(hit);   break;
        case MODE_FACE:   _handle_face_selection(hit);   break;
    }
}

void VFXEditorNode::_handle_vertex_selection(int hit) {
    switch (selection_mode) {
        case SELECTION_MODE_SINGLE:
            clear_selection();
            selected_vertices.insert(hit);
            selected_vertex = hit;
            break;
        case SELECTION_MODE_MULTI:
            if (selected_vertices.find(hit) != selected_vertices.end()) {
                selected_vertices.erase(hit);
                if (selected_vertex == hit) selected_vertex = -1;
            } else {
                selected_vertices.insert(hit);
                selected_vertex = hit;
            }
            break;
        case SELECTION_MODE_LOOP:
            clear_selection();
            if (mesh.is_valid()) {
                PackedInt32Array loop = mesh->get_vertex_loop(hit);
                for (int i = 0; i < loop.size(); i++) selected_vertices.insert(loop[i]);
            } else {
                selected_vertices.insert(hit);
            }
            selected_vertex = hit;
            break;
    }
}

void VFXEditorNode::_handle_edge_selection(int hit) {
    switch (selection_mode) {
        case SELECTION_MODE_SINGLE:
            clear_selection();
            selected_edges.insert(hit);
            selected_edge = hit;
            last_loop_type = 0;
            break;
        case SELECTION_MODE_MULTI:
            if (selected_edges.find(hit) != selected_edges.end()) {
                selected_edges.erase(hit);
                if (selected_edge == hit) { selected_edge = -1; last_loop_type = 0; }
            } else {
                selected_edges.insert(hit);
                selected_edge = hit;
            }
            break;
        case SELECTION_MODE_LOOP:
            if (!mesh.is_valid()) {
                clear_selection();
                selected_edges.insert(hit);
                selected_edge = hit;
                last_loop_type = 0;
                return;
            }
            if (selected_edge == hit && last_loop_type != 0) {
                selected_edges.clear();
                if (last_loop_type == 1) {
                    PackedInt32Array ring = mesh->get_edge_ring(hit);
                    for (int i = 0; i < ring.size(); i++) selected_edges.insert(ring[i]);
                    last_loop_type = 2;
                } else {
                    PackedInt32Array loop = mesh->get_edge_loop(hit);
                    for (int i = 0; i < loop.size(); i++) selected_edges.insert(loop[i]);
                    last_loop_type = 1;
                }
            } else {
                clear_selection();
                PackedInt32Array loop = mesh->get_edge_loop(hit);
                for (int i = 0; i < loop.size(); i++) selected_edges.insert(loop[i]);
                selected_edge = hit;
                last_loop_type = 1;
            }
            break;
    }
}

void VFXEditorNode::_handle_face_selection(int hit) {
    switch (selection_mode) {
        case SELECTION_MODE_SINGLE:
            clear_selection();
            selected_faces.insert(hit);
            selected_face = hit;
            break;
        case SELECTION_MODE_MULTI:
            if (selected_faces.find(hit) != selected_faces.end()) {
                selected_faces.erase(hit);
                if (selected_face == hit) selected_face = -1;
            } else {
                selected_faces.insert(hit);
                selected_face = hit;
            }
            break;
        case SELECTION_MODE_LOOP:
            clear_selection();
            if (mesh.is_valid()) {
                PackedInt32Array loop = mesh->get_face_loop(hit);
                for (int i = 0; i < loop.size(); i++) selected_faces.insert(loop[i]);
            } else {
                selected_faces.insert(hit);
            }
            selected_face = hit;
            break;
    }
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
// CAMERA
// ============================================================================
void VFXEditorNode::set_camera(Camera3D* p_camera) { camera = p_camera; }
Camera3D* VFXEditorNode::get_camera() const { return camera; }

void VFXEditorNode::set_select_pixel_tolerance(float px) { select_pixel_tolerance = MAX(px, 4.0f); }
float VFXEditorNode::get_select_pixel_tolerance() const { return select_pixel_tolerance; }

void VFXEditorNode::set_gizmo_select_pixel_tolerance(float px) { gizmo_select_pixel_tolerance = MAX(px, 2.0f); }
float VFXEditorNode::get_gizmo_select_pixel_tolerance() const { return gizmo_select_pixel_tolerance; }

void VFXEditorNode::set_proportional_editing_enabled(bool enabled) {
    proportional_editing = enabled;
    _update_proportional_cursor();
}
bool VFXEditorNode::get_proportional_editing_enabled() const { return proportional_editing; }

void VFXEditorNode::set_proportional_radius(float radius) {
    proportional_radius = MAX(radius, 0.001f);
    _update_proportional_cursor();
}
float VFXEditorNode::get_proportional_radius() const { return proportional_radius; }

void VFXEditorNode::set_falloff_mode(int mode) {
    if (mode >= FALLOFF_SMOOTH && mode <= FALLOFF_RANDOM) falloff_mode = mode;
}
int VFXEditorNode::get_falloff_mode() const { return falloff_mode; }

// ============================================================================
// GIZMO — SIMPLE ACCESSORS
// ============================================================================
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

// ============================================================================
// SKELETON — SIMPLE ACCESSORS
// ============================================================================
void VFXEditorNode::set_selected_bone(int idx) {
    selected_bone = idx;
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

// ============================================================================
// SYMMETRY
// ============================================================================
void VFXEditorNode::set_symmetry_enabled(bool enabled) { symmetry_enabled = enabled; }
bool VFXEditorNode::get_symmetry_enabled() const { return symmetry_enabled; }
void VFXEditorNode::set_symmetry_axis(int axis) { symmetry_axis = axis; }
int VFXEditorNode::get_symmetry_axis() const { return symmetry_axis; }

// ============================================================================
// SCENE TREE — HIGH LEVEL
// ============================================================================
void VFXEditorNode::set_scene(const Ref<VFXScene>& p_scene) {
    scene = p_scene;
    active_scene_node = Ref<VFXSceneNode>();
    _clear_scene_visuals();
    if (scene.is_valid()) {
        _ensure_scene_container();
        if (!scene->get_root().is_valid()) scene->create_default_root();
        if (mesh_instance) mesh_instance->set_visible(false);
    } else {
        if (mesh_instance) mesh_instance->set_visible(true);
    }
    if (scene_tree_panel) {
        scene_tree_panel->set_scene(scene);
    }
    mark_scene_dirty();
}

Ref<VFXScene> VFXEditorNode::get_scene() const { return scene; }

void VFXEditorNode::set_active_scene_node(const Ref<VFXSceneNode>& p_node) {
    active_scene_node = p_node;

    if (active_scene_node.is_null()) {
        clear_selection();
        _update_gizmo_visibility();
        mark_scene_dirty();
        return;
    }

    // Swap viewport content from the scene node (shared Refs, so edits propagate back)
    set_vfx_mesh(active_scene_node->get_mesh());
    set_vfx_skeleton(active_scene_node->get_skeleton());
    set_vfx_skin(active_scene_node->get_skin());
    set_vfx_animator(active_scene_node->get_animator());

    // Update transform gizmo to match node's world transform
    set_gizmo_transform(active_scene_node->get_global_transform());

    // Rebuild gizmo and clear mesh selection
    if (gizmo_node) {
        _build_gizmo_mesh();
    }
    clear_selection();
    _update_gizmo_visibility();
    mark_scene_dirty();
}

Ref<VFXSceneNode> VFXEditorNode::get_active_scene_node() const { return active_scene_node; }

bool VFXEditorNode::import_model(const String& filepath, const Ref<VFXSceneNode>& parent) {
    if (scene.is_null()) return false;
    bool ok = scene->import_model(filepath, parent);
    if (ok) mark_scene_dirty();
    return ok;
}

Ref<VFXSceneNode> VFXEditorNode::raycast_scene_node(const Vector3& ray_origin, const Vector3& ray_dir) {
    if (scene.is_null()) return Ref<VFXSceneNode>();
    float closest = 1e20f;
    Ref<VFXSceneNode> best;

    Array nodes = scene->flatten_tree();
    for (int i = 0; i < nodes.size(); i++) {
        Ref<VFXSceneNode> sn = nodes[i];
        if (sn.is_null() || sn->get_node_type() != VFXSceneNode::NODE_MESH) continue;
        if (!sn->is_visible()) continue;

        Ref<VFXMesh> vmesh = sn->get_mesh();
        if (vmesh.is_null()) continue;

        Transform3D global = sn->get_global_transform();
        Transform3D inv = global.affine_inverse();
        Vector3 local_origin = inv.xform(ray_origin);
        Vector3 local_dir = inv.basis.xform(ray_dir).normalized();

        Vector3 hit;
        if (vmesh->raycast(local_origin, local_dir, hit, closest)) {
            float dist = global.xform(hit).distance_to(ray_origin);
            if (dist < closest) {
                closest = dist;
                best = sn;
            }
        }
    }
    return best;
}

// ============================================================================
// SCENE VISUALS
// ============================================================================
MeshInstance3D* VFXEditorNode::_get_scene_visual(uint64_t node_id) {
    MeshInstance3D** ptr = scene_visuals.getptr(node_id);
    if (ptr) return *ptr;
    MeshInstance3D* vis = memnew(MeshInstance3D);
    vis->set_name("SceneVisual_" + String::num_int64(node_id));
    scene_container->add_child(vis);
    vis->set_owner(scene_container);
    scene_visuals.insert(node_id, vis);
    return vis;
}

void VFXEditorNode::_clear_scene_visuals() {
    for (auto& pair : scene_visuals) {
        if (pair.value) memdelete(pair.value);
    }
    scene_visuals.clear();
}

Ref<ArrayMesh> VFXEditorNode::_build_array_mesh_for_node(const Ref<VFXMesh>& p_mesh, const Ref<VFXSkeleton>& p_sk, const Ref<VFXSkin>& p_skin, bool p_show_weights, int p_viz_bone) {
    if (p_mesh.is_null()) return Ref<ArrayMesh>();
    Ref<ArrayMesh> am;
    am.instantiate();

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);

    PackedVector3Array positions;
    if (p_skin.is_valid() && p_sk.is_valid() && p_sk->get_bone_count() > 0 && !p_show_weights) {
        p_sk->update_transforms();
        p_skin->set_mesh_transform(Transform3D());
        positions = p_skin->compute_skinned_positions();
    } else {
        positions = p_mesh->get_positions();
    }
    arrays[Mesh::ARRAY_VERTEX] = positions;

    PackedVector3Array normals = p_mesh->get_normals();
    for (int i = 0; i < normals.size(); i++) {
        if (normals[i].length_squared() < 0.0001f) normals[i] = Vector3(0, 1, 0);
        else normals[i] = normals[i].normalized();
    }
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_TEX_UV] = p_mesh->get_uvs();
    arrays[Mesh::ARRAY_COLOR] = p_mesh->get_colors();
    arrays[Mesh::ARRAY_INDEX] = p_mesh->get_indices();

    if (!p_show_weights && p_skin.is_valid() && p_sk.is_valid() && p_sk->get_bone_count() > 0) {
        // Pre-skinned, no bone attributes needed
    } else {
        int vc = positions.size();
        if (vc > 0) {
            PackedInt32Array bones;
            PackedFloat32Array weights;
            bones.resize(vc * 4);
            weights.resize(vc * 4);
            for (int i = 0; i < vc; i++) {
                int b[4]; float w[4];
                p_mesh->get_vertex_skinning(i, b, w);
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

    if (p_show_weights && p_skin.is_valid()) {
        if (am->get_surface_count() > 0) {
            PackedColorArray colors = p_skin->get_weight_visualization(p_viz_bone);
            Array a = am->surface_get_arrays(0);
            PackedVector3Array verts = a[Mesh::ARRAY_VERTEX];
            if (colors.size() == verts.size()) {
                a[Mesh::ARRAY_COLOR] = colors;
                am->clear_surfaces();
                am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, a);
            }
        }
    }
    return am;
}

void VFXEditorNode::_sync_scene_visuals() {
    if (scene.is_null() || !scene_container) return;
    if (!scene->get_root().is_valid()) return;

    std::unordered_set<uint64_t> used;
    _sync_node_visual_recursive(scene->get_root(), used);

    // Hide visuals for nodes no longer in the tree or that became invisible
    for (const auto& pair : scene_visuals) {
        if (used.find(pair.key) == used.end()) {
            pair.value->set_visible(false);
        }
    }
}

void VFXEditorNode::_sync_node_visual_recursive(const Ref<VFXSceneNode>& p_node, std::unordered_set<uint64_t>& r_used) {
    if (p_node.is_null() || !p_node->is_visible()) return;

    if (p_node->get_node_type() == VFXSceneNode::NODE_MESH && p_node->has_mesh()) {
        uint64_t id = p_node->get_instance_id();
        r_used.insert(id);

        MeshInstance3D* visual = _get_scene_visual(id);
        visual->set_visible(true);
        visual->set_transform(p_node->get_global_transform());

        Ref<VFXMesh> vmesh = p_node->get_mesh();
        Ref<ArrayMesh> am = _build_array_mesh_for_node(vmesh, p_node->get_skeleton(), p_node->get_skin(), show_weights, visualize_bone);
        if (am.is_valid()) {
            visual->set_mesh(am);
            if (show_weights && p_node->get_skin().is_valid()) {
                visual->set_surface_override_material(0, weight_material);
            } else {
                visual->set_surface_override_material(0, base_material);
            }
        }
    }

    for (int i = 0; i < p_node->get_child_count(); ++i) {
        _sync_node_visual_recursive(p_node->get_child(i), r_used);
    }
}

// ============================================================================
// UNIFIED TOUCH API
// ============================================================================
int VFXEditorNode::on_touch_down(const Vector3& ray_origin, const Vector3& ray_dir, const Vector2& screen_pos) {
    // === SCENE MODE (OBJECT-level selection) ===
    if (scene.is_valid() && edit_mode == MODE_OBJECT) {
        if (!gizmo_locked && active_scene_node.is_valid() && gizmo_node && gizmo_node->is_visible()) {
            int axis;
            if (camera && screen_pos.x >= 0.0f)
                axis = screen_raycast_gizmo(screen_pos);
            else
                axis = raycast_gizmo(ray_origin, ray_dir);

            if (axis >= 0) {
                gizmo_begin_drag(axis, ray_origin, ray_dir);
                return -2;
            }
        }

        Ref<VFXSceneNode> hit = raycast_scene_node(ray_origin, ray_dir);
        if (hit.is_valid()) {
            set_active_scene_node(hit);
            return SCENE_NODE_HIT;
        }

        set_active_scene_node(Ref<VFXSceneNode>());
        return -1;
    }

    // === MESH EDIT MODE ===
    if (edit_mode != MODE_OBJECT && mesh.is_valid()) {
        if (!gizmo_locked && (selected_vertex >= 0 || selected_edge >= 0 || selected_face >= 0) && gizmo_node && gizmo_node->is_visible()) {
            int axis;
            if (camera && screen_pos.x >= 0.0f)
                axis = screen_raycast_gizmo(screen_pos);
            else
                axis = raycast_gizmo(ray_origin, ray_dir);

            if (axis >= 0) {
                gizmo_begin_drag(axis, ray_origin, ray_dir);
                return -2;
            }
        }

        int hit = -1;
        if (camera && screen_pos.x >= 0.0f) {
            switch (edit_mode) {
                case MODE_VERTEX: hit = screen_select_vertex(screen_pos); break;
                case MODE_EDGE:   hit = screen_select_edge(screen_pos); break;
                case MODE_FACE:   hit = screen_select_face(screen_pos); break;
            }
        } else {
            hit = raycast_select(ray_origin, ray_dir);
        }

        if (hit >= 0) {
            _handle_element_selection(hit);
            _build_selection_mesh();
            _update_gizmo_for_selection();
            return MESH_ELEMENT_HIT;
        }

        if (selection_mode == SELECTION_MODE_SINGLE) {
            clear_selection();
        } else {
            // Multi/Loop: hide gizmo on empty click so next click isn't stolen by gizmo raycast
            if (gizmo_node) gizmo_node->set_visible(false);
        }
        return -1;
    }

    // === SKELETON MODE ===
    if (show_skeleton) {
        if (!gizmo_locked && skeleton.is_valid() && selected_bone >= 0) {
            int axis;
            if (camera && screen_pos.x >= 0.0f)
                axis = screen_raycast_gizmo(screen_pos);
            else
                axis = raycast_gizmo(ray_origin, ray_dir);

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


void VFXEditorNode::refresh_scene() {
    mark_scene_dirty();
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
