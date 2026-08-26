#ifndef VFX_UV_EDITOR_VIEWPORT_H
#define VFX_UV_EDITOR_VIEWPORT_H

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <unordered_map>
#include <unordered_set>
#include "vfx_uv_editor.h"
#include "vfx_mesh.h"

using namespace godot;

class VFXUVEditorViewport : public Control {
    GDCLASS(VFXUVEditorViewport, Control)

public:
    enum Tool {
        TOOL_SELECT = 0,
        TOOL_TRANSLATE = 1,
        TOOL_ROTATE = 2,
        TOOL_SCALE = 3
    };

private:
    Ref<VFXUVEditor> uv_editor;

    float zoom = 256.0f;
    Vector2 pan_offset = Vector2(0.5f, 0.5f);

    int current_tool = TOOL_SELECT;
    int select_mode = VFXUVEditor::UV_SELECT_VERTEX;

    bool _is_navigating = false;
    bool _is_selecting = false;
    Vector2 _select_start;
    Vector2 _select_current;
    bool _is_transforming = false;
    Vector2 _transform_start_uv;
    Vector2 _transform_pivot;
    std::unordered_map<int, Vector2> _transform_starts;

    // Mobile touch
    std::unordered_map<int, Vector2> _touches;
    float _last_pinch_dist = -1.0f;
    Vector2 _last_pinch_center;
    bool _ignore_remaining_drag = false;

    // Cache
    bool _cache_dirty = true;

    struct CachedFace {
        PackedVector2Array uv_poly;
        PackedVector2Array screen_poly;
        Vector2 center;
        Rect2 bounds;
    };
    Vector<CachedFace> _cached_faces;
    void _batch_arc_outline(PackedVector2Array& r_lines, PackedColorArray& r_colors,
                            const Vector2& center, float radius, int segments,
                            const Color& color);

    // Cached draw data — rebuilt only when _cache_dirty
    PackedVector2Array _cached_sel_lines;
    PackedColorArray   _cached_sel_colors;
    PackedVector2Array _cached_unsel_lines;
    PackedColorArray   _cached_unsel_colors;
    PackedVector2Array _cached_face_dots;
    PackedColorArray   _cached_face_dot_colors;
    PackedVector2Array _cached_vert_dots;
    PackedColorArray   _cached_vert_dot_colors;
    PackedVector2Array _cached_vert_rings;
    PackedColorArray   _cached_vert_ring_colors;
    std::unordered_set<int> _cached_sel_face_set;

    void _request_cache_rebuild();
    void _rebuild_cache();
    void _rebuild_draw_data();

    void _on_left_down(const Vector2& p_pos, bool p_shift);
    void _on_left_up(const Vector2& p_pos);
    void _select_at(const Vector2& p_uv, bool p_add);
    void _begin_transform(const Vector2& p_uv);
    void _on_transform_drag(const Vector2& p_pos);
    void _zoom_at(const Vector2& p_screen_pos, float p_factor);

    void _draw_uv_grid();
    void _draw_gizmo();
    void _draw_arrowhead(const Vector2& p_pos, const Vector2& p_dir, float p_size, const Color& p_color);

    void _draw_gizmo();
    void _draw_arrowhead(const Vector2& p_pos, const Vector2& p_dir, float p_size, const Color& p_color);
    void _batch_arc_outline(PackedVector2Array& r_lines, PackedColorArray& r_colors,
                            const Vector2& center, float radius, int segments,
                            const Color& color);

    void _cancel_single_touch_actions();

    Vector2 screen_to_uv(const Vector2& p_screen) const;
    Vector2 uv_to_screen(const Vector2& p_uv) const;

protected:
    static void _bind_methods();
    void _notification(int p_what);

public:
    void _gui_input(const Ref<InputEvent>& p_event) override;
    void _draw() override;
    VFXUVEditorViewport();

    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;

    void set_current_tool(int p_tool);
    int get_current_tool() const;

    void set_select_mode(int p_mode);
    int get_select_mode() const;
};

VARIANT_ENUM_CAST(VFXUVEditorViewport::Tool);

#endif
