#include "vfx_mesh.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <cmath>
#include <unordered_map>
#include <algorithm>
#include <set>

using namespace godot;

void VFXMesh::_bind_methods() {
   ClassDB::bind_method(D_METHOD("clear"), &VFXMesh::clear);
   ClassDB::bind_method(D_METHOD("add_vertex", "position", "uv", "color"), &VFXMesh::add_vertex, DEFVAL(Vector2()), DEFVAL(Color(1,1,1,1)));
   ClassDB::bind_method(D_METHOD("add_triangle", "v0", "v1", "v2"), &VFXMesh::add_triangle);
   ClassDB::bind_method(D_METHOD("add_quad", "v0", "v1", "v2", "v3"), &VFXMesh::add_quad);

   ClassDB::bind_method(D_METHOD("get_vertex_count"), &VFXMesh::get_vertex_count);
   ClassDB::bind_method(D_METHOD("get_face_count"), &VFXMesh::get_face_count);
   ClassDB::bind_method(D_METHOD("get_edge_count"), &VFXMesh::get_edge_count);
   ClassDB::bind_method(D_METHOD("get_live_vertex_count"), &VFXMesh::get_live_vertex_count);
   ClassDB::bind_method(D_METHOD("get_live_face_count"), &VFXMesh::get_live_face_count);

   ClassDB::bind_method(D_METHOD("get_vertex_position", "idx"), &VFXMesh::get_vertex_position);
   ClassDB::bind_method(D_METHOD("set_vertex_position", "idx", "pos"), &VFXMesh::set_vertex_position);
   ClassDB::bind_method(D_METHOD("get_vertex_normal", "idx"), &VFXMesh::get_vertex_normal);
   ClassDB::bind_method(D_METHOD("set_vertex_normal", "idx", "n"), &VFXMesh::set_vertex_normal);
   ClassDB::bind_method(D_METHOD("get_vertex_uv", "idx"), &VFXMesh::get_vertex_uv);
   ClassDB::bind_method(D_METHOD("set_vertex_uv", "idx", "uv"), &VFXMesh::set_vertex_uv);

   ClassDB::bind_method(D_METHOD("extrude_face", "face_idx", "distance"), &VFXMesh::extrude_face);
   ClassDB::bind_method(D_METHOD("inset_face", "face_idx", "amount"), &VFXMesh::inset_face);
   ClassDB::bind_method(D_METHOD("delete_face", "face_idx"), &VFXMesh::delete_face);
   ClassDB::bind_method(D_METHOD("dissolve_face", "face_idx"), &VFXMesh::dissolve_face);
   ClassDB::bind_method(D_METHOD("merge_vertices", "v0", "v1"), &VFXMesh::merge_vertices);
   ClassDB::bind_method(D_METHOD("subdivide_face", "face_idx"), &VFXMesh::subdivide_face);
   ClassDB::bind_method(D_METHOD("loop_cut", "face_idx", "v0", "v1", "t"), &VFXMesh::loop_cut);
   ClassDB::bind_method(D_METHOD("bevel_edge", "edge_idx", "amount"), &VFXMesh::bevel_edge);
   ClassDB::bind_method(D_METHOD("bevel_vertex", "vidx", "amount"), &VFXMesh::bevel_vertex);
   ClassDB::bind_method(D_METHOD("dissolve_edge", "edge_idx"), &VFXMesh::dissolve_edge);
   ClassDB::bind_method(D_METHOD("dissolve_vertex", "vidx"), &VFXMesh::dissolve_vertex);
   ClassDB::bind_method(D_METHOD("bridge_faces", "face_a", "face_b"), &VFXMesh::bridge_faces);
   ClassDB::bind_method(D_METHOD("flip_face_normals", "face_idx"), &VFXMesh::flip_face_normals);
   ClassDB::bind_method(D_METHOD("flip_all_normals"), &VFXMesh::flip_all_normals);
   ClassDB::bind_method(D_METHOD("cleanup"), &VFXMesh::cleanup);

   ClassDB::bind_method(D_METHOD("set_vertex_bones", "vidx", "b0", "b1", "b2", "b3"), &VFXMesh::set_vertex_bones);
   ClassDB::bind_method(D_METHOD("set_vertex_weights", "vidx", "w0", "w1", "w2", "w3"), &VFXMesh::set_vertex_weights);
   ClassDB::bind_method(D_METHOD("normalize_weights", "vidx"), &VFXMesh::normalize_weights);
   ClassDB::bind_method(D_METHOD("get_vertex_bones", "vidx"), &VFXMesh::get_vertex_bones);
   ClassDB::bind_method(D_METHOD("get_vertex_weights", "vidx"), &VFXMesh::get_vertex_weights);
   ClassDB::bind_method(D_METHOD("set_vertex_skinning_arrays", "vidx", "bones", "weights"), &VFXMesh::set_vertex_skinning_arrays);

   ClassDB::bind_method(D_METHOD("get_positions"), &VFXMesh::get_positions);
   ClassDB::bind_method(D_METHOD("get_normals"), &VFXMesh::get_normals);
   ClassDB::bind_method(D_METHOD("get_uvs"), &VFXMesh::get_uvs);
   ClassDB::bind_method(D_METHOD("get_colors"), &VFXMesh::get_colors);
   ClassDB::bind_method(D_METHOD("get_indices"), &VFXMesh::get_indices);
   ClassDB::bind_method(D_METHOD("get_skinning_data"), &VFXMesh::get_skinning_data);

   ClassDB::bind_method(D_METHOD("to_godot_mesh"), &VFXMesh::to_godot_mesh);
   ClassDB::bind_method(D_METHOD("from_godot_mesh", "mesh"), &VFXMesh::from_godot_mesh);

   ClassDB::bind_method(D_METHOD("recalculate_normals"), &VFXMesh::recalculate_normals);
   ClassDB::bind_method(D_METHOD("recalculate_bounds"), &VFXMesh::recalculate_bounds);
   ClassDB::bind_method(D_METHOD("get_bounds"), &VFXMesh::get_bounds);

   ClassDB::bind_method(D_METHOD("serialize"), &VFXMesh::serialize);
   ClassDB::bind_method(D_METHOD("deserialize", "data"), &VFXMesh::deserialize);

   // create_from_curve is a static C++ helper, not bound to GDScript.
   // Use VFXCurve.to_tube_mesh() or VFXEditorNode.create_curve_tube() from GDScript.
}

VFXMesh::VFXMesh() {}

VFXMesh::~VFXMesh() {
   _clear_mesh();
}

void VFXMesh::_clear_mesh() {
   for (auto* v : vertices) delete v;
   for (auto* e : edges) delete e;
   for (auto* f : faces) delete f;
   vertices.clear();
   edges.clear();
   faces.clear();
   next_vertex_id = next_edge_id = next_face_id = 0;
   bounds = vfx::AABB();
   dirty = true;
   remap_dirty = true;
   vert_remap.clear();
   face_remap.clear();
}

void VFXMesh::clear() {
   _clear_mesh();
}

int VFXMesh::add_vertex(const Vector3& pos, const Vector2& uv, const Color& col) {
   vfx::HEVertex* v = new vfx::HEVertex();
   v->id = next_vertex_id++;
   v->position = pos;
   v->uv = uv;
   v->color = col;
   vertices.push_back(v);
   bounds.expand(pos);
   dirty = true;
   remap_dirty = true;
   return v->id;
}

