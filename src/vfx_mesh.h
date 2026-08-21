#ifndef VFX_MESH_H
#define VFX_MESH_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <vector>
#include <cstdint>
#include "vfx_math.h"

using namespace godot;

namespace vfx {

struct HEVertex {
    uint32_t id = 0;
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Color color = Color(1, 1, 1, 1);
    HEVertex* halfedge = nullptr;
    bool deleted = false;
    int bone_indices[4] = {-1, -1, -1, -1};
    float bone_weights[4] = {0, 0, 0, 0};
};

struct HEEdge {
    uint32_t id = 0;
    HEVertex* vertex = nullptr;
    HEEdge* next = nullptr;
    HEEdge* twin = nullptr;
    struct HEFace* face = nullptr;
    bool deleted = false;
    bool is_boundary = false;
};

struct HEFace {
    uint32_t id = 0;
    HEEdge* halfedge = nullptr;
    bool deleted = false;
    int vertex_count = 0;
    Vector3 normal;
};

struct VFXUVLayer {
    std::vector<std::vector<int>> face_corners;
    std::vector<Vector2> coords;
};

} // namespace vfx

class VFXMesh : public RefCounted {
    GDCLASS(VFXMesh, RefCounted)

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

    // === UV LAYER API ===
    int add_uv_layer();
    void remove_uv_layer(int idx);
    int get_uv_layer_count() const;
    void clear_uv_layers();
    void set_face_uv(int layer, int face_idx, int corner, const Vector2& uv);
    Vector2 get_face_uv(int layer, int face_idx, int corner) const;
    PackedVector2Array get_face_uvs(int layer, int face_idx) const;
    void set_face_uvs(int layer, int face_idx, const PackedVector2Array& uvs);
    void sync_uv_layers();
    PackedVector2Array get_uvs_from_layer(int layer) const;

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
    void cleanup();

    // === TOPOLOGY QUERIES ===
    void get_face_vertices(int face_idx, std::vector<vfx::HEVertex*>& out) const;
    void get_face_edges(int face_idx, std::vector<vfx::HEEdge*>& out) const;
    vfx::HEEdge* find_edge_between(int v0, int v1) const;
    void get_vertex_neighbors(int vidx, std::vector<int>& out_neighbors) const;
    void get_vertex_faces(int vidx, std::vector<int>& out_faces) const;
    Vector3 get_face_center(int face_idx) const;
    Vector3 get_face_normal(int face_idx) const;
    int get_face_vertex_count(int face_idx) const;
    void get_edge_endpoints(int edge_idx, int& out_v0, int& out_v1) const;
    Vector3 get_edge_midpoint(int edge_idx) const;

    // === LOOP SELECTION ===
    void get_ordered_edges_around_vertex(int vertex_id, std::vector<int>& out_edges) const;
    PackedInt32Array select_edge_loop(int edge_id) const;
    PackedInt32Array select_vertex_loop(int vertex_id) const;
    PackedInt32Array select_face_loop(int face_id) const;

    // === RAYCAST ===
    bool raycast(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, float max_distance = 1e20f) const;
    bool raycast_select_face(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, int& out_face_idx, float max_distance = 1e20f) const;
    bool raycast_select_edge(const Vector3& ray_origin, const Vector3& ray_dir, int& out_edge_idx, float max_distance = 1e20f) const;
    bool raycast_select_vertex(const Vector3& ray_origin, const Vector3& ray_dir, int& out_vertex_idx, float max_distance = 1e20f) const;

    // === SKINNING ===
    void set_vertex_bones(int vidx, int b0, int b1, int b2, int b3);
    void set_vertex_weights(int vidx, float w0, float w1, float w2, float w3);
    void normalize_weights(int vidx);
    void get_vertex_skinning(int idx, int out_bones[4], float out_weights[4]) const;
    void set_vertex_skinning(int idx, const int bones[4], const float weights[4]);
    PackedInt32Array get_vertex_bones(int idx) const;
    PackedFloat32Array get_vertex_weights(int idx) const;
    void set_vertex_skinning_arrays(int idx, const PackedInt32Array& bones, const PackedFloat32Array& weights);

    // === DATA EXPORT ===
    PackedVector3Array get_positions() const;
    PackedVector3Array get_normals() const;
    PackedVector2Array get_uvs() const;
    PackedColorArray get_colors() const;
    PackedInt32Array get_indices() const;
    PackedFloat32Array get_skinning_data() const;

    Ref<Mesh> to_godot_mesh() const;
    void from_godot_mesh(const Ref<Mesh>& mesh);

    void recalculate_normals();
    void recalculate_bounds();
    PackedFloat32Array get_bounds() const;

    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);

    // === CURVE TO MESH ===
    static Ref<VFXMesh> create_from_curve(const PackedVector3Array& points,
                                          const PackedVector3Array& handles_in,
                                          const PackedVector3Array& handles_out,
                                          float radius, int segments, int rings,
                                          bool cap_start = true, bool cap_end = true);

    // === TOPOLOGY EDITING ===
    bool get_edge_vertices(int edge_id, int& out_v0, int& out_v1) const;
    int get_edge_faces(int edge_id, int out_faces[2]) const;
    bool is_edge_boundary(int edge_id) const;
    bool collapse_edge(int edge_id, int keep_vertex);
    bool flip_edge(int edge_id);
    int split_edge(int edge_id);
    void remove_face(int face_id);

    const std::vector<vfx::HEVertex*>& get_vertices() const { return vertices; }
    const std::vector<vfx::HEEdge*>& get_edges() const { return edges; }
    const std::vector<vfx::HEFace*>& get_faces() const { return faces; }

protected:
    static void _bind_methods();

private:
    std::vector<vfx::HEVertex*> vertices;
    std::vector<vfx::HEEdge*> edges;
    std::vector<vfx::HEFace*> faces;
    std::vector<vfx::VFXUVLayer> uv_layers;

    uint32_t next_vertex_id = 0;
    uint32_t next_edge_id = 0;
    uint32_t next_face_id = 0;

    mutable std::vector<int> vert_remap;
    mutable std::vector<int> face_remap;
    mutable bool remap_dirty = true;

    vfx::AABB bounds;
    bool dirty = true;

    void _clear_mesh();
    void _rebuild_remap() const;
};

#endif
