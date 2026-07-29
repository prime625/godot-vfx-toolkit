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
#include "vfx_math.h"

using namespace godot;

namespace vfx {

// Half-edge data structure for topological editing
struct HEVertex;
struct HEEdge;
struct HEFace;

struct HEVertex {
    uint32_t id = 0;
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Color color;
    HEEdge* halfedge = nullptr;  // one outgoing half-edge

    // Skinning data (up to 4 bones per vertex, standard for games)
    int bone_indices[4] = {-1, -1, -1, -1};
    float bone_weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct HEEdge {
    uint32_t id = 0;
    HEVertex* vertex = nullptr;      // vertex this edge points TO
    HEEdge* twin = nullptr;          // opposite half-edge
    HEEdge* next = nullptr;          // next edge around face
    HEFace* face = nullptr;          // face this edge belongs to
    bool is_boundary = false;
};

struct HEFace {
    uint32_t id = 0;
    HEEdge* halfedge = nullptr;      // one edge of this face
    Vector3 normal;
    uint32_t vertex_count = 0;
};

} // namespace vfx

// Godot-exposed mesh class
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
    bool dirty = true;  // needs Godot mesh rebuild

    // Cached Godot arrays for rendering
    PackedVector3Array godot_positions;
    PackedVector3Array godot_normals;
    PackedVector2Array godot_uvs;
    PackedColorArray godot_colors;
    PackedInt32Array godot_indices;

    void _update_bounds();
    void _rebuild_godot_arrays();
    void _clear_mesh();

protected:
    static void _bind_methods();

public:
    VFXMesh();
    ~VFXMesh();

    // === CREATION ===
    void clear();
    int add_vertex(const Vector3& pos, const Vector2& uv = Vector2(), const Color& col = Color(1,1,1,1));
    void add_triangle(int v0, int v1, int v2);
    void add_quad(int v0, int v1, int v2, int v3);

    // === TOPOLOGY QUERIES ===
    int get_vertex_count() const;
    int get_face_count() const;
    int get_edge_count() const;

    Vector3 get_vertex_position(int idx) const;
    void set_vertex_position(int idx, const Vector3& pos);
    Vector3 get_vertex_normal(int idx) const;
    void set_vertex_normal(int idx, const Vector3& n);
    Vector2 get_vertex_uv(int idx) const;
    void set_vertex_uv(int idx, const Vector2& uv);

    // === MODELLING OPERATIONS ===
    void extrude_face(int face_idx, float distance);
    void inset_face(int face_idx, float amount);
    void delete_face(int face_idx);
    void merge_vertices(int v0, int v1);
    void subdivide_face(int face_idx);

    // === SKINNING ===
    void set_vertex_bones(int vidx, int b0, int b1, int b2, int b3);
    void set_vertex_weights(int vidx, float w0, float w1, float w2, float w3);
    void normalize_weights(int vidx);

    // === DATA EXPORT ===
    PackedVector3Array get_positions() const;
    PackedVector3Array get_normals() const;
    PackedVector2Array get_uvs() const;
    PackedColorArray get_colors() const;
    PackedInt32Array get_indices() const;

    // Returns raw bone data as PackedFloat32Array: [idx0, w0, idx1, w1, ...] per vertex
    PackedFloat32Array get_skinning_data() const;

    // === CONVERSION ===
    // Build a Godot ArrayMesh from our data (for viewport preview)
    Ref<Mesh> to_godot_mesh() const;

    // Import from Godot ArrayMesh
    void from_godot_mesh(const Ref<Mesh>& mesh);

    // === UTILS ===
    void recalculate_normals();
    void recalculate_bounds();
    vfx::AABB get_bounds() const;

    // === SERIALIZATION ===
    PackedByteArray serialize() const;
    void deserialize(const PackedByteArray& data);
};

#endif