void VFXMesh::add_triangle(int v0, int v1, int v2) {
   if (v0 >= (int)vertices.size() || v1 >= (int)vertices.size() || v2 >= (int)vertices.size()) return;
   if (vertices[v0]->deleted || vertices[v1]->deleted || vertices[v2]->deleted) return;

   vfx::HEFace* face = new vfx::HEFace();
   face->id = next_face_id++;
   face->vertex_count = 3;
   faces.push_back(face);

   vfx::HEEdge* e0 = new vfx::HEEdge(); e0->id = next_edge_id++;
   vfx::HEEdge* e1 = new vfx::HEEdge(); e1->id = next_edge_id++;
   vfx::HEEdge* e2 = new vfx::HEEdge(); e2->id = next_edge_id++;

   e0->vertex = vertices[v1]; e0->face = face; e0->next = e1;
   e1->vertex = vertices[v2]; e1->face = face; e1->next = e2;
   e2->vertex = vertices[v0]; e2->face = face; e2->next = e0;

   if (!vertices[v0]->halfedge) vertices[v0]->halfedge = e0;
   if (!vertices[v1]->halfedge) vertices[v1]->halfedge = e1;
   if (!vertices[v2]->halfedge) vertices[v2]->halfedge = e2;

   face->halfedge = e0;

   edges.push_back(e0);
   edges.push_back(e1);
   edges.push_back(e2);

   dirty = true;
   remap_dirty = true;
}

void VFXMesh::add_quad(int v0, int v1, int v2, int v3) {
   add_triangle(v0, v1, v2);
   add_triangle(v0, v2, v3);
}

int VFXMesh::get_vertex_count() const { return vertices.size(); }
int VFXMesh::get_face_count() const { return faces.size(); }
int VFXMesh::get_edge_count() const { return edges.size(); }

int VFXMesh::get_live_vertex_count() const {
    int c = 0;
    for (auto* v : vertices) if (!v->deleted) c++;
    return c;
}

int VFXMesh::get_live_face_count() const {
    int c = 0;
    for (auto* f : faces) if (!f->deleted) c++;
    return c;
}

Vector3 VFXMesh::get_vertex_position(int idx) const {
   if (idx >= 0 && idx < (int)vertices.size()) return vertices[idx]->position;
   return Vector3();
}

void VFXMesh::set_vertex_position(int idx, const Vector3& pos) {
   if (idx >= 0 && idx < (int)vertices.size() && !vertices[idx]->deleted) {
       vertices[idx]->position = pos;
       dirty = true;
   }
}

Vector3 VFXMesh::get_vertex_normal(int idx) const {
   if (idx >= 0 && idx < (int)vertices.size()) return vertices[idx]->normal;
   return Vector3();
}

void VFXMesh::set_vertex_normal(int idx, const Vector3& n) {
   if (idx >= 0 && idx < (int)vertices.size()) vertices[idx]->normal = n;
}

Vector2 VFXMesh::get_vertex_uv(int idx) const {
   if (idx >= 0 && idx < (int)vertices.size()) return vertices[idx]->uv;
   return Vector2();
}

void VFXMesh::set_vertex_uv(int idx, const Vector2& uv) {
   if (idx >= 0 && idx < (int)vertices.size()) vertices[idx]->uv = uv;
}

// ============================================================================
// REMAP HELPERS
// ============================================================================
void VFXMesh::_rebuild_remap() const {
    vert_remap.clear();
    vert_remap.resize(vertices.size(), -1);
    int live = 0;
    for (int i = 0; i < (int)vertices.size(); i++) {
        if (!vertices[i]->deleted) vert_remap[i] = live++;
    }
    remap_dirty = false;
}

// ============================================================================
// TOPOLOGY QUERIES
// ============================================================================
void VFXMesh::get_face_vertices(int face_idx, std::vector<vfx::HEVertex*>& out) const {
    out.clear();
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    vfx::HEEdge* start = faces[face_idx]->halfedge;
    vfx::HEEdge* e = start;
    do {
        if (!e || !e->vertex) break;
        out.push_back(e->vertex);
        e = e->next;
    } while (e && e != start);
}

void VFXMesh::get_face_edges(int face_idx, std::vector<vfx::HEEdge*>& out) const {
    out.clear();
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    vfx::HEEdge* start = faces[face_idx]->halfedge;
    vfx::HEEdge* e = start;
    do {
        if (!e) break;
        out.push_back(e);
        e = e->next;
    } while (e && e != start);
}

vfx::HEEdge* VFXMesh::find_edge_between(int v0, int v1) const {
    for (auto* e : edges) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        if (e->next->vertex->id == (uint32_t)v0 && e->vertex->id == (uint32_t)v1) return e;
    }
    return nullptr;
}

void VFXMesh::get_vertex_neighbors(int vidx, std::vector<int>& out_neighbors) const {
    out_neighbors.clear();
    if (vidx < 0 || vidx >= (int)vertices.size() || vertices[vidx]->deleted) return;
    vfx::HEVertex* v = vertices[vidx];
    if (!v->halfedge) return;

    vfx::HEEdge* start = v->halfedge;
    vfx::HEEdge* e = start;
    do {
        if (!e || !e->twin) break;
        if (e->twin->vertex && !e->twin->vertex->deleted) {
            out_neighbors.push_back(e->twin->vertex->id);
        }
        e = e->twin->next;
    } while (e && e != start);
}

void VFXMesh::get_vertex_faces(int vidx, std::vector<int>& out_faces) const {
    out_faces.clear();
    if (vidx < 0 || vidx >= (int)vertices.size() || vertices[vidx]->deleted) return;
    std::set<int> visited;
    for (auto* e : edges) {
        if (e->deleted || !e->face || e->face->deleted) continue;
        if (e->vertex->id == (uint32_t)vidx || (e->next && e->next->vertex && e->next->vertex->id == (uint32_t)vidx)) {
            if (visited.insert(e->face->id).second) out_faces.push_back(e->face->id);
        }
    }
}

// ============================================================================
// SELECTION QUERIES
// ============================================================================
Vector3 VFXMesh::get_face_center(int face_idx) const {
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    Vector3 c;
    for (auto* v : verts) c += v->position;
    if (!verts.empty()) c /= (float)verts.size();
    return c;
}

Vector3 VFXMesh::get_face_normal(int face_idx) const {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return Vector3(0, 1, 0);
    return faces[face_idx]->normal;
}

int VFXMesh::get_face_vertex_count(int face_idx) const {
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    return (int)verts.size();
}

void VFXMesh::get_edge_endpoints(int edge_idx, int& out_v0, int& out_v1) const {
    out_v0 = out_v1 = -1;
    if (edge_idx < 0 || edge_idx >= (int)edges.size() || edges[edge_idx]->deleted) return;
    vfx::HEEdge* e = edges[edge_idx];
    if (!e->vertex) return;
    out_v1 = (int)e->vertex->id; // head
    if (e->twin && e->twin->vertex) {
        out_v0 = (int)e->twin->vertex->id; // tail via twin
    } else if (e->face && !e->face->deleted) {
        // boundary edge: walk face to find edge whose next is e
        vfx::HEEdge* start = e->face->halfedge;
        vfx::HEEdge* cur = start;
        do {
            if (cur->next == e && cur->vertex) {
                out_v0 = (int)cur->vertex->id;
                return;
            }
            cur = cur->next;
        } while (cur && cur != start);
    }
}

Vector3 VFXMesh::get_edge_midpoint(int edge_idx) const {
    int v0, v1;
    get_edge_endpoints(edge_idx, v0, v1);
    if (v0 < 0) return Vector3();
    return (vertices[v0]->position + vertices[v1]->position) * 0.5f;
}

