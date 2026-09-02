#include "vfx_uv_editor_viewport.h"
#include "vfx_math.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_screen_touch.hpp>
#include <godot_cpp/classes/input_event_screen_drag.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_set>

using namespace godot;

// ============================================================================
// BINDINGS
// ============================================================================
void VFXUVEditorViewport::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXUVEditorViewport::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXUVEditorViewport::get_mesh);
    ClassDB::bind_method(D_METHOD("set_current_tool", "tool"), &VFXUVEditorViewport::set_current_tool);
    ClassDB::bind_method(D_METHOD("get_current_tool"), &VFXUVEditorViewport::get_current_tool);
    ClassDB::bind_method(D_METHOD("set_select_mode", "mode"), &VFXUVEditorViewport::set_select_mode);
    ClassDB::bind_method(D_METHOD("get_select_mode"), &VFXUVEditorViewport::get_select_mode);

    ClassDB::bind_integer_constant(get_class_static(), "", "TOOL_SELECT", TOOL_SELECT);
    ClassDB::bind_integer_constant(get_class_static(), "", "TOOL_TRANSLATE", TOOL_TRANSLATE);
    ClassDB::bind_integer_constant(get_class_static(), "", "TOOL_ROTATE", TOOL_ROTATE);
    ClassDB::bind_integer_constant(get_class_static(), "", "TOOL_SCALE", TOOL_SCALE);
}

// ============================================================================
// LIFECYCLE
// ============================================================================
VFXUVEditorViewport::VFXUVEditorViewport() {
    uv_editor.instantiate();
    set_clip_contents(true);
}

void VFXUVEditorViewport::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY) {
        // uv_editor already instantiated in constructor
    }
}

// ============================================================================
// PROPERTIES
// ============================================================================
void VFXUVEditorViewport::set_mesh(const Ref<VFXMesh>& p_mesh) {
    Ref<VFXMesh> m = p_mesh;
    if (m.is_valid() && m->get_uv_layer_count() == 0) {
        m->add_uv_layer();
        m->sync_uv_layers();
    }
    uv_editor->set_mesh(m);
    uv_editor->set_active_layer(0);
    if (m.is_valid()) {
        uv_editor->deselect_all();
    }
    _cache_dirty = true;
    queue_redraw();
}


Ref<VFXMesh> VFXUVEditorViewport::get_mesh() const {
    return uv_editor->get_mesh();
}

void VFXUVEditorViewport::set_current_tool(int p_tool) {
    current_tool = p_tool;
    queue_redraw();
}

int VFXUVEditorViewport::get_current_tool() const {
    return current_tool;
}

void VFXUVEditorViewport::set_select_mode(int p_mode) {
    select_mode = p_mode;
    _request_cache_rebuild();
}

int VFXUVEditorViewport::get_select_mode() const {
    return select_mode;
}

// ============================================================================
// COORDINATE MATH
// ============================================================================
Vector2 VFXUVEditorViewport::screen_to_uv(const Vector2& p_screen) const {
    return (p_screen - get_size() * 0.5f) / zoom + pan_offset;
}

Vector2 VFXUVEditorViewport::uv_to_screen(const Vector2& p_uv) const {
    return (p_uv - pan_offset) * zoom + get_size() * 0.5f;
}

