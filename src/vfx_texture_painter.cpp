#include "vfx_texture_painter.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>

using namespace godot;

void VFXTexturePainter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &VFXTexturePainter::set_mesh);
    ClassDB::bind_method(D_METHOD("get_mesh"), &VFXTexturePainter::get_mesh);

    ClassDB::bind_method(D_METHOD("create_texture", "width", "height", "fill_color"), &VFXTexturePainter::create_texture);
    ClassDB::bind_method(D_METHOD("set_texture", "image"), &VFXTexturePainter::set_texture);
    ClassDB::bind_method(D_METHOD("get_texture"), &VFXTexturePainter::get_texture);
    ClassDB::bind_method(D_METHOD("get_image_texture"), &VFXTexturePainter::get_image_texture);
    ClassDB::bind_method(D_METHOD("apply_to_image_texture", "tex"), &VFXTexturePainter::apply_to_image_texture);
    ClassDB::bind_method(D_METHOD("clear_texture", "color"), &VFXTexturePainter::clear_texture);

    ClassDB::bind_method(D_METHOD("set_brush_color", "color"), &VFXTexturePainter::set_brush_color);
    ClassDB::bind_method(D_METHOD("get_brush_color"), &VFXTexturePainter::get_brush_color);
    ClassDB::bind_method(D_METHOD("set_brush_radius", "radius"), &VFXTexturePainter::set_brush_radius);
    ClassDB::bind_method(D_METHOD("get_brush_radius"), &VFXTexturePainter::get_brush_radius);
    ClassDB::bind_method(D_METHOD("set_brush_hardness", "hardness"), &VFXTexturePainter::set_brush_hardness);
    ClassDB::bind_method(D_METHOD("get_brush_hardness"), &VFXTexturePainter::get_brush_hardness);
    ClassDB::bind_method(D_METHOD("set_brush_opacity", "opacity"), &VFXTexturePainter::set_brush_opacity);
    ClassDB::bind_method(D_METHOD("get_brush_opacity"), &VFXTexturePainter::get_brush_opacity);
    ClassDB::bind_method(D_METHOD("set_brush_mode", "mode"), &VFXTexturePainter::set_brush_mode);
    ClassDB::bind_method(D_METHOD("get_brush_mode"), &VFXTexturePainter::get_brush_mode);

    ClassDB::bind_method(D_METHOD("set_paint_top_only", "enabled"), &VFXTexturePainter::set_paint_top_only);
    ClassDB::bind_method(D_METHOD("get_paint_top_only"), &VFXTexturePainter::get_paint_top_only);
    ClassDB::bind_method(D_METHOD("set_top_threshold", "threshold"), &VFXTexturePainter::set_top_threshold);
    ClassDB::bind_method(D_METHOD("get_top_threshold"), &VFXTexturePainter::get_top_threshold);

    ClassDB::bind_method(D_METHOD("stroke_begin"), &VFXTexturePainter::stroke_begin);
    ClassDB::bind_method(D_METHOD("paint_at", "ray_origin", "ray_dir"), &VFXTexturePainter::paint_at);
    ClassDB::bind_method(D_METHOD("stroke_end"), &VFXTexturePainter::stroke_end);

    ClassDB::bind_method(D_METHOD("export_jpg", "filepath", "quality"), &VFXTexturePainter::export_jpg, DEFVAL(0.92f));

    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_PAINT", BRUSH_PAINT);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_ERASE", BRUSH_ERASE);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_SMUDGE", BRUSH_SMUDGE);
}

VFXTexturePainter::VFXTexturePainter() {}
VFXTexturePainter::~VFXTexturePainter() {}

void VFXTexturePainter::set_mesh(const Ref<VFXMesh>& p_mesh) { mesh = p_mesh; }
Ref<VFXMesh> VFXTexturePainter::get_mesh() const { return mesh; }

void VFXTexturePainter::create_texture(int width, int height, const Color& fill_color) {
    texture = Image::create(width, height, false, Image::FORMAT_RGBA8);
    if (texture.is_valid()) {
        texture->fill(fill_color);
    }
}

void VFXTexturePainter::set_texture(const Ref<Image>& p_texture) {
    texture = p_texture;
}