// ============================================================================
// MODELING OPERATIONS
// ============================================================================
void VFXMesh::extrude_face(int face_idx, float distance) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    int n = verts.size();
    if (n < 3) return;

    // Compute face normal
    Vector3 normal = faces[face_idx]->normal;
    if (normal.length_squared() < 0.0001f) {
        Vector3 e1 = verts[1]->position - verts[0]->position;
        Vector3 e2 = verts[2]->position - verts[0]->position;
        normal = e1.cross(e2).normalized();
    }

    // Create new vertices
    std::vector<int> new_ids;
    new_ids.reserve(n);
    for (int i = 0; i < n; i++) {
        Vector3 np = verts[i]->position + normal * distance;
        int nid = add_vertex(np, verts[i]->uv, verts[i]->color);
        new_ids.push_back(nid);
        int b[4]; float w[4];
        get_vertex_skinning(verts[i]->id, b, w);
        set_vertex_skinning(nid, b, w);
    }

    // Side faces
    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        // Winding: old_i, old_next, new_next, new_i
        add_quad(verts[i]->id, verts[next]->id, new_ids[next], new_ids[i]);
    }

    // Top face (reverse winding so normal points same way)
    std::vector<int> top;
    top.reserve(n);
    for (int i = n - 1; i >= 0; i--) top.push_back(new_ids[i]);
    for (size_t i = 1; i + 1 < top.size(); i++)
        add_triangle(top[0], top[i], top[i + 1]);

    // Remove original face (Blender-style extrude replaces it)
    delete_face(face_idx);

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::inset_face(int face_idx, float amount) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    int n = verts.size();
    if (n < 3) return;

    Vector3 center;
    for (auto* v : verts) center += v->position;
    center /= n;

    std::vector<int> inset_ids;
    inset_ids.reserve(n);
    for (int i = 0; i < n; i++) {
        Vector3 dir = (verts[i]->position - center).normalized();
        Vector3 np = verts[i]->position - dir * amount;
        int nid = add_vertex(np, verts[i]->uv, verts[i]->color);
        inset_ids.push_back(nid);
        int b[4]; float w[4];
        get_vertex_skinning(verts[i]->id, b, w);
        set_vertex_skinning(nid, b, w);
    }

    // Side quads
    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        add_quad(verts[i]->id, verts[next]->id, inset_ids[next], inset_ids[i]);
    }

    // Inset face
    for (int i = 1; i + 1 < (int)inset_ids.size(); i++)
        add_triangle(inset_ids[0], inset_ids[i], inset_ids[i + 1]);

    delete_face(face_idx);
    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::delete_face(int face_idx) {
    if (face_idx < 0 || face_idx >= (int)faces.size()) return;
    vfx::HEFace* f = faces[face_idx];
    if (!f || f->deleted) return;

    if (f->halfedge) {
        vfx::HEEdge* start = f->halfedge;
        vfx::HEEdge* e = start;
        do {
            if (e) {
                e->face = nullptr;
                e->is_boundary = true;
            }
            e = e->next;
        } while (e && e != start);
    }
    f->deleted = true;
    dirty = true;
    remap_dirty = true;
}

void VFXMesh::dissolve_face(int face_idx) {
    // Merge face into neighbors by collapsing its edges
    // Simplified: just delete for now
    delete_face(face_idx);
}

void VFXMesh::merge_vertices(int v0, int v1) {
    if (v0 == v1) return;
    if (v0 < 0 || v0 >= (int)vertices.size() || v1 < 0 || v1 >= (int)vertices.size()) return;
    if (vertices[v0]->deleted || vertices[v1]->deleted) return;

    vfx::HEVertex* keep = vertices[v0];
    vfx::HEVertex* discard = vertices[v1];

    keep->position = (keep->position + discard->position) * 0.5f;

    // Redirect all edges that point to discard to keep
    for (auto* e : edges) {
        if (e->deleted) continue;
        if (e->vertex == discard) e->vertex = keep;
    }

    // Mark discard deleted
    discard->deleted = true;
    discard->halfedge = nullptr;

    // Delete degenerate faces (faces with repeated vertex or < 3 unique verts)
    for (auto* f : faces) {
        if (f->deleted || !f->halfedge) continue;
        std::vector<vfx::HEVertex*> fv;
        get_face_vertices(f->id, fv);
        std::set<uint32_t> unique;
        for (auto* v : fv) unique.insert(v->id);
        if (unique.size() < 3) f->deleted = true;
    }

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::subdivide_face(int face_idx) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    int n = verts.size();
    if (n < 3) return;

    Vector3 center;
    Vector2 uv_center;
    for (auto* v : verts) { center += v->position; uv_center += v->uv; }
    center /= n; uv_center /= n;

    int cid = add_vertex(center, uv_center, verts[0]->color);
    int b[4] = {-1,-1,-1,-1}; float w[4] = {0,0,0,0};
    for (auto* v : verts) {
        int vb[4]; float vw[4];
        get_vertex_skinning(v->id, vb, vw);
        for (int j = 0; j < 4; j++) if (vw[j] > w[j]) { b[j] = vb[j]; w[j] = vw[j]; }
    }
    set_vertex_skinning(cid, b, w);

    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        add_triangle(cid, verts[i]->id, verts[next]->id);
    }

    delete_face(face_idx);
    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::loop_cut(int face_idx, int va, int vb, float t) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    int n = verts.size();
    if (n < 4) return; // need at least quad for meaningful loop cut on one face

    // Find edge between va and vb
    int a = -1, b = -1;
    for (int i = 0; i < n; i++) {
        if (verts[i]->id == (uint32_t)va) a = i;
        if (verts[i]->id == (uint32_t)vb) b = i;
    }
    if (a < 0 || b < 0) return;

    // Insert point on edge va-vb
    Vector3 mid = vertices[va]->position.lerp(vertices[vb]->position, t);
    Vector2 mid_uv = vertices[va]->uv.lerp(vertices[vb]->uv, t);
    int mid_id = add_vertex(mid, mid_uv, vertices[va]->color);
    int b0[4], b1[4]; float w0[4], w1[4];
    get_vertex_skinning(va, b0, w0); get_vertex_skinning(vb, b1, w1);
    int mb[4] = {b0[0], b0[1], b0[2], b0[3]}; float mw[4] = {w0[0], w0[1], w0[2], w0[3]};
    // simple lerp weights not correct for bones, but acceptable for tool
    set_vertex_skinning(mid_id, mb, mw);

    // Triangulate the two resulting polygons (simplified: fan from mid)
    // This is a naive implementation; full loop-cut needs edge-ring traversal
    std::vector<int> poly_a, poly_b;
    for (int i = a; i != b; i = (i + 1) % n) poly_a.push_back(verts[i]->id);
    poly_a.push_back(vb);
    poly_b.push_back(mid_id);
    for (int i = b; i != a; i = (i + 1) % n) poly_b.push_back(verts[i]->id);
    poly_b.push_back(va);
    poly_b.push_back(mid_id);

    // Actually simpler: just split face into triangle fan from mid_id
    delete_face(face_idx);
    for (int i = 0; i < n; i++) {
        int cur = verts[i]->id;
        int next = verts[(i + 1) % n]->id;
        if ((cur == va && next == vb) || (cur == vb && next == va)) {
            add_triangle(cur, mid_id, next);
        } else {
            add_triangle(cur, next, mid_id);
        }
    }

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}