// ============================================================================
// INPUT
// ============================================================================
void VFXUVEditorViewport::_gui_input(const Ref<InputEvent>& p_event) {
    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid()) {
        if (mb->get_button_index() == MOUSE_BUTTON_LEFT && !_touches.empty())
            return;
    }
    Ref<InputEventMouseMotion> mm = p_event;
    if (mm.is_valid() && !_touches.empty())
        return;

    if (mb.is_valid()) {
        if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP && mb->is_pressed()) {
            _zoom_at(mb->get_position(), 1.15f);
        } else if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN && mb->is_pressed()) {
            _zoom_at(mb->get_position(), 0.87f);
        } else if (mb->get_button_index() == MOUSE_BUTTON_LEFT) {
            if (mb->is_pressed()) {
                _on_left_down(mb->get_position(), mb->is_shift_pressed());
            } else {
                _on_left_up(mb->get_position());
            }
        } else if (mb->get_button_index() == MOUSE_BUTTON_RIGHT) {
            _is_navigating = mb->is_pressed();
        }
    }
    else if (mm.is_valid()) {
        if (_is_navigating) {
            pan_offset -= mm->get_relative() / zoom;
            queue_redraw();
        } else if (_is_selecting) {
            _select_current = mm->get_position();
            queue_redraw();
        } else if (_is_transforming) {
            _on_transform_drag(mm->get_position());
            queue_redraw();
        }
    }

    Ref<InputEventScreenTouch> st = p_event;
    if (st.is_valid()) {
        if (st->is_pressed()) {
            bool was_empty = _touches.empty();
            _touches[st->get_index()] = st->get_position();
            if (_touches.size() == 1 && was_empty) {
                _ignore_remaining_drag = false;
                _on_left_down(st->get_position(), false);
            } else if (_touches.size() == 2) {
                _cancel_single_touch_actions();
                _ignore_remaining_drag = false;
                Vector2 p0, p1;
                int count = 0;
                for (const auto& pair : _touches) {
                    if (count == 0) p0 = pair.second;
                    else if (count == 1) p1 = pair.second;
                    count++;
                }
                _last_pinch_dist = p0.distance_to(p1);
                _last_pinch_center = (p0 + p1) * 0.5f;
            }
        } else {
            _touches.erase(st->get_index());
            if (_touches.empty()) {
                _last_pinch_dist = -1.0f;
                _ignore_remaining_drag = false;
                if (_is_selecting || _is_transforming) {
                    _on_left_up(st->get_position());
                }
            } else if (_touches.size() == 1) {
                _last_pinch_dist = -1.0f;
                _ignore_remaining_drag = true;
            }
        }
    }

    Ref<InputEventScreenDrag> sd = p_event;
    if (sd.is_valid()) {
        _touches[sd->get_index()] = sd->get_position();
        if (_touches.size() == 2) {
            Vector2 p0, p1;
            int count = 0;
            for (const auto& pair : _touches) {
                if (count == 0) p0 = pair.second;
                else if (count == 1) p1 = pair.second;
                count++;
            }
            float dist = p0.distance_to(p1);
            Vector2 center = (p0 + p1) * 0.5f;
            if (_last_pinch_dist > 0.0f) {
                float factor = dist / _last_pinch_dist;
                _zoom_at(center, factor);
                pan_offset -= (center - _last_pinch_center) / zoom;
                queue_redraw();
            }
            _last_pinch_dist = dist;
            _last_pinch_center = center;
        } else if (_touches.size() == 1 && !_ignore_remaining_drag) {
            if (_is_selecting) {
                _select_current = sd->get_position();
                queue_redraw();
            } else if (_is_transforming) {
                _on_transform_drag(sd->get_position());
                queue_redraw();
            }
        }
    }
}

// ============================================================================
// INTERACTION HELPERS
// ============================================================================
void VFXUVEditorViewport::_on_left_down(const Vector2& p_pos, bool p_shift) {
    Vector2 uv = screen_to_uv(p_pos);
    if (current_tool == TOOL_SELECT) {
        _is_selecting = true;
        _select_start = p_pos;
        _select_current = p_pos;
        if (!p_shift) {
            uv_editor->deselect_all();
            _request_cache_rebuild();
        }
        _select_at(uv, p_shift);
        queue_redraw();
    } else {
        if (uv_editor->get_selected_verts().size() == 0) {
            if (!p_shift) {
                uv_editor->deselect_all();
                _request_cache_rebuild();
            }
            _select_at(uv, p_shift);
        }
        if (uv_editor->get_selected_verts().size() > 0) {
            _begin_transform(uv);
        }
    }
}

void VFXUVEditorViewport::_on_left_up(const Vector2& p_pos) {
    if (_is_selecting) {
        float drag_dist = _select_start.distance_to(_select_current);
        if (drag_dist > 3.0f) {
            Rect2 r(_select_start, _select_current - _select_start);
            r = r.abs();
            Vector2 uv0 = screen_to_uv(r.position);
            Vector2 uv1 = screen_to_uv(r.position + r.size);
            Rect2 box(uv0, uv1 - uv0);
            box = box.abs();
            uv_editor->select_box(box, true);
            _request_cache_rebuild();
            queue_redraw();
        }
        _is_selecting = false;
        queue_redraw();
    }
    if (_is_transforming) {
        _is_transforming = false;
        _transform_starts.clear();
        queue_redraw();
    }
}

