#ifndef VFX_TEXTURE_PAINTER_H
#define VFX_TEXTURE_PAINTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "vfx_mesh.h"
#include "vfx_math.h"

using namespace godot;

class VFXTexturePainter : public RefCounted {
    GDCLASS(VFXTexturePainter, RefCounted)

public:
    enum BrushMode {
        BRUSH_PAINT = 0,
        BRUSH_ERASE = 1,
        BRUSH_SMUDGE = 2
    };

private:
    Ref<VFXMesh> mesh;
    Ref<Image> texture;

    Color brush_color = Color(1.0f, 0.0f, 0.0f, 1.0f);
    float brush_radius = 16.0f;    // pixels on the texture (legacy splat path)
    float brush_hardness = 0.0f;   // 0 = fully soft, 1 = hard cutoff
    float brush_opacity = 1.0f;
    int brush_mode = BRUSH_PAINT;

    bool paint_top_only = false;   // only paint upward-facing triangles
    float top_threshold = 0.3f;    // minimum dot(N, world_up)

    bool is_stroking = false;
    Vector2 last_uv;
    bool has_last_uv = false;

    // --- screen-space 3D brush ---
    // Stamps are filtered by true 3D distance from the hit point, so a brush
    // circle can never bleed across neighbouring UV islands (packed unwraps
    // like character hands/backs). brush_screen_radius is in viewport pixels.
    Transform3D camera_transform = Transform3D();
    Vector2 viewport_size = Vector2(1152, 648);
    float camera_fov_deg = 70.0f;
    float brush_screen_radius = 24.0f;   // pixels on screen
    Vector3 last_anchor_world;
    bool has_last_anchor = false;

    bool _raycast_mesh_uv(const Vector3& ray_origin, const Vector3& ray_dir,
                          Vector2& out_uv, Vector3& out_normal, Vector3& out_pos) const;
    void _blend_pixel(int x, int y, float falloff);
    void _paint_splat(const Vector2& uv);
    void _paint_line_uv(const Vector2& from, const Vector2& to);
    void _paint_splat_3d(const Vector2& uv, const Vector3& anchor_world, float radius_world);
    void _paint_line_3d(const Vector2& from_uv, const Vector3& from_world,
                         const Vector2& to_uv, const Vector3& to_world, float radius_world);
    bool _uv_at_world_point(const Vector3& world_pos, Vector2& out_uv) const;

protected:
    static void _bind_methods();

public:
    VFXTexturePainter();
    ~VFXTexturePainter();

    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;

    void create_texture(int width, int height, const Color& fill_color);
    void set_texture(const Ref<Image>& p_texture);
    Ref<Image> get_texture() const;
    Ref<ImageTexture> get_image_texture() const;
    void apply_to_image_texture(const Ref<ImageTexture>& p_tex) const;
    void clear_texture(const Color& color);

    void set_brush_color(const Color& c);
    Color get_brush_color() const;
    void set_brush_radius(float r);
    float get_brush_radius() const;
    void set_brush_hardness(float h);
    float get_brush_hardness() const;
    void set_brush_opacity(float o);
    float get_brush_opacity() const;
    void set_brush_mode(int mode);
    int get_brush_mode() const;

    void set_paint_top_only(bool enabled);
    bool get_paint_top_only() const;
    void set_top_threshold(float t);
    float get_top_threshold() const;

    void set_camera_transform(const Transform3D& t);
    Transform3D get_camera_transform() const;
    void set_viewport_size(const Vector2& s);
    Vector2 get_viewport_size() const;
    void set_camera_fov(float fov_deg);
    float get_camera_fov() const;
    void set_brush_screen_radius(float r);
    float get_brush_screen_radius() const;

    // vfx_texture_painter.h
    Transform3D mesh_node_transform;
    void set_mesh_transform(const Transform3D& t) { mesh_node_transform = t; }
    Transform3D get_mesh_transform() const { return mesh_node_transform; }


    void stroke_begin();
    // screen_center in viewport pixels; pass Vector2(-1,-1) to use the legacy
    // texture-space splat path (no 3D island filter).
    void paint_at(const Vector3& ray_origin, const Vector3& ray_dir, const Vector2& screen_center = Vector2(-1, -1));
    void stroke_end();

    bool export_jpg(const String& filepath, float quality = 0.92f) const;
};

VARIANT_ENUM_CAST(VFXTexturePainter::BrushMode);

#endif