void VFXMesh::bevel_edge(int edge_idx, float amount) {
    if (edge_idx < 0 || edge_idx >= (int)edges.size() || edges[edge_idx]->deleted) return;
    vfx::HEEdge* e = edges[edge_idx];
    if (!e->vertex || !e->next || !e->next->vertex) return;

    int va, vb;
    get_edge_endpoints(edge_idx, va, vb);
    if (va < 0 || vb < 0) return;

    Vector3 pa = vertices[va]->position;
    Vector3 pb = vertices[vb]->position;
    Vector3 edir = (pb - pa).normalized();

    // Find faces using this edge and their "other" vertices
    std::vector<int> face_ids;
    std::vector<int> other_verts;
    for (auto* f : faces) {
        if (f->deleted || !f->halfedge) continue;
        std::vector<vfx::HEVertex*> fverts;
        get_face_vertices(f->id, fverts);
        bool has_a = false, has_b = false;
        int other = -1;
        for (auto* v : fverts) {
            if ((int)v->id == va) has_a = true;
            else if ((int)v->id == vb) has_b = true;
            else other = (int)v->id;
        }
        if (has_a && has_b) {
            face_ids.push_back(f->id);
            other_verts.push_back(other);
        }
    }
    if (face_ids.empty()) return;

    // Compute offset directions at va and vb (into face interiors)
    Vector3 off_a, off_b;
    for (size_t i = 0; i < face_ids.size(); i++) {
        int ov = other_verts[i];
        if (ov < 0) continue;
        Vector3 va_to_ov = vertices[ov]->position - pa;
        Vector3 vb_to_ov = vertices[ov]->position - pb;
        Vector3 perp_a = va_to_ov - edir * va_to_ov.dot(edir);
        Vector3 perp_b = vb_to_ov - edir * vb_to_ov.dot(edir);
        if (perp_a.length_squared() > 0.0001f) off_a += perp_a.normalized();
        if (perp_b.length_squared() > 0.0001f) off_b += perp_b.normalized();
    }
    if (off_a.length_squared() < 0.0001f || off_b.length_squared() < 0.0001f) return;
    off_a = off_a.normalized() * amount;
    off_b = off_b.normalized() * amount;

    int nva = add_vertex(pa + off_a, vertices[va]->uv, vertices[va]->color);
    int nvb = add_vertex(pb + off_b, vertices[vb]->uv, vertices[vb]->color);

    int b[4]; float w[4];
    get_vertex_skinning(va, b, w); set_vertex_skinning(nva, b, w);
    get_vertex_skinning(vb, b, w); set_vertex_skinning(nvb, b, w);

    bool bevel_face_added = false;
    for (size_t fi = 0; fi < face_ids.size(); fi++) {
        int fidx = face_ids[fi];
        std::vector<vfx::HEVertex*> fverts;
        get_face_vertices(fidx, fverts);
        int n = (int)fverts.size();

        // Build new inner polygon by replacing va->nva and vb->nvb
        std::vector<int> inner_ids;
        for (auto* v : fverts) {
            if ((int)v->id == va) inner_ids.push_back(nva);
            else if ((int)v->id == vb) inner_ids.push_back(nvb);
            else inner_ids.push_back((int)v->id);
        }

        delete_face(fidx);
        for (int i = 1; i + 1 < (int)inner_ids.size(); i++)
            add_triangle(inner_ids[0], inner_ids[i], inner_ids[i + 1]);

        if (!bevel_face_added) {
            add_quad(va, vb, nvb, nva);
            bevel_face_added = true;
        }
    }

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

// ============================================================================
// KNIFE CUT — slice a single face with a line segment (local space)
// ============================================================================
void VFXMesh::knife_cut_face(int face_idx, const Vector3& p0, const Vector3& p1) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> fverts;
    get_face_vertices(face_idx, fverts);
    int n = (int)fverts.size();
    if (n < 3) return;

    // Build orthonormal basis on face plane
    Vector3 fn = get_face_normal(face_idx);
    Vector3 up = fabsf(fn.dot(Vector3(0, 1, 0))) < 0.99f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 right = fn.cross(up).normalized();
    up = right.cross(fn).normalized();

    auto to_2d = [&](const Vector3& p) -> Vector2 {
        Vector3 local = p - fverts[0]->position;
        return Vector2(local.dot(right), local.dot(up));
    };
    auto to_3d = [&](const Vector2& p) -> Vector3 {
        return fverts[0]->position + right * p.x + up * p.y;
    };

    Vector2 k0 = to_2d(p0);
    Vector2 k1 = to_2d(p1);
    Vector2 kdir = k1 - k0;
    float klen_sq = kdir.length_squared();
    if (klen_sq < 0.0001f) return;

    std::vector<Vector2> poly2d;
    for (auto* v : fverts) poly2d.push_back(to_2d(v->position));

    struct CutHit { int edge; Vector2 pt; float kt; };
    std::vector<CutHit> hits;

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        Vector2 a = poly2d[i];
        Vector2 b = poly2d[j];
        Vector2 e = b - a;
        float denom = kdir.x * e.y - kdir.y * e.x;
        if (fabsf(denom) < 0.0001f) continue;
        float t = ((a.x - k0.x) * e.y - (a.y - k0.y) * e.x) / denom;
        float s = ((a.x - k0.x) * kdir.y - (a.y - k0.y) * kdir.x) / denom;
        if (t >= -0.001f && t <= 1.001f && s >= -0.001f && s <= 1.001f) {
            Vector2 pt = k0 + kdir * t;
            float kt = (pt - k0).dot(kdir) / klen_sq;
            hits.push_back({i, pt, kt});
        }
    }
    if (hits.size() != 2) return; // only clean bisections supported

    std::sort(hits.begin(), hits.end(), [](const CutHit& a, const CutHit& b) {
        return a.kt < b.kt;
    });

    // Create new vertices on the two hit edges
    int e0 = hits[0].edge;
    int e1 = hits[1].edge;
    int v0 = (int)fverts[e0]->id;
    int v1 = (int)fverts[(e0 + 1) % n]->id;
    int v2 = (int)fverts[e1]->id;
    int v3 = (int)fverts[(e1 + 1) % n]->id;

    float t0 = vfx::clampf((hits[0].pt - poly2d[e0]).length() / (poly2d[(e0 + 1) % n] - poly2d[e0]).length(), 0.0f, 1.0f);
    float t1 = vfx::clampf((hits[1].pt - poly2d[e1]).length() / (poly2d[(e1 + 1) % n] - poly2d[e1]).length(), 0.0f, 1.0f);

    Vector3 pos0 = to_3d(hits[0].pt);
    Vector3 pos1 = to_3d(hits[1].pt);
    Vector2 uv0 = vertices[v0]->uv.lerp(vertices[v1]->uv, t0);
    Vector2 uv1 = vertices[v2]->uv.lerp(vertices[v3]->uv, t1);

    int nv0 = add_vertex(pos0, uv0, vertices[v0]->color);
    int nv1 = add_vertex(pos1, uv1, vertices[v2]->color);

    int b[4]; float w[4];
    get_vertex_skinning(v0, b, w); set_vertex_skinning(nv0, b, w);
    get_vertex_skinning(v2, b, w); set_vertex_skinning(nv1, b, w);

    // Build two new polygons by walking the original ring
    std::vector<int> ring;
    for (int i = 0; i < n; i++) ring.push_back((int)fverts[i]->id);

    std::vector<int> face_a, face_b;
    int idx = (e0 + 1) % n;
    face_a.push_back(nv0);
    while (true) {
        face_a.push_back(ring[idx]);
        if (idx == e1) break;
        idx = (idx + 1) % n;
    }
    face_a.push_back(nv1);

    idx = (e1 + 1) % n;
    face_b.push_back(nv1);
    while (true) {
        face_b.push_back(ring[idx]);
        if (idx == e0) break;
        idx = (idx + 1) % n;
    }
    face_b.push_back(nv0);

    delete_face(face_idx);
    for (int i = 1; i + 1 < (int)face_a.size(); i++) add_triangle(face_a[0], face_a[i], face_a[i + 1]);
    for (int i = 1; i + 1 < (int)face_b.size(); i++) add_triangle(face_b[0], face_b[i], face_b[i + 1]);

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}
void VFXMesh::bevel_vertex(int vidx, float amount) {
    if (vidx < 0 || vidx >= (int)vertices.size() || vertices[vidx]->deleted) return;
    std::vector<int> nbrs;
    get_vertex_neighbors(vidx, nbrs);
    if (nbrs.size() < 2) return;

    Vector3 center = vertices[vidx]->position;
    std::vector<int> new_verts;
    for (int ni : nbrs) {
        Vector3 dir = (vertices[ni]->position - center).normalized();
        Vector3 np = center + dir * amount;
        int nid = add_vertex(np, vertices[vidx]->uv, vertices[vidx]->color);
        new_verts.push_back(nid);
        int b[4]; float w[4];
        get_vertex_skinning(vidx, b, w); set_vertex_skinning(nid, b, w);
    }

    // Create fan from original vertex to new verts
    for (size_t i = 0; i < new_verts.size(); i++) {
        int next = (i + 1) % new_verts.size();
        add_triangle(vidx, new_verts[i], new_verts[next]);
    }
    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::dissolve_edge(int edge_idx) {
    if (edge_idx < 0 || edge_idx >= (int)edges.size() || edges[edge_idx]->deleted) return;
    vfx::HEEdge* e = edges[edge_idx];
    if (!e->next || !e->next->vertex || !e->vertex) return;
    merge_vertices(e->next->vertex->id, e->vertex->id);
}

void VFXMesh::dissolve_vertex(int vidx) {
    if (vidx < 0 || vidx >= (int)vertices.size() || vertices[vidx]->deleted) return;
    std::vector<int> nbrs;
    get_vertex_neighbors(vidx, nbrs);
    if (nbrs.empty()) {
        vertices[vidx]->deleted = true;
        return;
    }
    // Merge into first neighbor
    merge_vertices(nbrs[0], vidx);
}

void VFXMesh::bridge_faces(int face_a, int face_b) {
    if (face_a == face_b) return;
    if (face_a < 0 || face_a >= (int)faces.size() || faces[face_a]->deleted) return;
    if (face_b < 0 || face_b >= (int)faces.size() || faces[face_b]->deleted) return;

    std::vector<vfx::HEVertex*> a_verts, b_verts;
    get_face_vertices(face_a, a_verts);
    get_face_vertices(face_b, b_verts);
    if (a_verts.size() != b_verts.size()) return; // require same edge count

    int n = a_verts.size();
    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        add_quad(a_verts[i]->id, a_verts[next]->id, b_verts[next]->id, b_verts[i]->id);
    }

    delete_face(face_a);
    delete_face(face_b);
    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::flip_face_normals(int face_idx) {
    if (face_idx < 0 || face_idx >= (int)faces.size() || faces[face_idx]->deleted) return;
    std::vector<vfx::HEVertex*> verts;
    get_face_vertices(face_idx, verts);
    int n = verts.size();
    if (n < 3) return;

    delete_face(face_idx);
    for (int i = n - 1; i >= 2; i--) {
        add_triangle(verts[0]->id, verts[i]->id, verts[i - 1]->id);
    }
    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::flip_all_normals() {
    for (auto* f : faces) {
        if (f->deleted || !f->halfedge) continue;
        std::vector<vfx::HEVertex*> verts;
        get_face_vertices(f->id, verts);
        int n = verts.size();
        if (n < 3) continue;
        f->deleted = true; // mark old
        for (int i = n - 1; i >= 2; i--) {
            add_triangle(verts[0]->id, verts[i]->id, verts[i - 1]->id);
        }
    }
    // Clean up deleted faces
    std::vector<vfx::HEFace*> new_faces;
    for (auto* f : faces) {
        if (f->deleted) delete f;
        else new_faces.push_back(f);
    }
    faces = new_faces;
    for (int i = 0; i < (int)faces.size(); i++) faces[i]->id = i;
    next_face_id = faces.size();

    dirty = true;
    remap_dirty = true;
    recalculate_normals();
    link_twins();
}

void VFXMesh::cleanup() {
    // Compact vertices
    std::vector<vfx::HEVertex*> new_verts;
    std::unordered_map<uint32_t, uint32_t> vmap;
    for (auto* v : vertices) {
        if (!v->deleted) {
            vmap[v->id] = new_verts.size();
            v->id = new_verts.size();
            new_verts.push_back(v);
        } else {
            delete v;
        }
    }
    vertices = new_verts;

    // Compact edges
    std::vector<vfx::HEEdge*> new_edges;
    std::unordered_map<uint32_t, uint32_t> emap;
    for (auto* e : edges) {
        if (!e->deleted && e->vertex && !e->vertex->deleted) {
            emap[e->id] = new_edges.size();
            e->id = new_edges.size();
            new_edges.push_back(e);
        } else {
            delete e;
        }
    }
    edges = new_edges;

    // Compact faces
    std::vector<vfx::HEFace*> new_faces;
    for (auto* f : faces) {
        if (!f->deleted && f->halfedge) {
            new_faces.push_back(f);
        } else {
            delete f;
        }
    }
    faces = new_faces;

    // Fix pointers
    for (auto* v : vertices) {
        if (v->halfedge) {
            auto it = emap.find(v->halfedge->id);
            v->halfedge = (it != emap.end()) ? edges[it->second] : nullptr;
        }
    }
    for (auto* e : edges) {
        e->vertex = vertices[vmap[e->vertex->id]];
        if (e->next) { auto it = emap.find(e->next->id); e->next = (it != emap.end()) ? edges[it->second] : nullptr; }
        if (e->twin) { auto it = emap.find(e->twin->id); e->twin = (it != emap.end()) ? edges[it->second] : nullptr; }
    }
    for (int i = 0; i < (int)faces.size(); i++) {
        faces[i]->id = i;
        if (faces[i]->halfedge) {
            auto it = emap.find(faces[i]->halfedge->id);
            faces[i]->halfedge = (it != emap.end()) ? edges[it->second] : nullptr;
        }
    }

    next_vertex_id = vertices.size();
    next_edge_id = edges.size();
    next_face_id = faces.size();
    remap_dirty = true;
}

// ============================================================================
// TOPOLOGY
// ============================================================================
void VFXMesh::link_twins() {
    struct EdgeKey {
        int a, b;
        bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
    };
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const {
            return ((uint64_t)(k.a) << 32) | (uint64_t)(k.b);
        }
    };
    std::unordered_map<EdgeKey, vfx::HEEdge*, EdgeKeyHash> edge_map;
    edge_map.reserve(edges.size());

    for (auto* e : edges) {
        if (!e || e->deleted || !e->next || !e->vertex || !e->next->vertex) continue;
        int a = e->vertex->id;
        int b = e->next->vertex->id;
        EdgeKey key{b, a};
        auto it = edge_map.find(key);
        if (it != edge_map.end()) {
            e->twin = it->second;
            it->second->twin = e;
            edge_map.erase(it);
        } else {
            EdgeKey my_key{a, b};
            edge_map[my_key] = e;
        }
    }
}

// ============================================================================
// RAYCAST (Möller–Trumbore)
// ============================================================================
bool VFXMesh::raycast(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, float max_distance) const {
    bool hit = false;
    float closest = max_distance;
    Vector3 dir = ray_dir.normalized();

    for (auto* face : faces) {
        if (face->deleted || !face->halfedge) continue;
        vfx::HEEdge* start = face->halfedge;
        vfx::HEEdge* e = start;
        std::vector<Vector3> fverts;
        do {
            if (!e || !e->vertex) break;
            fverts.push_back(e->vertex->position);
            e = e->next;
        } while (e && e != start);

        if (fverts.size() < 3) continue;

        for (size_t i = 1; i + 1 < fverts.size(); i++) {
            const Vector3& v0 = fverts[0];
            const Vector3& v1 = fverts[i];
            const Vector3& v2 = fverts[i+1];

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
                out_hit = ray_origin + dir * t;
                hit = true;
            }
        }
    }
    return hit;
}