void VFXUVEditorViewport::_select_at(const Vector2& p_uv, bool p_add) {
    switch (select_mode) {
        case VFXUVEditor::UV_SELECT_VERTEX:
            uv_editor->select_at(p_uv, 4.0f / zoom, p_add);
            break;
        case VFXUVEditor::UV_SELECT_EDGE:
            uv_editor->select_edge_at(p_uv, 4.0f / zoom, p_add);
            break;
        case VFXUVEditor::UV_SELECT_FACE:
            uv_editor->select_face_at(p_uv, p_add);
            break;
        case VFXUVEditor::UV_SELECT_ISLAND:
            uv_editor->select_island_at(p_uv);
            break;
    }
    _request_cache_rebuild();
}

void VFXUVEditorViewport::_begin_transform(const Vector2& p_uv) {
    _is_transforming = true;
    _transform_start_uv = p_uv;
    _transform_starts.clear();
    PackedInt32Array sel = uv_editor->get_selected_verts();
    Vector2 center;
    for (int i = 0; i < sel.size(); i++) {
        center += uv_editor->get_uv_vert(sel[i]);
    }
    if (sel.size() > 0) {
        center /= (float)sel.size();
    }
    _transform_pivot = center;
    for (int i = 0; i < sel.size(); i++) {
        int idx = sel[i];
        _transform_starts[idx] = uv_editor->get_uv_vert(idx);
    }
}

void VFXUVEditorViewport::_on_transform_drag(const Vector2& p_pos) {
    Vector2 current_uv = screen_to_uv(p_pos);
    switch (current_tool) {
        case TOOL_TRANSLATE: {
            uv_editor->translate_selected(current_uv - _transform_start_uv);
            _transform_start_uv = current_uv;
            _request_cache_rebuild();
            break;
        }
        case TOOL_ROTATE: {
            float a0 = (_transform_start_uv - _transform_pivot).angle();
            float a1 = (current_uv - _transform_pivot).angle();
            uv_editor->rotate_selected(a1 - a0, _transform_pivot);
            _transform_start_uv = current_uv;
            _request_cache_rebuild();
            break;
        }
        case TOOL_SCALE: {
            Vector2 local0 = _transform_start_uv - _transform_pivot;
            Vector2 local1 = current_uv - _transform_pivot;
            Vector2 scale(1.0f, 1.0f);
            if (std::fabs(local0.x) > 0.0001f) scale.x = local1.x / local0.x;
            if (std::fabs(local0.y) > 0.0001f) scale.y = local1.y / local0.y;
            scale.x = vfx::clampf(scale.x, 0.001f, 1000.0f);
            scale.y = vfx::clampf(scale.y, 0.001f, 1000.0f);
            for (const auto& pair : _transform_starts) {
                int idx = pair.first;
                Vector2 local = pair.second - _transform_pivot;
                uv_editor->set_uv_vert(idx, Vector2(local.x * scale.x, local.y * scale.y) + _transform_pivot);
            }
            _request_cache_rebuild();
            break;
        }
        default:
            break;
    }
}

void VFXUVEditorViewport::_zoom_at(const Vector2& p_screen_pos, float p_factor) {
    Vector2 uv_before = screen_to_uv(p_screen_pos);
    zoom *= p_factor;
    zoom = vfx::clampf(zoom, 16.0f, 8192.0f);
    Vector2 uv_after = screen_to_uv(p_screen_pos);
    pan_offset += uv_before - uv_after;
    _request_cache_rebuild();
}

void VFXUVEditorViewport::_cancel_single_touch_actions() {
    _is_selecting = false;
    _is_transforming = false;
    _transform_starts.clear();
    queue_redraw();
}

void VFXUVEditorViewport::_request_cache_rebuild() {
    _cache_dirty = true;
    queue_redraw();
}

// ============================================================================
// CACHE
// ============================================================================
void VFXUVEditorViewport::_rebuild_cache() {
    _cached_faces.clear();
    if (uv_editor.is_null() || uv_editor->get_mesh().is_null()) {
        _cache_dirty = false;
        return;
    }

    int fc = uv_editor->get_mesh()->get_face_count();
    for (int i = 0; i < fc; i++) {
        PackedVector2Array poly = uv_editor->get_face_uv_polygon(i);
        if (poly.size() < 3) continue;

        CachedFace cf;
        cf.uv_poly = poly;
        cf.screen_poly = PackedVector2Array();

        Vector2 bmin(1e30f, 1e30f);
        Vector2 bmax(-1e30f, -1e30f);
        Vector2 c;

        for (int j = 0; j < poly.size(); j++) {
            Vector2 sp = uv_to_screen(poly[j]);
            cf.screen_poly.push_back(sp);
            c += sp;
            bmin.x = std::min(bmin.x, sp.x);
            bmin.y = std::min(bmin.y, sp.y);
            bmax.x = std::max(bmax.x, sp.x);
            bmax.y = std::max(bmax.y, sp.y);
        }
        cf.center = c / (float)poly.size();
        cf.bounds = Rect2(bmin, bmax - bmin).grow(2.0f);
        _cached_faces.push_back(cf);
    }
    _cache_dirty = false;
}


