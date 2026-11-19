#include "vfx_texture_painter.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <algorithm>
#include <vector>

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

    ClassDB::bind_method(D_METHOD("set_camera_transform", "transform"), &VFXTexturePainter::set_camera_transform);
    ClassDB::bind_method(D_METHOD("get_camera_transform"), &VFXTexturePainter::get_camera_transform);
    ClassDB::bind_method(D_METHOD("set_viewport_size", "size"), &VFXTexturePainter::set_viewport_size);
    ClassDB::bind_method(D_METHOD("get_viewport_size"), &VFXTexturePainter::get_viewport_size);
    ClassDB::bind_method(D_METHOD("set_camera_fov", "fov_deg"), &VFXTexturePainter::set_camera_fov);
    ClassDB::bind_method(D_METHOD("get_camera_fov"), &VFXTexturePainter::get_camera_fov);
    ClassDB::bind_method(D_METHOD("set_brush_screen_radius", "radius"), &VFXTexturePainter::set_brush_screen_radius);
    ClassDB::bind_method(D_METHOD("get_brush_screen_radius"), &VFXTexturePainter::get_brush_screen_radius);

    ClassDB::bind_method(D_METHOD("set_mesh_transform", "transform"), &VFXTexturePainter::set_mesh_transform);
    ClassDB::bind_method(D_METHOD("get_mesh_transform"), &VFXTexturePainter::get_mesh_transform);

    ClassDB::bind_method(D_METHOD("stroke_begin"), &VFXTexturePainter::stroke_begin);
    ClassDB::bind_method(D_METHOD("paint_at", "ray_origin", "ray_dir", "screen_center"), &VFXTexturePainter::paint_at, DEFVAL(Vector2(-1, -1)));
    ClassDB::bind_method(D_METHOD("stroke_end"), &VFXTexturePainter::stroke_end);

    ClassDB::bind_method(D_METHOD("export_jpg", "filepath", "quality"), &VFXTexturePainter::export_jpg, DEFVAL(0.92f));

    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_PAINT", BRUSH_PAINT);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_ERASE", BRUSH_ERASE);
    ClassDB::bind_integer_constant(get_class_static(), "", "BRUSH_SMUDGE", BRUSH_SMUDGE);
}

VFXTexturePainter::VFXTexturePainter() {}
VFXTexturePainter::~VFXTexturePainter() {}

void VFXTexturePainter::set_mesh(const Ref<VFXMesh>& p_mesh) {
    if (mesh == p_mesh) return;
    mesh = p_mesh;
    _rebuild_triangle_cache();
}

void VFXTexturePainter::_rebuild_triangle_cache() {
    cached_triangles.clear();
    if (mesh.is_null()) return;
    std::vector<vfx::HEVertex*> verts;
    const int face_count = mesh->get_face_count();
    for (int fi = 0; fi < face_count; ++fi) {
        verts.clear();
        mesh->get_face_vertices(fi, verts);
        if (verts.size() < 3) continue;
        for (size_t i = 1; i + 1 < verts.size(); ++i) {
            CachedTriangle t;
            t.p0 = verts[0]->position; t.p1 = verts[i]->position; t.p2 = verts[i + 1]->position;
            t.uv0 = verts[0]->uv; t.uv1 = verts[i]->uv; t.uv2 = verts[i + 1]->uv;
            t.normal = (t.p1 - t.p0).cross(t.p2 - t.p0).normalized();
            t.umin = std::min(t.uv0.x, std::min(t.uv1.x, t.uv2.x));
            t.umax = std::max(t.uv0.x, std::max(t.uv1.x, t.uv2.x));
            t.vmin = std::min(t.uv0.y, std::min(t.uv1.y, t.uv2.y));
            t.vmax = std::max(t.uv0.y, std::max(t.uv1.y, t.uv2.y));
            cached_triangles.push_back(t);
        }
    }
}
Ref<VFXMesh> VFXTexturePainter::get_mesh() const { return mesh; }