// ============================================================================
// RAYCAST SELECTION (static helpers)
// ============================================================================
static bool _ray_vs_segment_mesh(const Vector3& ro, const Vector3& rd,
                                 const Vector3& a, const Vector3& b,
                                 float radius, float& out_t) {
    Vector3 ab = b - a;
    Vector3 ao = ro - a;
    float ab_len_sq = ab.length_squared();
    if (ab_len_sq < 0.0001f) {
        float b_ = rd.dot(ao);
        float c = ao.dot(ao) - radius * radius;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) return false;
        float t = -b_ - sqrtf(disc);
        if (t < 0.0f) t = -b_ + sqrtf(disc);
        if (t < 0.0f) return false;
        out_t = t;
        return true;
    }
    Vector3 u = rd;
    Vector3 v = ab;
    Vector3 w0 = ro - a;
    float uv = u.dot(v);
    float vv = v.length_squared();
    float uw0 = u.dot(w0);
    float vw0 = v.dot(w0);
    float det = uv * uv - vv;
    float t, s;
    if (fabsf(det) < 0.0001f) {
        s = vfx::clampf(vw0 / vv, 0.0f, 1.0f);
        Vector3 closest_seg = a + v * s;
        t = u.dot(closest_seg - ro);
    } else {
        t = (uw0 * vv - uv * vw0) / det;
        s = (uv * uw0 - vw0) / det;
    }
    if (s >= 0.0f && s <= 1.0f && t >= 0.0f) {
        Vector3 closest_seg = a + v * s;
        Vector3 closest_ray = ro + u * t;
        if ((closest_seg - closest_ray).length_squared() < radius * radius) {
            out_t = t;
            return true;
        }
    }
    return false;
}