// ============================================================================
// DRAW DATA CACHE — rebuilt only when _cache_dirty
// ============================================================================
void VFXUVEditorViewport::_rebuild_draw_data() {
    _cached_sel_lines.clear();
    _cached_sel_colors.clear();
    _cached_unsel_lines.clear();
    _cached_unsel_colors.clear();
    _cached_face_dots.clear();
    _cached_face_dot_colors.clear();
    _cached_vert_dots.clear();
    _cached_vert_dot_colors.clear();
    _cached_vert_rings.clear();
    _cached_vert_ring_colors.clear();
    _cached_sel_face_set.clear();

    if (uv_editor.is_null() || uv_editor->get_mesh().is_null()) return;

    Rect2 screen_rect(Vector2(), get_size());
    PackedInt32Array sel_faces = uv_editor->get_selected_faces();

    for (int i = 0; i < sel_faces.size(); i++) {
        _cached_sel_face_set.insert(sel_faces[i]);
    }

    // Wireframe
    for (int i = 0; i < _cached_faces.size(); i++) {
        const CachedFace& cf = _cached_faces[i];
        if (!screen_rect.intersects(cf.bounds)) continue;

        bool is_sel = _cached_sel_face_set.count(i) > 0;
        const PackedVector2Array& sp = cf.screen_poly;
        int n = sp.size();
        for (int j = 0; j < n; j++) {
            Vector2 a = sp[j];
            Vector2 b = sp[(j + 1) % n];
            if (is_sel) {
                _cached_sel_lines.push_back(a);
                _cached_sel_lines.push_back(b);
                _cached_sel_colors.push_back(Color(1.0f, 0.45f, 0.08f));
            } else {
                _cached_unsel_lines.push_back(a);
                _cached_unsel_lines.push_back(b);
                _cached_unsel_colors.push_back(Color(0.45f, 0.45f, 0.50f));
            }
        }
    }

    // Face-center dots
    if (select_mode == VFXUVEditor::UV_SELECT_FACE) {
        for (int i = 0; i < _cached_faces.size(); i++) {
            const CachedFace& cf = _cached_faces[i];
            if (!screen_rect.intersects(cf.bounds)) continue;
            bool is_sel = _cached_sel_face_set.count(i) > 0;
            _cached_face_dots.push_back(cf.center);
            _cached_face_dot_colors.push_back(is_sel ? Color(1.0f, 0.45f, 0.08f) : Color(0.35f, 0.35f, 0.40f));
        }
    }

    // Vertices — NO rings for unselected, 8-seg rings only for selected
    if (select_mode == VFXUVEditor::UV_SELECT_VERTEX) {
        int vc = uv_editor->get_uv_vert_count();
        for (int i = 0; i < vc; i++) {
            Vector2 pos = uv_to_screen(uv_editor->get_uv_vert(i));
            if (uv_editor->is_uv_selected(i)) {
                _cached_vert_dots.push_back(pos);
                _cached_vert_dot_colors.push_back(Color(1.0f, 0.45f, 0.08f));
                _batch_arc_outline(_cached_vert_rings, _cached_vert_ring_colors, pos, 5.5f, 8, Color(1, 1, 1));
            } else {
                _cached_vert_dots.push_back(pos);
                _cached_vert_dot_colors.push_back(Color(0.18f, 0.18f, 0.22f));
                // NO ring for unselected — this was the FPS killer
            }
        }
    } else {
        PackedInt32Array sel = uv_editor->get_selected_verts();
        for (int i = 0; i < sel.size(); i++) {
            int idx = sel[i];
            if (idx < 0 || idx >= uv_editor->get_uv_vert_count()) continue;
            Vector2 pos = uv_to_screen(uv_editor->get_uv_vert(idx));
            _cached_vert_dots.push_back(pos);
            _cached_vert_dot_colors.push_back(Color(1.0f, 0.45f, 0.08f));
            _batch_arc_outline(_cached_vert_rings, _cached_vert_ring_colors, pos, 5.5f, 8, Color(1, 1, 1));
        }
    }
}

