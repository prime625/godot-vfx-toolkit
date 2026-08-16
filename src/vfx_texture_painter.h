#ifndef VFX_TEXTURE_PAINTER_H
#define VFX_TEXTURE_PAINTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/color.hpp>
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
    float brush_radius = 16.0f;    // pixels on the texture
    float brush_hardness = 0.0f;   // 0 = fully soft, 1 = hard cutoff
    float brush_opacity = 1.0f;
    int brush_mode = BRUSH_PAINT;

    bool paint_top_only = false;   // only paint upward-facing triangles
    float top_threshold = 0.3f;    // minimum dot(N, world_up)

    bool is_stroking = false;
    Vector2 last_uv;
    bool has_last_uv = false;

    bool _raycast_mesh_uv(const Vector3& ray_origin, const Vector3& ray_dir,
                          Vector2& out_uv, Vector3& out_normal) const;
    void _paint_splat(const Vector2& uv);
    void _paint_line_uv(const Vector2& from, const Vector2& to);

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

    void stroke_begin();
    void paint_at(const Vector3& ray_origin, const Vector3& ray_dir);
    void stroke_end();

    bool export_jpg(const String& filepath, float quality = 0.92f) const;
};

VARIANT_ENUM_CAST(VFXTexturePainter::BrushMode);

#endif