bool VFXMesh::raycast_select_face(const Vector3& ray_origin, const Vector3& ray_dir, Vector3& out_hit, int& out_face_idx, float max_distance) const {
    bool hit = false;
    float closest = max_distance;
    Vector3 dir = ray_dir.normalized();
    out_face_idx = -1;

    for (auto* face : faces) {
        if (face->deleted || !face->halfedge) continue;
        vfx::HEEdge* start = face->halfedge;
        vfx::HEEdge* e = start;
        std::vector<Vector3> fverts;
        do {
            if (!e || !e->vertex) break;
            fverts.push_back(e->vertex->position);
            e = e->next;
        } while (e && e != start);

        if (fverts.size() < 3) continue;
        for (size_t i = 1; i + 1 < fverts.size(); i++) {
            const Vector3& v0 = fverts[0];
            const Vector3& v1 = fverts[i];
            const Vector3& v2 = fverts[i+1];

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
                out_hit = ray_origin + dir * t;
                out_face_idx = (int)face->id;
                hit = true;
            }
        }
    }
    return hit;
}

bool VFXMesh::raycast_select_edge(const Vector3& ray_origin, const Vector3& ray_dir, int& out_edge_idx, float max_distance) const {
    float closest = max_distance;
    Vector3 rd = ray_dir.normalized();
    out_edge_idx = -1;

    for (auto* e : edges) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        float t;
        if (_ray_vs_segment_mesh(ray_origin, rd, a, b, 0.04f, t)) {
            if (t < closest) {
                closest = t;
                out_edge_idx = (int)e->id;
            }
        }
    }
    return out_edge_idx >= 0;
}

bool VFXMesh::raycast_select_vertex(const Vector3& ray_origin, const Vector3& ray_dir, int& out_vertex_idx, float max_distance) const {
    float closest = max_distance;
    Vector3 rd = ray_dir.normalized();
    out_vertex_idx = -1;

    for (auto* v : vertices) {
        if (v->deleted) continue;
        Vector3 oc = ray_origin - v->position;
        float b_ = rd.dot(oc);
        float c = oc.dot(oc) - 0.025f * 0.025f;
        float disc = b_ * b_ - c;
        if (disc < 0.0f) continue;
        float t = -b_ - sqrtf(disc);
        if (t < 0.0f) t = -b_ + sqrtf(disc);
        if (t < 0.0f) continue;
        if (t < closest) {
            closest = t;
            out_vertex_idx = (int)v->id;
        }
    }
    return out_vertex_idx >= 0;
}

// ============================================================================
// SKINNING
// ============================================================================
void VFXMesh::set_vertex_bones(int vidx, int b0, int b1, int b2, int b3) {
   if (vidx >= 0 && vidx < (int)vertices.size() && !vertices[vidx]->deleted) {
       vertices[vidx]->bone_indices[0] = b0;
       vertices[vidx]->bone_indices[1] = b1;
       vertices[vidx]->bone_indices[2] = b2;
       vertices[vidx]->bone_indices[3] = b3;
   }
}

void VFXMesh::set_vertex_weights(int vidx, float w0, float w1, float w2, float w3) {
   if (vidx >= 0 && vidx < (int)vertices.size() && !vertices[vidx]->deleted) {
       vertices[vidx]->bone_weights[0] = w0;
       vertices[vidx]->bone_weights[1] = w1;
       vertices[vidx]->bone_weights[2] = w2;
       vertices[vidx]->bone_weights[3] = w3;
   }
}

void VFXMesh::normalize_weights(int vidx) {
   if (vidx < 0 || vidx >= (int)vertices.size() || vertices[vidx]->deleted) return;
   float sum = 0.0f;
   for (int i = 0; i < 4; i++) sum += vertices[vidx]->bone_weights[i];
   if (sum > 0.0001f) {
       for (int i = 0; i < 4; i++) vertices[vidx]->bone_weights[i] /= sum;
   }
}

void VFXMesh::get_vertex_skinning(int idx, int out_bones[4], float out_weights[4]) const {
   if (idx < 0 || idx >= (int)vertices.size() || vertices[idx]->deleted) {
       out_bones[0] = out_bones[1] = out_bones[2] = out_bones[3] = -1;
       out_weights[0] = out_weights[1] = out_weights[2] = out_weights[3] = 0.0f;
       return;
   }
   const vfx::HEVertex* v = vertices[idx];
   for (int i = 0; i < 4; i++) {
       out_bones[i] = v->bone_indices[i];
       out_weights[i] = v->bone_weights[i];
   }
}

void VFXMesh::set_vertex_skinning(int idx, const int bones[4], const float weights[4]) {
   if (idx < 0 || idx >= (int)vertices.size() || vertices[idx]->deleted) return;
   vfx::HEVertex* v = vertices[idx];
   for (int i = 0; i < 4; i++) {
       v->bone_indices[i] = bones[i];
       v->bone_weights[i] = weights[i];
   }
}