// ============================================================================
// BATCHED DRAWING HELPER
// ============================================================================
void VFXUVEditorViewport::_batch_arc_outline(PackedVector2Array& r_lines, PackedColorArray& r_colors,
                               const Vector2& center, float radius, int segments,
                               const Color& color) {
    float step = 6.283185307179586f / segments;
    for (int i = 0; i < segments; i++) {
        float a0 = i * step;
        float a1 = ((i + 1) % segments) * step;
        r_lines.push_back(center + Vector2(cosf(a0), sinf(a0)) * radius);
        r_lines.push_back(center + Vector2(cosf(a1), sinf(a1)) * radius);
        r_colors.push_back(color);
    }
}

// ============================================================================
// DRAWING (OPTIMIZED — cached batched arrays)
// ============================================================================
void VFXUVEditorViewport::_draw() {
    draw_rect(Rect2(Vector2(), get_size()), Color(0.08f, 0.08f, 0.08f, 1.0f));

    if (uv_editor.is_null() || uv_editor->get_mesh().is_null()) {
        Ref<Font> font = get_theme_default_font();
        if (font.is_valid()) {
            draw_string(font, Vector2(20, 32), "No mesh assigned",
                HORIZONTAL_ALIGNMENT_LEFT, -1.0f, 16, Color(0.7f, 0.7f, 0.7f));
        }
        return;
    }

    _draw_uv_grid();

    if (_cache_dirty) {
        _rebuild_cache();
        _rebuild_draw_data();
    }

    Rect2 screen_rect(Vector2(), get_size());

    // 1. Face fills
    for (int i = 0; i < _cached_faces.size(); i++) {
        const CachedFace& cf = _cached_faces[i];
        if (!screen_rect.intersects(cf.bounds)) continue;
        bool is_sel = _cached_sel_face_set.count(i) > 0;
        Color fill = is_sel ? Color(0.22f, 0.17f, 0.12f, 0.90f) : Color(0.16f, 0.14f, 0.12f, 0.85f);
        draw_colored_polygon(cf.screen_poly, fill);
    }

    // 2. Wireframe
    if (_cached_sel_lines.size() > 0)
        draw_multiline_colors(_cached_sel_lines, _cached_sel_colors, 1.2f);
    if (_cached_unsel_lines.size() > 0)
        draw_multiline_colors(_cached_unsel_lines, _cached_unsel_colors, 0.8f);

    // 3. Face-center dots
    if (select_mode == VFXUVEditor::UV_SELECT_FACE) {
        for (int i = 0; i < _cached_face_dots.size(); i++) {
            draw_rect(Rect2(_cached_face_dots[i] - Vector2(2.5f, 2.5f), Vector2(5, 5)), _cached_face_dot_colors[i]);
        }
    }

    // 4. Vertices — selected = filled orange circles, unselected = tiny 3x3 rects
    if (select_mode == VFXUVEditor::UV_SELECT_VERTEX) {
        for (int i = 0; i < _cached_vert_dots.size(); i++) {
            bool is_selected = _cached_vert_dot_colors[i].r > 0.5f;
            if (is_selected) {
                draw_circle(_cached_vert_dots[i], 4.0f, Color(1.0f, 0.45f, 0.08f));
            } else {
                draw_rect(Rect2(_cached_vert_dots[i] - Vector2(1.5f, 1.5f), Vector2(3, 3)), _cached_vert_dot_colors[i]);
            }
        }
        if (_cached_vert_rings.size() > 0)
            draw_multiline_colors(_cached_vert_rings, _cached_vert_ring_colors, 1.0f);
    } else {
        for (int i = 0; i < _cached_vert_dots.size(); i++) {
            draw_circle(_cached_vert_dots[i], 4.0f, _cached_vert_dot_colors[i]);
        }
        if (_cached_vert_rings.size() > 0)
            draw_multiline_colors(_cached_vert_rings, _cached_vert_ring_colors, 1.0f);
    }

    // 5. Selection box / gizmo
    if (_is_selecting) {
        Rect2 r(_select_start, _select_current - _select_start);
        r = r.abs();
        draw_rect(r, Color(0.25f, 0.65f, 1.0f, 0.15f), true);
        draw_rect(r, Color(0.45f, 0.85f, 1.0f), false, 1.0f);
    }

    if (current_tool != TOOL_SELECT && uv_editor->get_selected_verts().size() > 0) {
        _draw_gizmo();
    }
}