void VFXTexturePainter::create_texture(int width, int height, const Color& fill_color) {
    texture = Image::create(width, height, false, Image::FORMAT_RGBA8);
    if (texture.is_valid()) {
        texture->fill(fill_color);
    }
    background_color = fill_color;
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
    background_color = color;
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

void VFXTexturePainter::set_camera_transform(const Transform3D& t) { camera_transform = t; }
Transform3D VFXTexturePainter::get_camera_transform() const { return camera_transform; }

void VFXTexturePainter::set_viewport_size(const Vector2& s) { viewport_size = s; }
Vector2 VFXTexturePainter::get_viewport_size() const { return viewport_size; }

void VFXTexturePainter::set_camera_fov(float fov_deg) { camera_fov_deg = fov_deg; }
float VFXTexturePainter::get_camera_fov() const { return camera_fov_deg; }

void VFXTexturePainter::set_brush_screen_radius(float r) { brush_screen_radius = r < 1.0f ? 1.0f : r; }
float VFXTexturePainter::get_brush_screen_radius() const { return brush_screen_radius; }

void VFXTexturePainter::stroke_begin() {
    is_stroking = true;
    has_last_uv = false;
    has_last_anchor = false;
}

void VFXTexturePainter::stroke_end() {
    is_stroking = false;
    has_last_uv = false;
    has_last_anchor = false;
}

bool VFXTexturePainter::_raycast_mesh_uv(const Vector3& ray_origin, const Vector3& ray_dir,
                                         Vector2& out_uv, Vector3& out_normal, Vector3& out_pos) const {
    if (mesh.is_null() || cached_triangles.empty()) return false;
    Vector3 dir = ray_dir.normalized();
    float closest = 1e20f;
    bool found = false;
    for (const CachedTriangle& tri : cached_triangles) {
        if (paint_top_only && tri.normal.dot(Vector3(0, 1, 0)) < top_threshold) continue;
        Vector3 edge1 = tri.p1 - tri.p0, edge2 = tri.p2 - tri.p0;
        Vector3 h = dir.cross(edge2);
        float a = edge1.dot(h);
        if (a > -0.00001f && a < 0.00001f) continue;
        float f = 1.0f / a;
        Vector3 q0 = ray_origin - tri.p0;
        float u = f * q0.dot(h);
        if (u < 0.0f || u > 1.0f) continue;
        Vector3 q = q0.cross(edge1);
        float v = f * dir.dot(q);
        if (v < 0.0f || u + v > 1.0f) continue;
        float t = f * edge2.dot(q);
        if (t > 0.00001f && t < closest) {
            closest = t;
            float w = 1.0f - u - v;
            out_uv = tri.uv0 * w + tri.uv1 * u + tri.uv2 * v;
            out_pos = ray_origin + dir * t;
            out_normal = tri.normal;
            found = true;
        }
    }
    return found;
}

// Shared blend for one texture pixel
void VFXTexturePainter::_blend_pixel(int x, int y, float falloff) {
    Color base = texture->get_pixel(x, y);
    Color stamp = brush_color;
    float alpha = brush_opacity * falloff;

    Color result;
    switch (brush_mode) {
        case BRUSH_ERASE:
            // Blend back toward the canvas background (e.g. white). The old
            // code only lowered alpha, but the paint material is opaque, so
            // alpha is ignored by the renderer and erase looked broken.
            result = base.lerp(background_color, alpha);
            break;
        case BRUSH_SMUDGE:
            result = base.lerp(stamp, alpha * 0.3f);
            break;
        case BRUSH_PAINT:
        default:
            result = base.lerp(stamp, alpha);
            break;
    }
    texture->set_pixel(x, y, result);
}

void VFXTexturePainter::_paint_splat(const Vector2& uv) {
    if (texture.is_null()) return;

    int tw = texture->get_width();
    int th = texture->get_height();
    if (tw <= 0 || th <= 0) return;

    // Some meshes (e.g. GLTF exports) carry out-of-range UVs (negative V, V > 1).
    // Wrap into [0,1) before mapping to pixels, otherwise set_pixel/get_pixel
    // silently fail out of bounds and nothing is painted.
    float u = uv.x - floorf(uv.x);
    float v = uv.y - floorf(uv.y);

    float fx = u * (float)tw;
    float fy = v * (float)th;

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
            _blend_pixel(x, y, falloff);
        }
    }
}