PackedInt32Array VFXMesh::get_vertex_bones(int idx) const {
    PackedInt32Array arr;
    arr.resize(4);
    if (idx >= 0 && idx < (int)vertices.size() && !vertices[idx]->deleted) {
        for (int i = 0; i < 4; i++) arr[i] = vertices[idx]->bone_indices[i];
    } else {
        arr[0] = arr[1] = arr[2] = arr[3] = -1;
    }
    return arr;
}

PackedFloat32Array VFXMesh::get_vertex_weights(int idx) const {
    PackedFloat32Array arr;
    arr.resize(4);
    if (idx >= 0 && idx < (int)vertices.size() && !vertices[idx]->deleted) {
        for (int i = 0; i < 4; i++) arr[i] = vertices[idx]->bone_weights[i];
    }
    return arr;
}

void VFXMesh::set_vertex_skinning_arrays(int idx, const PackedInt32Array& bones, const PackedFloat32Array& weights) {
    if (idx < 0 || idx >= (int)vertices.size() || vertices[idx]->deleted) return;
    vfx::HEVertex* v = vertices[idx];
    for (int i = 0; i < 4 && i < bones.size(); i++) v->bone_indices[i] = bones[i];
    for (int i = 0; i < 4 && i < weights.size(); i++) v->bone_weights[i] = weights[i];
}

// ============================================================================
// DATA EXPORT (with deleted-vertex remapping)
// ============================================================================
PackedVector3Array VFXMesh::get_positions() const {
   if (remap_dirty) _rebuild_remap();
   PackedVector3Array arr;
   int live = 0;
   for (int i = 0; i < (int)vertices.size(); i++) if (vert_remap[i] >= 0) live++;
   arr.resize(live);
   for (int i = 0; i < (int)vertices.size(); i++) {
       if (vert_remap[i] >= 0) arr[vert_remap[i]] = vertices[i]->position;
   }
   return arr;
}

PackedVector3Array VFXMesh::get_normals() const {
   if (remap_dirty) _rebuild_remap();
   PackedVector3Array arr;
   int live = 0;
   for (int i = 0; i < (int)vertices.size(); i++) if (vert_remap[i] >= 0) live++;
   arr.resize(live);
   for (int i = 0; i < (int)vertices.size(); i++) {
       if (vert_remap[i] >= 0) {
           Vector3 n = vertices[i]->normal;
           if (n.length_squared() < 0.0001f) n = Vector3(0, 1, 0);
           arr[vert_remap[i]] = n.normalized();
       }
   }
   return arr;
}

PackedVector2Array VFXMesh::get_uvs() const {
   if (remap_dirty) _rebuild_remap();
   PackedVector2Array arr;
   int live = 0;
   for (int i = 0; i < (int)vertices.size(); i++) if (vert_remap[i] >= 0) live++;
   arr.resize(live);
   for (int i = 0; i < (int)vertices.size(); i++) {
       if (vert_remap[i] >= 0) arr[vert_remap[i]] = vertices[i]->uv;
   }
   return arr;
}

PackedColorArray VFXMesh::get_colors() const {
   if (remap_dirty) _rebuild_remap();
   PackedColorArray arr;
   int live = 0;
   for (int i = 0; i < (int)vertices.size(); i++) if (vert_remap[i] >= 0) live++;
   arr.resize(live);
   for (int i = 0; i < (int)vertices.size(); i++) {
       if (vert_remap[i] >= 0) arr[vert_remap[i]] = vertices[i]->color;
   }
   return arr;
}

PackedInt32Array VFXMesh::get_indices() const {
   if (remap_dirty) _rebuild_remap();
   PackedInt32Array arr;
   for (auto* face : faces) {
       if (face->deleted || !face->halfedge) continue;
       vfx::HEEdge* start = face->halfedge;
       vfx::HEEdge* e = start;
       std::vector<int> face_verts;
       do {
           if (!e || !e->vertex) break;
           int remapped = vert_remap[e->vertex->id];
           if (remapped < 0) { face_verts.clear(); break; }
           face_verts.push_back(remapped);
           e = e->next;
       } while (e && e != start);

       for (size_t i = 1; i + 1 < face_verts.size(); i++) {
           arr.push_back(face_verts[0]);
           arr.push_back(face_verts[i]);
           arr.push_back(face_verts[i + 1]);
       }
   }
   return arr;
}

PackedFloat32Array VFXMesh::get_skinning_data() const {
   if (remap_dirty) _rebuild_remap();
   int live = 0;
   for (int i = 0; i < (int)vertices.size(); i++) if (vert_remap[i] >= 0) live++;
   PackedFloat32Array arr;
   arr.resize(live * 8);
   for (int i = 0; i < (int)vertices.size(); i++) {
       if (vert_remap[i] < 0) continue;
       int base = vert_remap[i] * 8;
       for (int j = 0; j < 4; j++) {
           arr[base + j] = (float)vertices[i]->bone_indices[j];
           arr[base + 4 + j] = vertices[i]->bone_weights[j];
       }
   }
   return arr;
}

// ============================================================================
// CONVERSION
// ============================================================================
Ref<Mesh> VFXMesh::to_godot_mesh() const {
   Ref<ArrayMesh> am;
   am.instantiate();

   Array arrays;
   arrays.resize(Mesh::ARRAY_MAX);
   arrays[Mesh::ARRAY_VERTEX] = get_positions();
   arrays[Mesh::ARRAY_NORMAL] = get_normals();
   arrays[Mesh::ARRAY_TEX_UV] = get_uvs();
   arrays[Mesh::ARRAY_COLOR] = get_colors();
   arrays[Mesh::ARRAY_INDEX] = get_indices();

   PackedFloat32Array skin = get_skinning_data();
   if (skin.size() > 0) {
       int live = skin.size() / 8;
       PackedInt32Array bones;
       PackedFloat32Array weights;
       bones.resize(live * 4);
       weights.resize(live * 4);
       for (int i = 0; i < live; i++) {
           for (int j = 0; j < 4; j++) {
               bones[i * 4 + j] = (int)skin[i * 8 + j];
               weights[i * 4 + j] = skin[i * 8 + 4 + j];
           }
       }
       arrays[Mesh::ARRAY_BONES] = bones;
       arrays[Mesh::ARRAY_WEIGHTS] = weights;
   }

   am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
   return am;
}

void VFXMesh::from_godot_mesh(const Ref<Mesh>& mesh) {
   if (mesh.is_null()) return;
   clear();

   for (int si = 0; si < mesh->get_surface_count(); si++) {
       Array arrays = mesh->surface_get_arrays(si);
       PackedVector3Array positions = arrays[Mesh::ARRAY_VERTEX];
       PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
       PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
       PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
       PackedInt32Array bones = arrays[Mesh::ARRAY_BONES];
       PackedFloat32Array weights = arrays[Mesh::ARRAY_WEIGHTS];

       std::vector<int> vert_remap;
       vert_remap.resize(positions.size());
       for (int i = 0; i < positions.size(); i++) {
           Vector2 uv = (i < uvs.size()) ? uvs[i] : Vector2();
           vert_remap[i] = add_vertex(positions[i], uv);
           if (i < normals.size()) set_vertex_normal(vert_remap[i], normals[i]);
       }

       if (indices.size() > 0) {
           for (int i = 0; i < indices.size(); i += 3) {
               add_triangle(vert_remap[indices[i]], vert_remap[indices[i+1]], vert_remap[indices[i+2]]);
           }
       } else {
           for (int i = 0; i < positions.size(); i += 3) {
               add_triangle(vert_remap[i], vert_remap[i+1], vert_remap[i+2]);
           }
       }

       if (bones.size() > 0 && weights.size() > 0) {
           int bw = bones.size() / positions.size();
           int ww = weights.size() / positions.size();
           for (int i = 0; i < positions.size(); i++) {
               int b[4] = {-1,-1,-1,-1};
               float w[4] = {0,0,0,0};
               for (int j = 0; j < 4 && j < bw; j++) b[j] = bones[i * bw + j];
               for (int j = 0; j < 4 && j < ww; j++) w[j] = weights[i * ww + j];
               set_vertex_bones(vert_remap[i], b[0], b[1], b[2], b[3]);
               set_vertex_weights(vert_remap[i], w[0], w[1], w[2], w[3]);
           }
       }
   }
   recalculate_normals();
   link_twins();
}