Ref<Image> VFXTexturePainter::get_texture() const { return texture; }

Ref<ImageTexture> VFXTexturePainter::get_image_texture() const {
    if (texture.is_null()) return Ref<ImageTexture>();
    return ImageTexture::create_from_image(texture);
}

void VFXTexturePainter::apply_to_image_texture(const Ref<ImageTexture>& p_tex) const {
    if (p_tex.is_valid() && texture.is_valid()) {
        p_tex->set_image(texture);
    }
}

void VFXTexturePainter::clear_texture(const Color& color) {
    if (texture.is_null()) return;
    texture->fill(color);
}

void VFXTexturePainter::set_brush_color(const Color& c) { brush_color = c; }
Color VFXTexturePainter::get_brush_color() const { return brush_color; }

void VFXTexturePainter::set_brush_radius(float r) { brush_radius = r < 1.0f ? 1.0f : r; }
float VFXTexturePainter::get_brush_radius() const { return brush_radius; }

void VFXTexturePainter::set_brush_hardness(float h) { brush_hardness = vfx::clampf(h, 0.0f, 1.0f); }
float VFXTexturePainter::get_brush_hardness() const { return brush_hardness; }

void VFXTexturePainter::set_brush_opacity(float o) { brush_opacity = vfx::clampf(o, 0.0f, 1.0f); }
float VFXTexturePainter::get_brush_opacity() const { return brush_opacity; }

void VFXTexturePainter::set_brush_mode(int mode) { brush_mode = mode; }
int VFXTexturePainter::get_brush_mode() const { return brush_mode; }

void VFXTexturePainter::set_paint_top_only(bool enabled) { paint_top_only = enabled; }
bool VFXTexturePainter::get_paint_top_only() const { return paint_top_only; }

void VFXTexturePainter::set_top_threshold(float t) { top_threshold = t; }
float VFXTexturePainter::get_top_threshold() const { return top_threshold; }

void VFXTexturePainter::stroke_begin() {
    is_stroking = true;
    has_last_uv = false;
}

void VFXTexturePainter::stroke_end() {
    is_stroking = false;
    has_last_uv = false;
}

bool VFXTexturePainter::_raycast_mesh_uv(const Vector3& ray_origin, const Vector3& ray_dir,
                                         Vector2& out_uv, Vector3& out_normal) const {
    if (mesh.is_null()) return false;

    Vector3 dir = ray_dir.normalized();
    float closest = 1e20f;
    bool found = false;

    int face_count = mesh->get_face_count();
    for (int fi = 0; fi < face_count; ++fi) {
        std::vector<vfx::HEVertex*> verts;
        mesh->get_face_vertices(fi, verts);
        if (verts.size() < 3) continue;

        // Top-surface filter: reject faces pointing too far from world-up
        if (paint_top_only) {
            Vector3 e1 = verts[1]->position - verts[0]->position;
            Vector3 e2 = verts[2]->position - verts[0]->position;
            Vector3 fn = e1.cross(e2).normalized();
            if (fn.dot(Vector3(0, 1, 0)) < top_threshold) continue;
        }

        // Triangulate polygon fan and test each triangle
        for (size_t i = 1; i + 1 < verts.size(); ++i) {
            const Vector3& v0 = verts[0]->position;
            const Vector3& v1 = verts[i]->position;
            const Vector3& v2 = verts[i + 1]->position;

            Vector3 edge1 = v1 - v0;
            Vector3 edge2 = v2 - v0;
            Vector3 h = dir.cross(edge2);
            float a = edge1.dot(h);
            if (a > -0.00001f && a < 0.00001f) continue;

            float f = 1.0f / a;
            Vector3 s = ray_origin - v0;
            float u = f * s.dot(h);
            if (u < 0.0f || u > 1.0f) continue;

            Vector3 q = s.cross(edge1);
            float v = f * dir.dot(q);
            if (v < 0.0f || u + v > 1.0f) continue;

            float t = f * edge2.dot(q);
            if (t > 0.00001f && t < closest) {
                closest = t;
                // Barycentric interpolation of UVs
                float w = 1.0f - u - v;
                const Vector2& uv0 = verts[0]->uv;
                const Vector2& uv1 = verts[i]->uv;
                const Vector2& uv2 = verts[i + 1]->uv;
                out_uv = uv0 * w + uv1 * u + uv2 * v;

                Vector3 e1n = v1 - v0;
                Vector3 e2n = v2 - v0;
                out_normal = e1n.cross(e2n).normalized();
                found = true;
            }
        }
    }
    return found;
}

