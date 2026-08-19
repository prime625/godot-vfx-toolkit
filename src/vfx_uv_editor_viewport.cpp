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
    // Guard: ignore emulated mouse-from-touch while real touches are active
    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid()) {
        if (mb->get_button_index() == MOUSE_BUTTON_LEFT && !_touches.empty())
            return;
    }
    Ref<InputEventMouseMotion> mm = p_event;
    if (mm.is_valid() && !_touches.empty())
        return;

    // MOUSE (desktop)
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

    // TOUCH (mobile)
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
            Vector2 uv1 = screen_to_uv(r.end);
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
    queue_redraw();
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
    if (uv_editor->get_mesh().is_null()) return;

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
// DRAWING
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
    }

    Rect2 screen_rect(Vector2(), get_size());
    PackedInt32Array sel_faces = uv_editor->get_selected_faces();

    // 1. Face fills
    for (int i = 0; i < _cached_faces.size(); i++) {
        const CachedFace& cf = _cached_faces[i];
        if (!screen_rect.intersects(cf.bounds)) continue;

        bool is_sel = false;
        for (int j = 0; j < sel_faces.size(); j++) {
            if (sel_faces[j] == i) {
                is_sel = true;
                break;
            }
        }
        Color fill = is_sel ? Color(0.22f, 0.17f, 0.12f, 0.90f) : Color(0.16f, 0.14f, 0.12f, 0.85f);
        draw_colored_polygon(cf.screen_poly, fill);
    }

    // 2. Wireframe (batched per color)
    PackedVector2Array sel_lines;
    PackedVector2Array unsel_lines;
    for (int i = 0; i < _cached_faces.size(); i++) {
        const CachedFace& cf = _cached_faces[i];
        if (!screen_rect.intersects(cf.bounds)) continue;

        bool is_sel = false;
        for (int j = 0; j < sel_faces.size(); j++) {
            if (sel_faces[j] == i) {
                is_sel = true;
                break;
            }
        }
        const PackedVector2Array& sp = cf.screen_poly;
        int n = sp.size();
        for (int j = 0; j < n; j++) {
            Vector2 a = sp[j];
            Vector2 b = sp[(j + 1) % n];
            if (is_sel) {
                sel_lines.push_back(a);
                sel_lines.push_back(b);
            } else {
                unsel_lines.push_back(a);
                unsel_lines.push_back(b);
            }
        }
    }

    for (int i = 0; i < sel_lines.size(); i += 2) {
        draw_line(sel_lines[i], sel_lines[i + 1], Color(1.0f, 0.45f, 0.08f), 1.2f);
    }
    for (int i = 0; i < unsel_lines.size(); i += 2) {
        draw_line(unsel_lines[i], unsel_lines[i + 1], Color(0.45f, 0.45f, 0.50f), 0.8f);
    }

    // 3. Face-center dots
    if (select_mode == VFXUVEditor::UV_SELECT_FACE) {
        for (int i = 0; i < _cached_faces.size(); i++) {
            const CachedFace& cf = _cached_faces[i];
            if (!screen_rect.intersects(cf.bounds)) continue;

            bool is_sel = false;
            for (int j = 0; j < sel_faces.size(); j++) {
                if (sel_faces[j] == i) {
                    is_sel = true;
                    break;
                }
            }
            Color dot_col = is_sel ? Color(1.0f, 0.45f, 0.08f) : Color(0.35f, 0.35f, 0.40f);
            draw_circle(cf.center, 2.2f, dot_col);
        }
    }

    // 4. Vertices
    if (select_mode == VFXUVEditor::UV_SELECT_VERTEX) {
        int vc = uv_editor->get_uv_vert_count();
        for (int i = 0; i < vc; i++) {
            Vector2 pos = uv_to_screen(uv_editor->get_uv_vert(i));
            if (uv_editor->is_uv_selected(i)) {
                draw_circle(pos, 4.0f, Color(1.0f, 0.45f, 0.08f));
                draw_arc(pos, 5.5f, 0.0f, 6.283185307179586f, 32, Color(1, 1, 1), 1.0f);
            } else {
                draw_circle(pos, 3.0f, Color(0.18f, 0.18f, 0.22f));
                draw_arc(pos, 4.0f, 0.0f, 6.283185307179586f, 24, Color(0.50f, 0.50f, 0.55f), 1.0f);
            }
        }
    } else {
        PackedInt32Array sel = uv_editor->get_selected_verts();
        for (int i = 0; i < sel.size(); i++) {
            int idx = sel[i];
            Vector2 pos = uv_to_screen(uv_editor->get_uv_vert(idx));
            draw_circle(pos, 4.0f, Color(1.0f, 0.45f, 0.08f));
            draw_arc(pos, 5.5f, 0.0f, 6.283185307179586f, 32, Color(1, 1, 1), 1.0f);
        }
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
// GRID
// ============================================================================
void VFXUVEditorViewport::_draw_uv_grid() {
    Vector2 uv0 = screen_to_uv(Vector2());
    Vector2 uv1 = screen_to_uv(get_size());
    Rect2 uv_rect(uv0, uv1 - uv0);
    uv_rect = uv_rect.abs();

    int x0 = (int)std::floor(uv_rect.position.x);
    int y0 = (int)std::floor(uv_rect.position.y);
    int x1 = (int)std::ceil(uv_rect.end.x);
    int y1 = (int)std::ceil(uv_rect.end.y);

    // Major checkerboard tiles
    for (int x = x0; x < x1; x++) {
        for (int y = y0; y < y1; y++) {
            bool even = ((x + y) % 2) == 0;
            Color col = even ? Color(0.10f, 0.10f, 0.10f) : Color(0.115f, 0.115f, 0.115f);
            Vector2 p = uv_to_screen(Vector2(x, y));
            Vector2 q = uv_to_screen(Vector2(x + 1, y + 1));
            draw_rect(Rect2(p, q - p), col);
        }
    }

    // Medium grid (7 divisions)
    if (zoom > 80.0f) {
        Color med(0.16f, 0.16f, 0.16f);
        for (int x = x0; x < x1; x++) {
            for (int i = 1; i < 7; i++) {
                float t = (float)i / 7.0f;
                float vx = uv_to_screen(Vector2(x + t, 0)).x;
                draw_line(Vector2(vx, 0), Vector2(vx, get_size().y), med, 0.6f);
            }
        }
        for (int y = y0; y < y1; y++) {
            for (int i = 1; i < 7; i++) {
                float t = (float)i / 7.0f;
                float vy = uv_to_screen(Vector2(0, y + t)).y;
                draw_line(Vector2(0, vy), Vector2(get_size().x, vy), med, 0.6f);
            }
        }
    }

    // Fine grid (49 divisions)
    if (zoom > 400.0f) {
        Color fine(0.13f, 0.13f, 0.13f);
        float step = 1.0f / 49.0f;

        float fx = std::floor(uv_rect.position.x / step) * step;
        while (fx <= uv_rect.end.x + step) {
            bool major = std::fabs(std::fmod(fx, 1.0)) < 0.001;
            bool medium = std::fabs(std::fmod(fx * 7.0, 1.0)) < 0.001;
            if (!major && !medium) {
                float vx = uv_to_screen(Vector2(fx, 0.0f)).x;
                draw_line(Vector2(vx, 0.0f), Vector2(vx, get_size().y), fine, 0.4f);
            }
            fx += step;
        }

        float fy = std::floor(uv_rect.position.y / step) * step;
        while (fy <= uv_rect.end.y + step) {
            bool major = std::fabs(std::fmod(fy, 1.0)) < 0.001;
            bool medium = std::fabs(std::fmod(fy * 7.0, 1.0)) < 0.001;
            if (!major && !medium) {
                float vy = uv_to_screen(Vector2(0.0f, fy)).y;
                draw_line(Vector2(0.0f, vy), Vector2(get_size().x, vy), fine, 0.4f);
            }
            fy += step;
        }
    }

    // Major borders
    for (int x = x0; x <= x1; x++) {
        float vx = uv_to_screen(Vector2(x, 0.0f)).x;
        draw_line(Vector2(vx, 0.0f), Vector2(vx, get_size().y), Color(0.22f, 0.22f, 0.24f), 1.0f);
    }
    for (int y = y0; y <= y1; y++) {
        float vy = uv_to_screen(Vector2(0.0f, y)).y;
        draw_line(Vector2(0.0f, vy), Vector2(get_size().x, vy), Color(0.22f, 0.22f, 0.24f), 1.0f);
    }

    // 0..1 border
    Vector2 b0 = uv_to_screen(Vector2(0.0f, 0.0f));
    Vector2 b1 = uv_to_screen(Vector2(1.0f, 1.0f));
    draw_rect(Rect2(b0, b1 - b0), Color(0.35f, 0.35f, 0.40f), false, 1.5f);
}

// ============================================================================
// GIZMO
// ============================================================================
void VFXUVEditorViewport::_draw_gizmo() {
    Vector2 pivot = uv_to_screen(_transform_pivot);
    float s = 60.0f;
    Ref<Font> font = get_theme_default_font();

    switch (current_tool) {
        case TOOL_TRANSLATE: {
            draw_line(pivot, pivot + Vector2(s, 0), Color(0.85f, 0.20f, 0.20f), 2.0f);
            _draw_arrowhead(pivot + Vector2(s, 0), Vector2(1, 0), 8, Color(0.85f, 0.20f, 0.20f));
            draw_line(pivot, pivot + Vector2(0, -s), Color(0.20f, 0.85f, 0.20f), 2.0f);
            _draw_arrowhead(pivot + Vector2(0, -s), Vector2(0, -1), 8, Color(0.20f, 0.85f, 0.20f));

            Vector2 c = pivot + Vector2(s * 0.2f, -s * 0.2f);
            draw_rect(Rect2(c, Vector2(s * 0.15f, -s * 0.15f)), Color(0.9f, 0.9f, 0.20f, 0.25f), true);
            draw_rect(Rect2(c, Vector2(s * 0.15f, -s * 0.15f)), Color(0.9f, 0.9f, 0.20f), false, 1.0f);

            if (font.is_valid()) {
                draw_string(font, pivot + Vector2(s + 6, 4), "X",
                    HORIZONTAL_ALIGNMENT_LEFT, -1.0f, 13, Color(0.85f, 0.20f, 0.20f));
                draw_string(font, pivot + Vector2(4, -s - 4), "Y",
                    HORIZONTAL_ALIGNMENT_LEFT, -1.0f, 13, Color(0.20f, 0.85f, 0.20f));
            }
            break;
        }
        case TOOL_ROTATE: {
            draw_arc(pivot, s * 0.5f, 0.0f, 6.283185307179586f, 64, Color(0.20f, 0.70f, 0.90f), 2.0f);
            draw_line(pivot, pivot + Vector2(s * 0.5f, 0), Color(0.20f, 0.70f, 0.90f), 2.0f);
            break;
        }
        case TOOL_SCALE: {
            draw_line(pivot, pivot + Vector2(s, 0), Color(0.85f, 0.20f, 0.20f), 2.0f);
            draw_rect(Rect2(pivot + Vector2(s - 3, -3), Vector2(6, 6)), Color(0.85f, 0.20f, 0.20f));
            draw_line(pivot, pivot + Vector2(0, -s), Color(0.20f, 0.85f, 0.20f), 2.0f);
            draw_rect(Rect2(pivot + Vector2(-3, -s - 3), Vector2(6, 6)), Color(0.20f, 0.85f, 0.20f));
            draw_rect(Rect2(pivot - Vector2(3, 3), Vector2(6, 6)), Color(1, 1, 1));
            break;
        }
        default:
            break;
    }
}

void VFXUVEditorViewport::_draw_arrowhead(const Vector2& p_pos, const Vector2& p_dir, float p_size, const Color& p_color) {
    Vector2 n = p_dir.orthogonal();
    Vector2 p0 = p_pos;
    Vector2 p1 = p_pos - p_dir * p_size + n * p_size * 0.5f;
    Vector2 p2 = p_pos - p_dir * p_size - n * p_size * 0.5f;
    PackedVector2Array pts;
    pts.push_back(p0);
    pts.push_back(p1);
    pts.push_back(p2);
    draw_colored_polygon(pts, p_color);
}
