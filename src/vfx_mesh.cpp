#include "vfx_mesh.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <cmath>

using namespace godot;

void VFXMesh::_bind_methods() {
   ClassDB::bind_method(D_METHOD("clear"), &VFXMesh::clear);
   ClassDB::bind_method(D_METHOD("add_vertex", "position", "uv", "color"), &VFXMesh::add_vertex, DEFVAL(Vector2()), DEFVAL(Color(1,1,1,1)));
   ClassDB::bind_method(D_METHOD("add_triangle", "v0", "v1", "v2"), &VFXMesh::add_triangle);
   ClassDB::bind_method(D_METHOD("add_quad", "v0", "v1", "v2", "v3"), &VFXMesh::add_quad);

   ClassDB::bind_method(D_METHOD("get_vertex_count"), &VFXMesh::get_vertex_count);
   ClassDB::bind_method(D_METHOD("get_face_count"), &VFXMesh::get_face_count);
   ClassDB::bind_method(D_METHOD("get_edge_count"), &VFXMesh::get_edge_count);

   ClassDB::bind_method(D_METHOD("get_vertex_position", "idx"), &VFXMesh::get_vertex_position);
   ClassDB::bind_method(D_METHOD("set_vertex_position", "idx", "pos"), &VFXMesh::set_vertex_position);
   ClassDB::bind_method(D_METHOD("get_vertex_normal", "idx"), &VFXMesh::get_vertex_normal);
   ClassDB::bind_method(D_METHOD("set_vertex_normal", "idx", "n"), &VFXMesh::set_vertex_normal);
   ClassDB::bind_method(D_METHOD("get_vertex_uv", "idx"), &VFXMesh::get_vertex_uv);
   ClassDB::bind_method(D_METHOD("set_vertex_uv", "idx", "uv"), &VFXMesh::set_vertex_uv);

   ClassDB::bind_method(D_METHOD("extrude_face", "face_idx", "distance"), &VFXMesh::extrude_face);
   ClassDB::bind_method(D_METHOD("inset_face", "face_idx", "amount"), &VFXMesh::inset_face);
   ClassDB::bind_method(D_METHOD("delete_face", "face_idx"), &VFXMesh::delete_face);
   ClassDB::bind_method(D_METHOD("merge_vertices", "v0", "v1"), &VFXMesh::merge_vertices);
   ClassDB::bind_method(D_METHOD("subdivide_face", "face_idx"), &VFXMesh::subdivide_face);

   ClassDB::bind_method(D_METHOD("set_vertex_bones", "vidx", "b0", "b1", "b2", "b3"), &VFXMesh::set_vertex_bones);
   ClassDB::bind_method(D_METHOD("set_vertex_weights", "vidx", "w0", "w1", "w2", "w3"), &VFXMesh::set_vertex_weights);
   ClassDB::bind_method(D_METHOD("normalize_weights", "vidx"), &VFXMesh::normalize_weights);

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
   return v->id;
}

void VFXMesh::add_triangle(int v0, int v1, int v2) {
   if (v0 >= (int)vertices.size() || v1 >= (int)vertices.size() || v2 >= (int)vertices.size()) return;

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
}

void VFXMesh::add_quad(int v0, int v1, int v2, int v3) {
   add_triangle(v0, v1, v2);
   add_triangle(v0, v2, v3);
}

int VFXMesh::get_vertex_count() const { return vertices.size(); }
int VFXMesh::get_face_count() const { return faces.size(); }
int VFXMesh::get_edge_count() const { return edges.size(); }

Vector3 VFXMesh::get_vertex_position(int idx) const {
   if (idx >= 0 && idx < (int)vertices.size()) return vertices[idx]->position;
   return Vector3();
}