void VFXTexturePainter::_paint_splat(const Vector2& uv) {
    if (texture.is_null()) return;

    int tw = texture->get_width();
    int th = texture->get_height();
    if (tw <= 0 || th <= 0) return;

    float fx = uv.x * (float)tw;
    float fy = uv.y * (float)th;

    int x0 = (int)floorf(fx - brush_radius);
    int x1 = (int)ceilf(fx + brush_radius);
    int y0 = (int)floorf(fy - brush_radius);
    int y1 = (int)ceilf(fy + brush_radius);

    x0 = (int)vfx::clampf((float)x0, 0.0f, (float)(tw - 1));
    x1 = (int)vfx::clampf((float)x1, 0.0f, (float)(tw - 1));
    y0 = (int)vfx::clampf((float)y0, 0.0f, (float)(th - 1));
    y1 = (int)vfx::clampf((float)y1, 0.0f, (float)(th - 1));

    float r2 = brush_radius * brush_radius;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            float dx = (float)x - fx;
            float dy = (float)y - fy;
            float d2 = dx * dx + dy * dy;
            if (d2 > r2) continue;

            float dist = sqrtf(d2);
            float t = dist / brush_radius;

            // Hardness: lerp between soft linear falloff and hard circle
            float soft = 1.0f - t;
            float hard = (t < 1.0f) ? 1.0f : 0.0f;
            float falloff = vfx::lerp(soft, hard, brush_hardness);

            if (falloff <= 0.0f) continue;

            Color base = texture->get_pixel(x, y);
            Color stamp = brush_color;
            float alpha = brush_opacity * falloff;

            Color result;
            switch (brush_mode) {
                case BRUSH_ERASE:
                    result = Color(base.r, base.g, base.b, vfx::clampf(base.a - alpha, 0.0f, 1.0f));
                    break;
                case BRUSH_SMUDGE:
                    // Smudge drags color slightly toward brush tint
                    result = base.lerp(stamp, alpha * 0.3f);
                    break;
                case BRUSH_PAINT:
                default:
                    result = base.lerp(stamp, alpha);
                    break;
            }

            texture->set_pixel(x, y, result);
        }
    }
}

void VFXTexturePainter::_paint_line_uv(const Vector2& from, const Vector2& to) {
    if (texture.is_null()) return;
    int tw = texture->get_width();
    int th = texture->get_height();
    if (tw <= 0 || th <= 0) return;

    Vector2 diff = to - from;
    float pixel_dist = diff.length() * (float)std::max(tw, th);
    int steps = (int)(pixel_dist * 0.5f) + 1;
    if (steps > 256) steps = 256; // safety cap for huge UV jumps

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (float)i / (float)steps : 0.0f;
        Vector2 uv = from.lerp(to, t);
        _paint_splat(uv);
    }
}

void VFXTexturePainter::paint_at(const Vector3& ray_origin, const Vector3& ray_dir) {
    Vector2 hit_uv;
    Vector3 hit_normal;
    if (!_raycast_mesh_uv(ray_origin, ray_dir, hit_uv, hit_normal)) return;

    if (is_stroking) {
        if (has_last_uv) {
            _paint_line_uv(last_uv, hit_uv);
        } else {
            _paint_splat(hit_uv);
        }
        last_uv = hit_uv;
        has_last_uv = true;
    } else {
        _paint_splat(hit_uv);
    }
}

bool VFXTexturePainter::export_jpg(const String& filepath, float quality) const {
    if (texture.is_null()) {
        UtilityFunctions::push_warning("VFXTexturePainter: no texture to export");
        return false;
    }
    Error err = texture->save_jpg(filepath, quality);
    if (err != OK) {
        UtilityFunctions::push_warning("VFXTexturePainter: failed to save JPG to ", filepath);
        return false;
    }
    return true;
}