// ============================================================================
// GRID (OPTIMIZED — batched into 1 draw call for lines)
// ============================================================================
void VFXUVEditorViewport::_draw_uv_grid() {
    // ------------------------------------------------------------------
    // Island-aware grid: only draw inside the UV island bounding box
    // ------------------------------------------------------------------
    Rect2 island_bounds;
    if (uv_editor.is_valid() && uv_editor->get_mesh().is_valid()) {
        island_bounds = uv_editor->get_total_bounds();
    }

    // If nothing is loaded, just fill the viewport with the dark background
    if (island_bounds.size.x < 0.0001f || island_bounds.size.y < 0.0001f) {
        draw_rect(Rect2(Vector2(), get_size()), Color(0.08f, 0.08f, 0.08f, 1.0f));
        return;
    }

    // Visible area in UV space
    Vector2 uv0 = screen_to_uv(Vector2());
    Vector2 uv1 = screen_to_uv(get_size());
    Rect2 view_rect(uv0, uv1 - uv0);
    view_rect = view_rect.abs();

    // Intersect view with island box so we only iterate useful grid lines
    float gx0 = std::floor(std::max(view_rect.position.x, island_bounds.position.x));
    float gy0 = std::floor(std::max(view_rect.position.y, island_bounds.position.y));
    float gx1 = std::ceil(std::min(view_rect.position.x + view_rect.size.x,
                                   island_bounds.position.x + island_bounds.size.x));
    float gy1 = std::ceil(std::min(view_rect.position.y + view_rect.size.y,
                                   island_bounds.position.y + island_bounds.size.y));

    // Background fill limited to the island box (on-screen)
    Vector2 island_scr_min = uv_to_screen(island_bounds.position);
    Vector2 island_scr_max = uv_to_screen(island_bounds.position + island_bounds.size);
    Rect2 island_screen_rect(island_scr_min, island_scr_max - island_scr_min);
    island_screen_rect = island_screen_rect.abs();
    draw_rect(island_screen_rect, Color(0.10f, 0.10f, 0.10f));

    // Batch ALL grid lines into a single draw_multiline_colors call
    PackedVector2Array grid_lines;
    PackedColorArray grid_colors;

    auto add_seg = [&](const Vector2& a, const Vector2& b, const Color& c) {
        grid_lines.push_back(a);
        grid_lines.push_back(b);
        grid_colors.push_back(c);
    };

    // Helper: clip a vertical or horizontal line to the island screen rect
    auto clip_vline = [&](float sx, const Color& c) {
        if (sx < island_screen_rect.position.x || sx > island_screen_rect.position.x + island_screen_rect.size.x)
            return;
        add_seg(Vector2(sx, island_screen_rect.position.y),
                Vector2(sx, island_screen_rect.position.y + island_screen_rect.size.y), c);
    };
    auto clip_hline = [&](float sy, const Color& c) {
        if (sy < island_screen_rect.position.y || sy > island_screen_rect.position.y + island_screen_rect.size.y)
            return;
        add_seg(Vector2(island_screen_rect.position.x, sy),
                Vector2(island_screen_rect.position.x + island_screen_rect.size.x, sy), c);
    };

    // Medium grid (7 divisions)
    if (zoom > 80.0f) {
        Color med(0.16f, 0.16f, 0.16f);
        for (int x = (int)gx0; x < (int)gx1; x++) {
            for (int i = 1; i < 7; i++) {
                float t = (float)i / 7.0f;
                clip_vline(uv_to_screen(Vector2(x + t, 0.0f)).x, med);
            }
        }
        for (int y = (int)gy0; y < (int)gy1; y++) {
            for (int i = 1; i < 7; i++) {
                float t = (float)i / 7.0f;
                clip_hline(uv_to_screen(Vector2(0.0f, y + t)).y, med);
            }
        }
    }

    // Fine grid (49 divisions)
    if (zoom > 400.0f) {
        Color fine(0.13f, 0.13f, 0.13f);
        float step = 1.0f / 49.0f;

        float fx = std::floor(gx0 / step) * step;
        while (fx <= gx1 + step) {
            bool major = std::fabs(std::fmod(fx, 1.0)) < 0.001;
            bool medium = std::fabs(std::fmod(fx * 7.0, 1.0)) < 0.001;
            if (!major && !medium)
                clip_vline(uv_to_screen(Vector2(fx, 0.0f)).x, fine);
            fx += step;
        }

        float fy = std::floor(gy0 / step) * step;
        while (fy <= gy1 + step) {
            bool major = std::fabs(std::fmod(fy, 1.0)) < 0.001;
            bool medium = std::fabs(std::fmod(fy * 7.0, 1.0)) < 0.001;
            if (!major && !medium)
                clip_hline(uv_to_screen(Vector2(0.0f, fy)).y, fine);
            fy += step;
        }
    }

    // Major integer borders — only inside the island box
    for (int x = (int)gx0; x <= (int)gx1; x++) {
        clip_vline(uv_to_screen(Vector2(x, 0.0f)).x, Color(0.22f, 0.22f, 0.24f));
    }
    for (int y = (int)gy0; y <= (int)gy1; y++) {
        clip_hline(uv_to_screen(Vector2(0.0f, y)).y, Color(0.22f, 0.22f, 0.24f));
    }

    // Island bounds border (replaces the old 0..1 border)
    Vector2 b0 = island_screen_rect.position;
    Vector2 b1 = island_screen_rect.position + island_screen_rect.size;
    add_seg(b0, Vector2(b1.x, b0.y), Color(0.35f, 0.35f, 0.40f));
    add_seg(Vector2(b1.x, b0.y), b1, Color(0.35f, 0.35f, 0.40f));
    add_seg(b1, Vector2(b0.x, b1.y), Color(0.35f, 0.35f, 0.40f));
    add_seg(Vector2(b0.x, b1.y), b0, Color(0.35f, 0.35f, 0.40f));

    if (grid_lines.size() > 0)
        draw_multiline_colors(grid_lines, grid_colors, 1.0f);
}