void VFXMesh::set_vertex_position(int idx, const Vector3& pos) {
   if (idx >= 0 && idx < (int)vertices.size()) {
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

// === MODELLING (Stubs - implement incrementally) ===
void VFXMesh::extrude_face(int face_idx, float distance) {
   UtilityFunctions::print("VFXMesh::extrude_face - TODO");
}

void VFXMesh::inset_face(int face_idx, float amount) {
   UtilityFunctions::print("VFXMesh::inset_face - TODO");
}

void VFXMesh::delete_face(int face_idx) {
   UtilityFunctions::print("VFXMesh::delete_face - TODO");
}

void VFXMesh::merge_vertices(int v0, int v1) {
   UtilityFunctions::print("VFXMesh::merge_vertices - TODO");
}

void VFXMesh::subdivide_face(int face_idx) {
   UtilityFunctions::print("VFXMesh::subdivide_face - TODO");
}

// === SKINNING ===
void VFXMesh::set_vertex_bones(int vidx, int b0, int b1, int b2, int b3) {
   if (vidx >= 0 && vidx < (int)vertices.size()) {
       vertices[vidx]->bone_indices[0] = b0;
       vertices[vidx]->bone_indices[1] = b1;
       vertices[vidx]->bone_indices[2] = b2;
       vertices[vidx]->bone_indices[3] = b3;
   }
}

void VFXMesh::set_vertex_weights(int vidx, float w0, float w1, float w2, float w3) {
   if (vidx >= 0 && vidx < (int)vertices.size()) {
       vertices[vidx]->bone_weights[0] = w0;
       vertices[vidx]->bone_weights[1] = w1;
       vertices[vidx]->bone_weights[2] = w2;
       vertices[vidx]->bone_weights[3] = w3;
   }
}

void VFXMesh::normalize_weights(int vidx) {
   if (vidx < 0 || vidx >= (int)vertices.size()) return;
   float sum = 0.0f;
   for (int i = 0; i < 4; i++) sum += vertices[vidx]->bone_weights[i];
   if (sum > 0.0001f) {
       for (int i = 0; i < 4; i++) vertices[vidx]->bone_weights[i] /= sum;
   }
}

// === NEW: ZERO-COPY SKINNING ACCESSORS ===
void VFXMesh::get_vertex_skinning(int idx, int out_bones[4], float out_weights[4]) const {
   if (idx < 0 || idx >= (int)vertices.size()) {
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
   if (idx < 0 || idx >= (int)vertices.size()) return;
   vfx::HEVertex* v = vertices[idx];
   for (int i = 0; i < 4; i++) {
       v->bone_indices[i] = bones[i];
       v->bone_weights[i] = weights[i];
   }
}

// === DATA EXPORT ===
PackedVector3Array VFXMesh::get_positions() const {
   PackedVector3Array arr;
   arr.resize(vertices.size());
   for (int i = 0; i < (int)vertices.size(); i++) arr[i] = vertices[i]->position;
   return arr;
}

PackedVector3Array VFXMesh::get_normals() const {
   PackedVector3Array arr;
   arr.resize(vertices.size());
   for (int i = 0; i < (int)vertices.size(); i++) arr[i] = vertices[i]->normal;
   return arr;
}

PackedVector2Array VFXMesh::get_uvs() const {
   PackedVector2Array arr;
   arr.resize(vertices.size());
   for (int i = 0; i < (int)vertices.size(); i++) arr[i] = vertices[i]->uv;
   return arr;
}

PackedColorArray VFXMesh::get_colors() const {
   PackedColorArray arr;
   arr.resize(vertices.size());
   for (int i = 0; i < (int)vertices.size(); i++) arr[i] = vertices[i]->color;
   return arr;
}

PackedInt32Array VFXMesh::get_indices() const {
   PackedInt32Array arr;
   for (auto* face : faces) {
       if (!face->halfedge) continue;
       vfx::HEEdge* start = face->halfedge;
       vfx::HEEdge* e = start;
       std::vector<int> face_verts;
       do {
           if (!e || !e->vertex) break;
           face_verts.push_back(e->vertex->id);
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
   PackedFloat32Array arr;
   arr.resize(vertices.size() * 8);
   for (int i = 0; i < (int)vertices.size(); i++) {
       int base = i * 8;
       for (int j = 0; j < 4; j++) {
           arr[base + j] = (float)vertices[i]->bone_indices[j];
           arr[base + 4 + j] = vertices[i]->bone_weights[j];
       }
   }
   return arr;
}

// === CONVERSION ===
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
       PackedInt32Array bones;
       PackedFloat32Array weights;
       bones.resize(vertices.size() * 4);
       weights.resize(vertices.size() * 4);
       for (int i = 0; i < (int)vertices.size(); i++) {
           for (int j = 0; j < 4; j++) {
               bones[i * 4 + j] = vertices[i]->bone_indices[j];
               weights[i * 4 + j] = vertices[i]->bone_weights[j];
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
}

void VFXMesh::recalculate_normals() {
   for (auto* v : vertices) v->normal = Vector3();

   for (auto* face : faces) {
       if (!face->halfedge) continue;
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
           for (auto* v : face_verts) v->normal += fn;
       }
   }

   for (auto* v : vertices) v->normal = v->normal.normalized();
}

void VFXMesh::recalculate_bounds() {
   bounds = vfx::AABB();
   for (auto* v : vertices) bounds.expand(v->position);
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
