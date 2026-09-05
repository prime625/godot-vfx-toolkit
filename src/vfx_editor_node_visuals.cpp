void VFXEditorNode::_build_selection_mesh() {
    if (!selection_visual) _ensure_selection_visual();
    if (mesh.is_null() || !show_wireframe) {
        selection_visual->set_mesh(Ref<ArrayMesh>());
        return;
    }

    Ref<ArrayMesh> am;
    am.instantiate();

    PackedVector3Array verts;
    PackedColorArray cols;
    PackedInt32Array idx;

    // === SCREEN-SPACE CONSISTENT SCALE (Prisma3D style) ===
    // Wireframe keeps the same on-screen thickness regardless of zoom,
    // but clamps so it never gets too thick when far or too thin when close.
    float cam_dist = 5.0f;
    if (camera) {
        Vector3 mesh_pos = _get_active_mesh_transform().get_origin();
        cam_dist = camera->get_global_transform().get_origin().distance_to(mesh_pos);
        if (cam_dist < 0.001f) cam_dist = 0.001f;
    }

    // At 5 units distance, scale = 1.0 (matches old fixed values)
    float dist_scale = cam_dist / 5.0f;
    if (dist_scale < 0.2f) dist_scale = 0.2f;   // close-up: thinner
    if (dist_scale > 3.0f) dist_scale = 3.0f;   // far away: thicker, clamped

    const float BASE_EDGE_R = 0.0025f;
    const float BASE_VERT_S = 0.014f;

    float base_edge_r = BASE_EDGE_R * dist_scale;
    float base_vert_s = BASE_VERT_S * dist_scale;

    // Build set of edge IDs that border selected faces (for face-mode highlighting)
    std::set<int> selected_face_edge_ids;
    if (edit_mode == MODE_FACE) {
        for (auto* f : mesh->get_faces()) {
            if (f->deleted || !f->halfedge) continue;
            if (selected_faces.find((int)f->id) == selected_faces.end()) continue;
            auto* he = f->halfedge;
            auto* start = he;
            do {
                if (he && he->id >= 0) selected_face_edge_ids.insert((int)he->id);
                he = he->next;
            } while (he && he != start);
        }
    }

    // Wireframe edges — Prisma3D style: black, screen-space consistent thickness
    for (auto* e : mesh->get_edges()) {
        if (e->deleted || !e->vertex || !e->next || !e->next->vertex) continue;
        Vector3 a = e->next->vertex->position;
        Vector3 b = e->vertex->position;
        bool in_set = selected_edges.find((int)e->id) != selected_edges.end();
        bool is_active = (edit_mode == MODE_EDGE && selected_edge == (int)e->id);
        bool is_face_selected = (edit_mode == MODE_FACE && selected_face_edge_ids.find((int)e->id) != selected_face_edge_ids.end());

        Color col;
        float r;
        if (is_active) {
            col = Color(1.0f, 0.8f, 0.2f, 1.0f);   // bright yellow active
            r = base_edge_r * 4.0f;
        } else if (in_set) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);   // orange selected
            r = base_edge_r * 3.2f;
        } else if (is_face_selected) {
            col = Color(1.0f, 0.5f, 0.0f, 1.0f);   // orange for selected face border
            r = base_edge_r * 3.2f;
        } else {
            col = Color(0.0f, 0.0f, 0.0f, 1.0f);   // black
            r = base_edge_r;
        }
        vfx_editor::append_cylinder(verts, cols, idx, a, b, r, 4, col);
    }

    // Vertices — ONLY in vertex mode, Prisma3D style: black dots
    if (edit_mode == MODE_VERTEX) {
        for (auto* v : mesh->get_vertices()) {
            if (v->deleted) continue;
            bool in_set = selected_vertices.find((int)v->id) != selected_vertices.end();
            bool is_active = (selected_vertex == (int)v->id);

            Color col;
            float s;
            if (is_active) {
                col = Color(1.0f, 0.8f, 0.2f, 1.0f);
                s = base_vert_s * 2.5f;
            } else if (in_set) {
                col = Color(1.0f, 0.5f, 0.0f, 1.0f);
                s = base_vert_s * 2.0f;
            } else {
                col = Color(0.0f, 0.0f, 0.0f, 1.0f); // black
                s = base_vert_s;
            }
            vfx_editor::append_box(verts, cols, idx, v->position, s, col);
        }
    }

    if (verts.size() > 0) {
        Array arrays;
        arrays.resize(Mesh::ARRAY_MAX);
        arrays[Mesh::ARRAY_VERTEX] = verts;
        arrays[Mesh::ARRAY_COLOR] = cols;
        arrays[Mesh::ARRAY_INDEX] = idx;
        am->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    }

    Ref<StandardMaterial3D> mat;
    mat.instantiate();
    mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
    mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    selection_visual->set_material_override(mat);
    selection_visual->set_mesh(am);
    selection_visual->set_transform(_get_active_mesh_transform());
}