// ============================================================================
// GIZMO
// ============================================================================
void VFXUVEditorViewport::_draw_gizmo() {
    PackedInt32Array sel = uv_editor->get_selected_verts();
    if (sel.size() == 0) return;

    Vector2 center;
    for (int i = 0; i < sel.size(); i++) {
        center += uv_editor->get_uv_vert(sel[i]);
    }
    center /= (float)sel.size();
    Vector2 sc = uv_to_screen(center);

    float len = 40.0f;
    Color xcol(0.9f, 0.3f, 0.2f);
    Color ycol(0.3f, 0.8f, 0.3f);

    if (current_tool == TOOL_TRANSLATE || current_tool == TOOL_SCALE) {
        draw_line(sc, sc + Vector2(len, 0), xcol, 2.0f);
        draw_line(sc, sc + Vector2(0, -len), ycol, 2.0f);
        _draw_arrowhead(sc + Vector2(len, 0), Vector2(1, 0), 8.0f, xcol);
        _draw_arrowhead(sc + Vector2(0, -len), Vector2(0, -1), 8.0f, ycol);
    } else if (current_tool == TOOL_ROTATE) {
        float r = 28.0f;
        int segs = 24;
        float step = 6.283185307179586f / segs;
        for (int i = 0; i < segs; i++) {
            float a0 = i * step;
            float a1 = ((i + 1) % segs) * step;
            Vector2 p0 = sc + Vector2(cosf(a0), sinf(a0)) * r;
            Vector2 p1 = sc + Vector2(cosf(a1), sinf(a1)) * r;
            draw_line(p0, p1, Color(0.5f, 0.7f, 1.0f), 2.0f);
        }
        Vector2 tip = sc + Vector2(1, 0) * r;
        draw_line(sc, tip, Color(0.5f, 0.7f, 1.0f), 2.0f);
        _draw_arrowhead(tip, Vector2(1, 0), 8.0f, Color(0.5f, 0.7f, 1.0f));
    }
}

void VFXUVEditorViewport::_draw_arrowhead(const Vector2& p_pos, const Vector2& p_dir, float p_size, const Color& p_color) {
    Vector2 perp(-p_dir.y, p_dir.x);
    Vector2 a = p_pos - p_dir * p_size + perp * (p_size * 0.5f);
    Vector2 b = p_pos - p_dir * p_size - perp * (p_size * 0.5f);
    draw_line(p_pos, a, p_color, 2.0f);
    draw_line(p_pos, b, p_color, 2.0f);
}