// ------------------------------------------------------------------
// 3D-filtered stamping.
//
// The legacy splat stamps a circle of brush_radius TEXTURE pixels around
// the hit UV. On tightly packed unwraps (251 islands on a 1024 canvas for a
// character mesh) that circle spills across neighbouring islands, so
// painting a hand also paints the mirrored hand / back / face that happen
// to live next to it in UV space.
//
// Here the stamp region is still located via the hit UV, but every candidate
// texel is inverse-mapped through its own triangle to a world position and
// only painted if its true 3D distance to the brush anchor (the hit point)
// is inside the brush's WORLD-space radius. Texels on other islands fail the
// distance test and are skipped, no matter how close they are in UV space.
// ------------------------------------------------------------------
void VFXTexturePainter::_paint_splat_3d(const Vector2& uv, const Vector3& anchor_world, float radius_world) {
    if (texture.is_null() || mesh.is_null()) return;

    int tw = texture->get_width();
    int th = texture->get_height();
    if (tw <= 0 || th <= 0) return;

    float u = uv.x - floorf(uv.x);
    float v = uv.y - floorf(uv.y);

    // Conservative texture-space bbox: a world-space circle maps to at most
    // this many texels in any direction.
    float half_px = ceilf(radius_world * (float)std::max(tw, th)) + 1.0f;
    float fx = u * (float)tw;
    float fy = v * (float)th;

    int x0 = (int)floorf(fx - half_px);
    int x1 = (int)ceilf(fx + half_px);
    int y0 = (int)floorf(fy - half_px);
    int y1 = (int)ceilf(fy + half_px);
    int bw = x1 - x0 + 1;
    int bh = y1 - y0 + 1;
    if (bw <= 0 || bh <= 0) return;
    size_t cell_count = (size_t)bw * (size_t)bh;

    // Per-texel: best (closest-to-camera) candidate only â€” this is what
    // stops back-facing parts of the SAME island from painting through.
    std::vector<float> best_z(cell_count, -1e30f);
    std::vector<float> best_t(cell_count, 1.0f);

    const float r2 = radius_world * radius_world;
    const Transform3D cam_inv = camera_transform.affine_inverse();

    for (const CachedTriangle& tri : cached_triangles) {
        const float umin = tri.umin, umax = tri.umax, vmin = tri.vmin, vmax = tri.vmax;

        // UVs may live in arbitrary integer tiles (for example this GLB has
        // V approximately [-2,-1]). Compute the integer shifts that can
        // actually overlap this brush region instead of assuming only -1..1.
        const float brush_u_min = (float)x0 / (float)tw;
        const float brush_u_max = (float)x1 / (float)tw;
        const float brush_v_min = (float)y0 / (float)th;
        const float brush_v_max = (float)y1 / (float)th;
        const int su_min = (int)ceilf(brush_u_min - umax);
        const int su_max = (int)floorf(brush_u_max - umin);
        const int sv_min = (int)ceilf(brush_v_min - vmax);
        const int sv_max = (int)floorf(brush_v_max - vmin);
        for (int su = su_min; su <= su_max; ++su) {
            for (int sv = sv_min; sv <= sv_max; ++sv) {
                int tx0 = std::max(x0, (int)floorf((umin + (float)su) * (float)tw));
                int tx1 = std::min(x1, (int)ceilf ((umax + (float)su) * (float)tw));
                int ty0 = std::max(y0, (int)floorf((vmin + (float)sv) * (float)th));
                int ty1 = std::min(y1, (int)ceilf ((vmax + (float)sv) * (float)th));
                if (tx0 > tx1 || ty0 > ty1) continue;

                const Vector2 shift((float)su, (float)sv);
                {
                    const Vector2 tuv0 = tri.uv0 + shift;
                    const Vector2 tuv1 = tri.uv1 + shift;
                    const Vector2 tuv2 = tri.uv2 + shift;
                    const Vector2 e1 = tuv1 - tuv0;
                    const Vector2 e2 = tuv2 - tuv0;
                    const float D = e1.x * e2.y - e1.y * e2.x;
                    if (fabsf(D) < 1e-10f) continue;

                    const Vector3& p0 = tri.p0;
                    const Vector3& p1 = tri.p1;
                    const Vector3& p2 = tri.p2;

                    for (int y = ty0; y <= ty1; ++y) {
                        for (int x = tx0; x <= tx1; ++x) {
                            // Texel UV in [0,1), handling wrap-around indices
                            float pxu = (float)(((x % tw) + tw) % tw) + 0.5f;
                            float pyu = (float)(((y % th) + th) % th) + 0.5f;
                            Vector2 ppu(pxu / (float)tw, pyu / (float)th);

                            Vector2 d0 = ppu - tuv0;
                            float ba = (d0.x * e2.y - d0.y * e2.x) / D;
                            float bb = (e1.x * d0.y - e1.y * d0.x) / D;
                            float bw0 = 1.0f - ba - bb;
                            if (ba < -1e-4f || bb < -1e-4f || bw0 < -1e-4f) continue;

                            Vector3 local = p0 * bw0 + p1 * ba + p2 * bb;
                            Vector3 world = mesh_node_transform.xform(local);
                            Vector3 d = world - anchor_world;
                            if (d.length_squared() > r2) continue;

                            // Closer to camera = larger z in Godot camera space
                            float cz = cam_inv.xform(world).z;
                            size_t idx = (size_t)(y - y0) * (size_t)bw + (size_t)(x - x0);
                            if (cz > best_z[idx]) {
                                best_z[idx] = cz;
                                best_t[idx] = sqrtf(d.length_squared()) / radius_world;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            size_t idx = (size_t)(y - y0) * (size_t)bw + (size_t)(x - x0);
            if (best_z[idx] < -1e29f) continue;

            float soft = 1.0f - best_t[idx];
            float hard = (best_t[idx] < 1.0f) ? 1.0f : 0.0f;
            float falloff = vfx::lerp(soft, hard, brush_hardness);
            if (falloff <= 0.0f) continue;

            int xi = ((x % tw) + tw) % tw;
            int yi = ((y % th) + th) % th;
            _blend_pixel(xi, yi, falloff);
        }
    }
}

// Find the UV of the surface point closest to a world position (used to walk
// a stroke segment across the mesh without ever leaving the surface).
bool VFXTexturePainter::_uv_at_world_point(const Vector3& world_pos, Vector2& out_uv) const {
    if (mesh.is_null() || cached_triangles.empty()) return false;
    Vector3 lp = mesh_node_transform.affine_inverse().xform(world_pos);
    float best_dist = 1e30f;
    bool found = false;
    for (const CachedTriangle& tri : cached_triangles) {
        Vector3 n = tri.normal;
        if (n.length_squared() < 1e-12f) continue;
        float dn = n.dot(lp - tri.p0);
        Vector3 q = lp - n * dn;
        Vector3 e1 = tri.p1 - tri.p0, e2 = tri.p2 - tri.p0, d = q - tri.p0;
        float d00 = e1.dot(e1), d01 = e1.dot(e2), d11 = e2.dot(e2);
        float d20 = d.dot(e1), d21 = d.dot(e2);
        float den = d00 * d11 - d01 * d01;
        if (fabsf(den) < 1e-12f) continue;
        float a = (d11 * d20 - d01 * d21) / den;
        float b = (d00 * d21 - d01 * d20) / den;
        float w0 = 1.0f - a - b;
        if (a < -1e-3f || b < -1e-3f || w0 < -1e-3f) continue;
        float ad = fabsf(dn);
        if (ad < best_dist) {
            best_dist = ad;
            out_uv = tri.uv0 * w0 + tri.uv1 * a + tri.uv2 * b;
            found = true;
        }
    }
    return found;
}

// Stroke continuation in 3D: walk the world-space segment between the last
// hit and the current hit, stamping only at real surface points. No UV-space
// line is ever drawn, so nothing between islands can be crossed.
void VFXTexturePainter::_paint_line_3d(const Vector2& from_uv, const Vector3& from_world,
                                          const Vector2& to_uv, const Vector3& to_world,
                                          float radius_world) {
    // Input is coalesced to at most one latest pointer position per frame by
    // editor.gd. The old implementation subdivided every frame segment into
    // as many as 64 steps; each step searched every mesh face for a UV and
    // then scanned the mesh again to stamp. On dense meshes this multiplied
    // the cost of a single drag update dramatically. Consecutive frame stamps
    // already provide stroke continuity at normal frame rates, so stamp only
    // the newest hit here. This also avoids painting across unrelated UV
    // islands because the 3D-filtered stamp remains unchanged.
    (void)from_uv;
    (void)from_world;
    _paint_splat_3d(to_uv, to_world, radius_world);
}

void VFXTexturePainter::_paint_line_uv(const Vector2& from, const Vector2& to) {
    if (texture.is_null()) return;
    int tw = texture->get_width();
    int th = texture->get_height();
    if (tw <= 0 || th <= 0) return;

    // shortest path on the UV torus â€” prevents streaks across the wrap seam
    Vector2 diff = to - from;
    diff.x -= Math::round(diff.x);
    diff.y -= Math::round(diff.y);

    float pixel_dist = diff.length() * (float)std::max(tw, th);
    int steps = (int)(pixel_dist * 0.5f) + 1;
    if (steps > 256) steps = 256;

    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? (float)i / (float)steps : 0.0f;
        _paint_splat(from + diff * t);   // _paint_splat still wraps each splat
    }
}

void VFXTexturePainter::paint_at(const Vector3& ray_origin, const Vector3& ray_dir, const Vector2& screen_center) {
    Vector2 hit_uv;
    Vector3 hit_normal;
    Vector3 hit_pos_local;

    // Convert the world-space ray into the mesh node's local space before raycasting
    Transform3D inv = mesh_node_transform.affine_inverse();
    Vector3 local_origin = inv.xform(ray_origin);
    Vector3 local_dir    = inv.basis.xform(ray_dir);   // direction: no translation, no normalize needed

    if (!_raycast_mesh_uv(local_origin, local_dir, hit_uv, hit_normal, hit_pos_local)) return;

    const bool use_3d = (screen_center.x >= 0.0f);

    if (use_3d) {
        Vector3 hit_world = mesh_node_transform.xform(hit_pos_local);

        // Convert the screen-pixel brush radius into a world radius at the
        // hit depth: half view height at depth d is d*tan(fov/2).
        float depth = (hit_world - camera_transform.origin).length();
        static const float DEG2RAD = 0.017453292519943295f;
        float half_h_world = depth * tanf(camera_fov_deg * 0.5f * DEG2RAD);
        float half_h_px = std::max(viewport_size.y * 0.5f, 1.0f);
        float radius_world = half_h_world * (brush_screen_radius / half_h_px);
        if (radius_world < 1e-5f) radius_world = 1e-5f;

        if (is_stroking && has_last_uv && has_last_anchor) {
            _paint_line_3d(last_uv, last_anchor_world, hit_uv, hit_world, radius_world);
        } else {
            _paint_splat_3d(hit_uv, hit_world, radius_world);
        }
        last_uv = hit_uv;
        last_anchor_world = hit_world;
        has_last_uv = true;
        has_last_anchor = true;
        return;
    }

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