void VFXMesh::recalculate_normals() {
   for (auto* v : vertices) if (!v->deleted) v->normal = Vector3();

   for (auto* face : faces) {
       if (face->deleted || !face->halfedge) continue;
       vfx::HEEdge* start = face->halfedge;
       vfx::HEEdge* e = start;
       std::vector<vfx::HEVertex*> face_verts;
       do {
           if (!e || !e->vertex) break;
           face_verts.push_back(e->vertex);
           e = e->next;
       } while (e && e != start);

       if (face_verts.size() >= 3) {
           Vector3 e1 = face_verts[1]->position - face_verts[0]->position;
           Vector3 e2 = face_verts[2]->position - face_verts[0]->position;
           Vector3 fn = e1.cross(e2).normalized();
           face->normal = fn;
           for (auto* v : face_verts) v->normal += fn;
       }
   }

   for (auto* v : vertices) if (!v->deleted) v->normal = v->normal.normalized();
}

void VFXMesh::recalculate_bounds() {
   bounds = vfx::AABB();
   for (auto* v : vertices) if (!v->deleted) bounds.expand(v->position);
}

PackedFloat32Array VFXMesh::get_bounds() const {
   PackedFloat32Array arr;
   arr.resize(6);
   arr[0] = bounds.min.x; arr[1] = bounds.min.y; arr[2] = bounds.min.z;
   arr[3] = bounds.max.x; arr[4] = bounds.max.y; arr[5] = bounds.max.z;
   return arr;
}

PackedByteArray VFXMesh::serialize() const {
   PackedByteArray data;
   data.append(0x56); data.append(0x46); data.append(0x58); data.append(0x4D);
   data.append(1);

   data.append((vertices.size() >> 0) & 0xFF);
   data.append((vertices.size() >> 8) & 0xFF);
   data.append((vertices.size() >> 16) & 0xFF);
   data.append((vertices.size() >> 24) & 0xFF);

   data.append((faces.size() >> 0) & 0xFF);
   data.append((faces.size() >> 8) & 0xFF);
   data.append((faces.size() >> 16) & 0xFF);
   data.append((faces.size() >> 24) & 0xFF);

   return data;
}

void VFXMesh::deserialize(const PackedByteArray& data) {
   clear();
}

// ============================================================================
// CURVE TO MESH (Blender-style bevel)
// ============================================================================
static inline Vector3 cubic_bezier(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    float omt = 1.0f - t;
    float omt2 = omt * omt;
    float omt3 = omt2 * omt;
    float t2 = t * t;
    float t3 = t2 * t;
    return p0 * omt3 + p1 * (3.0f * omt2 * t) + p2 * (3.0f * omt * t2) + p3 * t3;
}

static inline Vector3 cubic_bezier_tangent(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
    float omt = 1.0f - t;
    return (p1 - p0) * (3.0f * omt * omt) + (p2 - p1) * (6.0f * omt * t) + (p3 - p2) * (3.0f * t * t);
}

Ref<VFXMesh> VFXMesh::create_from_curve(const PackedVector3Array& points,
                                          const PackedVector3Array& handles_in,
                                          const PackedVector3Array& handles_out,
                                          float radius, int segments, int rings,
                                          bool cap_start, bool cap_end) {
    Ref<VFXMesh> m;
    m.instantiate();
    if (points.size() < 2 || segments < 2 || rings < 3) return m;

    int curve_count = points.size();
    int sample_count = (curve_count - 1) * segments + 1;

    std::vector<Vector3> samples;
    std::vector<Vector3> tangents;
    samples.reserve(sample_count);
    tangents.reserve(sample_count);

    for (int i = 0; i < curve_count - 1; i++) {
        Vector3 p0 = points[i];
        Vector3 p1 = handles_out[i];
        Vector3 p2 = handles_in[i + 1];
        Vector3 p3 = points[i + 1];
        for (int s = 0; s < segments; s++) {
            float t = (float)s / segments;
            samples.push_back(cubic_bezier(p0, p1, p2, p3, t));
            tangents.push_back(cubic_bezier_tangent(p0, p1, p2, p3, t).normalized());
        }
    }
    samples.push_back(points[points.size() - 1]);
    tangents.push_back((points[points.size() - 1] - points[points.size() - 2]).normalized());

    // Parallel transport frame
    std::vector<Basis> frames;
    frames.reserve(samples.size());
    Vector3 ref = (fabs(tangents[0].dot(Vector3(0, 1, 0))) < 0.99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
    Vector3 x = ref.cross(tangents[0]).normalized();
    Vector3 y = tangents[0].cross(x);
    frames.push_back(Basis(x, y, tangents[0]));

    for (size_t i = 1; i < samples.size(); i++) {
        Vector3 t0 = tangents[i - 1];
        Vector3 t1 = tangents[i];
        Vector3 axis = t0.cross(t1);
        if (axis.length_squared() < 0.0001f) {
            frames.push_back(frames.back());
            continue;
        }
        float angle = atan2(axis.length(), t0.dot(t1));
        Basis rot(axis.normalized(), angle);
        Basis new_basis = rot * frames.back();
        frames.push_back(new_basis);
    }

    // Generate vertices
    std::vector<std::vector<int>> ring_verts;
    ring_verts.resize(samples.size());

    for (size_t i = 0; i < samples.size(); i++) {
        ring_verts[i].resize(rings);
        for (int r = 0; r < rings; r++) {
            float ang = (float)r / rings * 3.14159265f * 2.0f;
            Vector3 local_pos = frames[i].get_column(0) * cosf(ang) * radius + frames[i].get_column(1) * sinf(ang) * radius;
            Vector3 world_pos = samples[i] + local_pos;
            float u = (float)i / (samples.size() - 1);
            float v = (float)r / rings;
            ring_verts[i][r] = m->add_vertex(world_pos, Vector2(u, v));
        }
    }

    // Side faces
    for (size_t i = 0; i + 1 < samples.size(); i++) {
        for (int r = 0; r < rings; r++) {
            int r_next = (r + 1) % rings;
            int a = ring_verts[i][r];
            int b = ring_verts[i][r_next];
            int c = ring_verts[i + 1][r_next];
            int d = ring_verts[i + 1][r];
            m->add_quad(a, b, c, d);
        }
    }

    // Caps
    if (cap_start) {
        int center = m->add_vertex(samples[0], Vector2(0.5f, 0.5f));
        for (int r = 0; r < rings; r++) {
            int r_next = (r + 1) % rings;
            m->add_triangle(center, ring_verts[0][r], ring_verts[0][r_next]);
        }
    }
    if (cap_end) {
        int center = m->add_vertex(samples.back(), Vector2(0.5f, 0.5f));
        for (int r = 0; r < rings; r++) {
            int r_next = (r + 1) % rings;
            m->add_triangle(center, ring_verts.back()[r_next], ring_verts.back()[r]);
        }
    }

    m->recalculate_normals();
    m->link_twins();
    return m;
}