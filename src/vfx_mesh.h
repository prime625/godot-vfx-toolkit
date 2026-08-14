#ifndef VFX_MESH_H
#define VFX_MESH_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "vfx_math.h"

using namespace godot;

namespace vfx {

struct HEVertex;
struct HEEdge;
struct HEFace;

struct HEVertex {
    uint32_t id = 0;
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Color color;
    HEEdge* halfedge = nullptr;

    int bone_indices[4] = {-1, -1, -1, -1};
    float bone_weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    bool deleted = false;
};

struct HEEdge {
    uint32_t id = 0;
    HEVertex* vertex = nullptr;
    HEEdge* twin = nullptr;
    HEEdge* next = nullptr;
    HEFace* face = nullptr;
    bool is_boundary = false;
    bool deleted = false;
};

struct HEFace {
    uint32_t id = 0;
    HEEdge* halfedge = nullptr;
    Vector3 normal;
    uint32_t vertex_count = 0;
    bool deleted = false;
};

} // namespace vfx

class VFXMesh : public RefCounted {
    GDCLASS(VFXMesh, RefCounted)

private:
    std::vector<vfx::HEVertex*> vertices;
    std::vector<vfx::HEEdge*> edges;
    std::vector<vfx::HEFace*> faces;

    uint32_t next_vertex_id = 0;
    uint32_t next_edge_id = 0;
    uint32_t next_face_id = 0;

    vfx::AABB bounds;
    mutable bool dirty = true;
    mutable bool remap_dirty = true;

    mutable std::vector<int> vert_remap;
    mutable std::vector<int> face_remap;

    void _update_bounds();
    void _rebuild_remap() const;
    void _clear_mesh();

protected:
    static void _bind_methods();

public:
    VFXMesh();
    ~VFXMesh();

    void clear();
    int add_vertex(const Vector3& pos, const Vector2& uv = Vector2(), const Color& col = Color(1,1,1,1));
    void add_triangle(int v0, int v1, int v2);
    void add_quad(int v0, int v1, int v2, int v3);

    int get_vertex_count() const;
    int get_face_count() const;
    int get_edge_count() const;
    int get_live_vertex_count() const;
    int get_live_face_count() const;

    Vector3 get_vertex_position(int idx) const;
    void set_vertex_position(int idx, const Vector3& pos);
    Vector3 get_vertex_normal(int idx) const;
    void set_vertex_normal(int idx, const Vector3& n);
    Vector2 get_vertex_uv(int idx) const;
    void set_vertex_uv(int idx, const Vector2& uv);

    // === MODELING ===
    void extrude_face(int face_idx, float distance);
    void inset_face(int face_idx, float amount);
    void delete_face(int face_idx);
    void dissolve_face(int face_idx);
    void merge_vertices(int v0, int v1);
    void subdivide_face(int face_idx);
    void loop_cut(int face_idx, int v0, int v1, float t);
    void bevel_edge(int edge_idx, float amount);
    void bevel_vertex(int vidx, float amount);
    void dissolve_edge(int edge_idx);
    void dissolve_vertex(int vidx);
    void bridge_faces(int face_a, int face_b);
    void flip_face_normals(int face_idx);
    void flip_all_normals();
    void recalculate_normals();
    Vector3 get_face_normal(int idx) const;
    void get_edge_vertices(int edge_idx, Vector3& out_a, Vector3& out_b) const;
    void cleanup();

    // === TOPOLOGY ===
    void link_twins();
    vfx::HEEdge* find_edge_between(int v0, int v1) const;
    void get_face_vertices(int face_idx, std::vector<vfx::HEVertex*>& out) const;
    void get_face_edges(int face_idx, std::vector<vfx::HEEdge*>& out) const;
    void get_vertex_neighbors(int vidx, std::vector<int>& out_neighbors) const;
    void get_vertex_faces(int vidx, std::vector<int>& out_faces) const;

    // === RAYCAST (selection) ===
    bool raycast(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, float max_distance = 1e20f) const;
    int raycast_face(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, float max_distance = 1e20f) const;
    int raycast_vertex(const Vector3& ray_origin, const Vector3& ray_dir, float radius, float& out_t) const;
    int raycast_edge(const Vector3& ray_origin, const Vector3& ray_dir, float radius, float& out_t) const;

    // === SKINNING ===
    void set_vertex_bones(int vidx, int b0, int b1, int b2, int b3);
    void set_vertex_weights(int vidx, float w0, float w1, float w2, float w3);
    void normalize_weights(int vidx);
    void get_vertex_skinning(int idx, int out_bones[4], float out_weights[4]) const;
    void set_vertex_skinning(int idx, const int bones[4], const float weights[4]);

    // === DATA EXPORT ===
    PackedVector3Array get_positions() const;
    PackedVector3Array get_normals() const;
    PackedVector2Array get_uvs() const;
    PackedColorArray get_colors() const;
    PackedInt32Array get_indices() const;
    PackedFloat32Array get_skinning_data() const;

    // Wireframe line segments [v0, v1, v2, v3, ...] for every unique edge
    PackedVector3Array get_wireframe_lines() const;

    Ref<Mesh> to_godot_mesh() const;
    void from_godot_mesh(const Ref<Mesh>& mesh);

    void recalculate_bounds();
    PackedFloat32Array get_bounds() const;

    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);

    // C++ helper only — not bound to GDScript
    static Ref<VFXMesh> create_from_curve(const PackedVector3Array& points,
                                          const PackedVector3Array& handles_in,
                                          const PackedVector3Array& handles_out,
                                          float radius, int segments, int rings,
                                          bool cap_start = true, bool cap_end = true);
};

#endif