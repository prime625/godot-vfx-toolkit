#ifndef VFX_RETOPOLOGY_H
#define VFX_RETOPOLOGY_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <vector>
#include <unordered_set>
#include "vfx_mesh.h"

using namespace godot;

namespace vfx {

// Retopology quality metric for a potential quad
struct QuadQuality {
    float planarity = 0.0f;      // how flat
    float aspect_ratio = 1.0f;   // how square (1.0 = perfect)
    float edge_flow = 0.0f;      // alignment to curvature
    float valence_score = 0.0f;  // ideal valence (4) preference
    float overall() const {
        return planarity * aspect_ratio * edge_flow * valence_score;
    }
};

// Edge loop detection result
struct EdgeLoop {
    std::vector<int> edge_ids;
    bool is_boundary = false;
    bool is_feature = false;
    float importance = 1.0f;  // higher = preserve during simplification
};

// Feature edge classification
enum EdgeType {
    EDGE_NORMAL = 0,
    EDGE_BOUNDARY = 1,
    EDGE_FEATURE = 2,     // sharp crease
    EDGE_SEAM = 3         // UV seam or material boundary
};

} // namespace vfx

class VFXRetopology : public RefCounted {
    GDCLASS(VFXRetopology, RefCounted)

private:
    Ref<VFXMesh> mesh;

    // Parameters
    float feature_angle_threshold = 45.0f;  // degrees
    float target_edge_length = 0.05f;
    bool preserve_boundary = true;
    bool preserve_features = true;
    bool preserve_uv_seams = true;
    int max_iterations = 5;

    // Internal state
    std::vector<vfx::EdgeType> edge_types;
    std::vector<vfx::EdgeLoop> edge_loops;
    std::vector<float> vertex_curvature;
    std::unordered_set<int> protected_edges;
    std::unordered_set<int> protected_vertices;

    // === ANALYSIS ===
    void _classify_edges();
    void _detect_edge_loops();
    void _compute_curvature();
    void _mark_protected();

    // === OPERATORS ===
    bool _can_collapse_edge(int edge_id) const;
    bool _can_flip_edge(int edge_id) const;
    void _relax_vertices(int iterations);
    void _remesh_pass();
    void _simplify_pass(int target_faces);

    // === QUALITY ===
    vfx::QuadQuality _evaluate_quad(int v0, int v1, int v2, int v3) const;
    float _edge_length(int edge_id) const;
    float _face_area(int face_id) const;
    float _dihedral_angle(int edge_id) const;
    Vector3 _face_normal(int face_id) const;

    // === HELPERS ===
    bool _is_boundary_vertex(int vidx) const;
    bool _is_feature_vertex(int vidx) const;
    int _vertex_valence(int vidx) const;
    void _get_adjacent_faces(int vidx, std::vector<int>& out_faces) const;
    void _get_ring_vertices(int vidx, std::vector<int>& out_ring) const;

protected:
    static void _bind_methods();

public:
    VFXRetopology();
    ~VFXRetopology();

    // === SETUP ===
    void set_mesh(const Ref<VFXMesh>& p_mesh);
    Ref<VFXMesh> get_mesh() const;

    void set_feature_angle(float degrees);
    float get_feature_angle() const;
    void set_target_edge_length(float len);
    float get_target_edge_length() const;
    void set_preserve_boundary(bool enable);
    bool get_preserve_boundary() const;
    void set_preserve_features(bool enable);
    bool get_preserve_features() const;
    void set_max_iterations(int iters);
    int get_max_iterations() const;

    // === MAIN PIPELINE ===
    // Structure-preserving retopology. Reduces to target face count while
    // maintaining edge loops, features, and quad-dominant topology.
    bool retopologize(int target_faces);

    // Quick uniform remesh (preserves shape, regularizes triangles)
    bool remesh_uniform(float edge_length, int iterations);

    // Quad-dominant simplification (like Instant Meshes lite)
    bool simplify_quad_dominant(int target_faces);

    // Decimate with topology preservation (collapses least important edges first)
    bool decimate_preserve_topology(int target_faces);

    // Post-process: optimize vertex positions for smoothness
    void relax_vertices(int iterations);

    // Detect and return edge loop indices (for UI visualization)
    PackedInt32Array get_edge_loops() const;

    // Get feature edges (for UI visualization)
    PackedInt32Array get_feature_edges() const;

    // === PROGRESS CALLBACK (GDScript can poll) ===
    float get_progress() const;
    String get_status() const;

private:
    float progress = 0.0f;
    String status = "Idle";
    void _set_progress(float p, const String& msg);
};

#endif
