#include "vfx_fbx_importer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/vector4.hpp>
#include <godot_cpp/variant/vector4i.hpp>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>

// zlib for FBX compressed arrays
#include <zlib.h>

using namespace godot;
using namespace vfx_fbx;

// ============================================================================
// FBX PROPERTY ACCESSORS
// ============================================================================

String FBXProperty::as_string() const {
    if (type == FBXPropType::STRING && !data.empty()) {
        return String::utf8(reinterpret_cast<const char*>(data.data()), data.size());
    }
    if (type == FBXPropType::RAW) {
        return "<raw:" + String::num_int64(data.size()) + ">";
    }
    return "";
}

int64_t FBXProperty::as_int() const {
    switch (type) {
        case FBXPropType::INT16: return int_val;
        case FBXPropType::INT32: return int_val;
        case FBXPropType::INT64: return int_val;
        case FBXPropType::BOOL: return bool_val ? 1 : 0;
        case FBXPropType::FLOAT: return (int64_t)double_val;
        case FBXPropType::DOUBLE: return (int64_t)double_val;
        default: return 0;
    }
}

double FBXProperty::as_double() const {
    switch (type) {
        case FBXPropType::DOUBLE: return double_val;
        case FBXPropType::FLOAT: return double_val;
        case FBXPropType::INT16:
        case FBXPropType::INT32:
        case FBXPropType::INT64: return (double)int_val;
        default: return 0.0;
    }
}

float FBXProperty::as_float() const {
    return (float)as_double();
}

bool FBXProperty::as_bool() const {
    if (type == FBXPropType::BOOL) return bool_val;
    return as_int() != 0;
}

PackedFloat32Array FBXProperty::as_float_array() const {
    PackedFloat32Array result;
    if (!is_array()) return result;
    
    if (type == FBXPropType::ARRAY_FLOAT) {
        result.resize(array_count);
        const float* src = reinterpret_cast<const float*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = src[i];
        }
    } else if (type == FBXPropType::ARRAY_DOUBLE) {
        result.resize(array_count);
        const double* src = reinterpret_cast<const double*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = (float)src[i];
        }
    } else if (type == FBXPropType::ARRAY_INT32) {
        result.resize(array_count);
        const int32_t* src = reinterpret_cast<const int32_t*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = (float)src[i];
        }
    } else if (type == FBXPropType::ARRAY_INT64) {
        result.resize(array_count);
        const int64_t* src = reinterpret_cast<const int64_t*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = (float)src[i];
        }
    }
    return result;
}

PackedInt32Array FBXProperty::as_int_array() const {
    PackedInt32Array result;
    if (!is_array()) return result;
    
    if (type == FBXPropType::ARRAY_INT32) {
        result.resize(array_count);
        const int32_t* src = reinterpret_cast<const int32_t*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = src[i];
        }
    } else if (type == FBXPropType::ARRAY_INT64) {
        result.resize(array_count);
        const int64_t* src = reinterpret_cast<const int64_t*>(data.data());
        for (uint32_t i = 0; i < array_count; i++) {
            result[i] = (int32_t)src[i];
        }
    }
    return result;
}

PackedVector3Array FBXProperty::as_vec3_array() const {
    PackedVector3Array result;
    PackedFloat32Array floats = as_float_array();
    if (floats.size() < 3) return result;
    
    int count = floats.size() / 3;
    result.resize(count);
    for (int i = 0; i < count; i++) {
        result[i] = Vector3(floats[i*3], floats[i*3+1], floats[i*3+2]);
    }
    return result;
}

PackedVector2Array FBXProperty::as_vec2_array() const {
    PackedVector2Array result;
    PackedFloat32Array floats = as_float_array();
    if (floats.size() < 2) return result;
    
    int count = floats.size() / 2;
    result.resize(count);
    for (int i = 0; i < count; i++) {
        result[i] = Vector2(floats[i*2], floats[i*2+1]);
    }
    return result;
}

// ============================================================================
// FBX RECORD HELPERS
// ============================================================================

const FBXRecord* FBXRecord::find_child(const String& child_name) const {
    for (const auto& c : children) {
        if (c->name == child_name) return c.get();
    }
    return nullptr;
}

const FBXProperty* FBXRecord::find_property(int index) const {
    if (index >= 0 && index < (int)properties.size()) {
        return &properties[index];
    }
    return nullptr;
}

// ============================================================================
// FBX DOCUMENT PARSER
// ============================================================================

bool FBXDocument::_parse_header(const uint8_t* data, int size, String& out_error) {
    const char* expected = "Kaydara FBX Binary  ";
    if (size < 27) {
        out_error = "File too small for FBX header";
        return false;
    }
    if (memcmp(data, expected, 20) != 0) {
        out_error = "Not a valid FBX binary file (bad header)";
        return false;
    }
    version = *reinterpret_cast<const uint32_t*>(data + 23);
    if (version < 7000 || version > 8000) {
        out_error = "Unsupported FBX version: " + String::num_int64(version);
        return false;
    }
    return true;
}

bool FBXDocument::_decompress_deflate(const uint8_t* src, int src_len, uint8_t* dst, int dst_len, String& out_error) {
    uLongf dest_len = dst_len;
    int ret = uncompress(dst, &dest_len, src, src_len);
    if (ret != Z_OK) {
        out_error = "zlib decompression failed: " + String::num_int64(ret);
        return false;
    }
    return true;
}

bool FBXDocument::_parse_property(const uint8_t* data, int size, int& offset, FBXProperty& prop, 
                                   uint32_t record_version, String& out_error) {
    if (offset >= size) {
        out_error = "Unexpected end of file reading property type";
        return false;
    }
    
    char type_code = static_cast<char>(data[offset]);
    offset++;
    
    auto read_le32 = [&](uint32_t& v) {
        v = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
        offset += 4;
    };
    
    switch (type_code) {
        case 'Y': { // int16
            if (offset + 2 > size) { out_error = "Truncated int16"; return false; }
            int16_t v = data[offset] | (data[offset+1] << 8);
            offset += 2;
            prop.type = FBXPropType::INT16; prop.int_val = v;
            break;
        }
        case 'C': { // bool
            if (offset + 1 > size) { out_error = "Truncated bool"; return false; }
            prop.type = FBXPropType::BOOL; prop.bool_val = (data[offset] != 0); offset++;
            break;
        }
        case 'I': { // int32
            if (offset + 4 > size) { out_error = "Truncated int32"; return false; }
            int32_t v = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
            offset += 4;
            prop.type = FBXPropType::INT32; prop.int_val = v;
            break;
        }
        case 'F': { // float
            if (offset + 4 > size) { out_error = "Truncated float"; return false; }
            float v; memcpy(&v, data + offset, 4);
            offset += 4;
            prop.type = FBXPropType::FLOAT; prop.double_val = v;
            break;
        }
        case 'D': { // double
            if (offset + 8 > size) { out_error = "Truncated double"; return false; }
            double v; memcpy(&v, data + offset, 8);
            offset += 8;
            prop.type = FBXPropType::DOUBLE; prop.double_val = v;
            break;
        }
        case 'L': { // int64
            if (offset + 8 > size) { out_error = "Truncated int64"; return false; }
            int64_t v; memcpy(&v, data + offset, 8);
            offset += 8;
            prop.type = FBXPropType::INT64; prop.int_val = v;
            break;
        }
        case 'f':
        case 'd':
        case 'l':
        case 'i':
        case 'b': { // arrays
            if (offset + 12 > size) { out_error = "Truncated array header"; return false; }
            uint32_t arr_count, encoding, compressed_len;
            read_le32(arr_count);
            read_le32(encoding);
            read_le32(compressed_len);
            
            prop.array_count = arr_count;
            prop.array_encoding = encoding;
            prop.array_compressed_len = compressed_len;
            
            int elem_size = 0;
            switch (type_code) {
                case 'f': elem_size = 4; prop.type = FBXPropType::ARRAY_FLOAT; break;
                case 'd': elem_size = 8; prop.type = FBXPropType::ARRAY_DOUBLE; break;
                case 'l': elem_size = 8; prop.type = FBXPropType::ARRAY_INT64; break;
                case 'i': elem_size = 4; prop.type = FBXPropType::ARRAY_INT32; break;
                case 'b': elem_size = 1; prop.type = FBXPropType::ARRAY_BOOL; break;
            }
            
            int raw_size = arr_count * elem_size;
            
            if (encoding == 0) {
                if (offset + raw_size > size) { out_error = "Truncated array data"; return false; }
                prop.data.resize(raw_size);
                memcpy(prop.data.data(), data + offset, raw_size);
                offset += raw_size;
            } else if (encoding == 1) {
                if (offset + (int)compressed_len > size) { out_error = "Truncated compressed data"; return false; }
                prop.data.resize(raw_size);
                if (!_decompress_deflate(data + offset, compressed_len, prop.data.data(), raw_size, out_error)) {
                    return false;
                }
                offset += compressed_len;
            } else {
                out_error = "Unknown array encoding: " + String::num_int64(encoding);
                return false;
            }
            break;
        }
        case 'S': { // string
            if (offset + 4 > size) { out_error = "Truncated string length"; return false; }
            uint32_t len;
            read_le32(len);
            if (offset + (int)len > size) { out_error = "Truncated string data"; return false; }
            prop.type = FBXPropType::STRING;
            prop.data.resize(len);
            if (len > 0) memcpy(prop.data.data(), data + offset, len);
            offset += len;
            break;
        }
        case 'R': { // raw binary
            if (offset + 4 > size) { out_error = "Truncated raw length"; return false; }
            uint32_t len;
            read_le32(len);
            if (offset + (int)len > size) { out_error = "Truncated raw data"; return false; }
            prop.type = FBXPropType::RAW;
            prop.data.resize(len);
            if (len > 0) memcpy(prop.data.data(), data + offset, len);
            offset += len;
            break;
        }
        default:
            out_error = "Unknown property type code: " + String::chr(type_code);
            return false;
    }
    return true;
}

bool FBXDocument::_parse_record(const uint8_t* data, int size, int& offset, FBXRecord* parent,
                                uint32_t record_version, String& out_error) {
    bool is_64bit = (record_version >= 7500);
    int header_size = is_64bit ? 25 : 13;
    
    if (offset + header_size > size) {
        return true;
    }
    
    uint64_t end_offset, num_props, prop_list_len;
    uint8_t name_len;
    
    if (is_64bit) {
        memcpy(&end_offset, data + offset, 8);
        memcpy(&num_props, data + offset + 8, 8);
        memcpy(&prop_list_len, data + offset + 16, 8);
        name_len = data[offset + 24];
        offset += 25;
    } else {
        uint32_t eo, np, pl;
        memcpy(&eo, data + offset, 4);
        memcpy(&np, data + offset + 4, 4);
        memcpy(&pl, data + offset + 8, 4);
        end_offset = eo;
        num_props = np;
        prop_list_len = pl;
        name_len = data[offset + 12];
        offset += 13;
    }
    
    if (end_offset == 0 && num_props == 0 && prop_list_len == 0 && name_len == 0) {
        return true;
    }
    
    if (end_offset > (uint64_t)size) {
        out_error = "Record end offset exceeds file size";
        return false;
    }
    
    auto record = std::make_unique<FBXRecord>();
    
    if (offset + name_len > size) { out_error = "Truncated record name"; return false; }
    if (name_len > 0) {
        record->name.parse_utf8(reinterpret_cast<const char*>(data + offset), name_len);
    }
    offset += name_len;
    
    int props_end = offset + (int)prop_list_len;
    for (uint64_t i = 0; i < num_props; i++) {
        if (offset > props_end) {
            out_error = "Property list overflow";
            return false;
        }
        FBXProperty prop;
        if (!_parse_property(data, size, offset, prop, record_version, out_error)) {
            return false;
        }
        record->properties.push_back(std::move(prop));
    }
    
    if (offset < props_end) offset = props_end;
    
    while ((uint64_t)offset < end_offset) {
        int prev_offset = offset;
        bool ok = _parse_record(data, size, offset, record.get(), record_version, out_error);
        if (!ok) return false;
        if (offset == prev_offset) {
            if (offset + header_size <= size) {
                uint64_t eo, np, pl;
                uint8_t nl;
                if (is_64bit) {
                    memcpy(&eo, data + offset, 8);
                    memcpy(&np, data + offset + 8, 8);
                    memcpy(&pl, data + offset + 16, 8);
                    nl = data[offset + 24];
                } else {
                    uint32_t teo, tnp, tpl;
                    memcpy(&teo, data + offset, 4);
                    memcpy(&tnp, data + offset + 4, 4);
                    memcpy(&tpl, data + offset + 8, 4);
                    eo = teo; np = tnp; pl = tpl;
                    nl = data[offset + 12];
                }
                if (eo == 0 && np == 0 && pl == 0 && nl == 0) {
                    offset += header_size;
                    break;
                }
            }
            break;
        }
    }
    
    if ((uint64_t)offset > end_offset) {
        offset = (int)end_offset;
    }
    
    if (parent) {
        parent->children.push_back(std::move(record));
    } else {
        root = std::move(record);
    }
    
    return true;
}

bool FBXDocument::parse(const PackedByteArray& data, String& out_error) {
    if (!_parse_header(data.ptr(), data.size(), out_error)) {
        return false;
    }
    
    root = std::make_unique<FBXRecord>();
    root->name = "Root";
    
    int offset = 27;
    while (offset < data.size()) {
        int prev_offset = offset;
        bool ok = _parse_record(data.ptr(), data.size(), offset, root.get(), version, out_error);
        if (!ok) return false;
        if (offset == prev_offset) break;
    }
    
    build_connection_graph();
    classify_objects();
    return true;
}


// ============================================================================
// CONNECTION GRAPH & OBJECT CLASSIFICATION
// ============================================================================

void FBXDocument::build_connection_graph() {
    connections.clear();
    parent_of.clear();
    objects_by_id.clear();
    
    if (!root) return;
    
    const FBXRecord* objects = root->find_child("Objects");
    if (!objects) return;
    
    for (const auto& obj : objects->children) {
        if (obj->properties.empty()) continue;
        int64_t id = obj->properties[0].as_int();
        if (id != 0) {
            objects_by_id[id] = obj.get();
        }
    }
    
    const FBXRecord* conns = root->find_child("Connections");
    if (!conns) return;
    
    for (const auto& c : conns->children) {
        if (c->name != "C" && c->name != "Connect") continue;
        if (c->properties.size() < 3) continue;
        
        int64_t child_id = c->properties[1].as_int();
        int64_t parent_id = c->properties[2].as_int();
        
        if (child_id != 0 && parent_id != 0) {
            connections[parent_id].push_back(child_id);
            parent_of[child_id] = parent_id;
        }
    }
}

void FBXDocument::classify_objects() {
    geometry_ids.clear();
    model_ids.clear();
    limb_node_ids.clear();
    mesh_model_ids.clear();
    skin_ids.clear();
    cluster_ids.clear();
    anim_stack_ids.clear();
    anim_layer_ids.clear();
    anim_curve_node_ids.clear();
    anim_curve_ids.clear();
    pose_ids.clear();
    unit_scale = 0.01f;
    
    for (const auto& kv : objects_by_id) {
        int64_t id = kv.first;
        const FBXRecord* rec = kv.second;
        
        if (rec->name == "Geometry") {
            geometry_ids.push_back(id);
        } else if (rec->name == "Model") {
            model_ids.push_back(id);
            if (rec->properties.size() >= 3) {
                String type = rec->properties[2].as_string();
                if (type == "Mesh") {
                    mesh_model_ids.push_back(id);
                } else if (type == "LimbNode" || type == "Limb") {
                    limb_node_ids.push_back(id);
                }
            }
        } else if (rec->name == "Deformer") {
            if (rec->properties.size() >= 3) {
                String type = rec->properties[2].as_string();
                if (type == "Skin") {
                    skin_ids.push_back(id);
                } else if (type == "Cluster") {
                    cluster_ids.push_back(id);
                }
            }
        } else if (rec->name == "AnimationStack") {
            anim_stack_ids.push_back(id);
        } else if (rec->name == "AnimationLayer") {
            anim_layer_ids.push_back(id);
        } else if (rec->name == "AnimationCurveNode") {
            anim_curve_node_ids.push_back(id);
        } else if (rec->name == "AnimationCurve") {
            anim_curve_ids.push_back(id);
        } else if (rec->name == "Pose") {
            pose_ids.push_back(id);
        }
    }
    
    const FBXRecord* gs = root->find_child("GlobalSettings");
    if (gs) {
        const FBXRecord* props = gs->find_child("Properties70");
        if (props) {
            for (const auto& p : props->children) {
                if (p->name == "P" && p->properties.size() >= 5) {
                    String prop_name = p->properties[0].as_string();
                    if (prop_name == "UnitScaleFactor") {
                        unit_scale = p->properties[4].as_float() * 0.01f;
                    }
                }
            }
        }
    }
}

// ============================================================================
// VFX FBX IMPORTER — BINDINGS & LIFECYCLE
// ============================================================================

void VFXFBXImporter::_bind_methods() {
    ClassDB::bind_method(D_METHOD("import_fbx", "path"), &VFXFBXImporter::import_fbx);
    
    ClassDB::bind_static_method("VFXFBXImporter", D_METHOD("convert_position", "fbx_pos"), &VFXFBXImporter::convert_position);
    ClassDB::bind_static_method("VFXFBXImporter", D_METHOD("convert_normal", "fbx_normal"), &VFXFBXImporter::convert_normal);
    ClassDB::bind_static_method("VFXFBXImporter", D_METHOD("convert_rotation", "fbx_rot"), &VFXFBXImporter::convert_rotation);
    ClassDB::bind_static_method("VFXFBXImporter", D_METHOD("convert_transform", "fbx_transform"), &VFXFBXImporter::convert_transform);
}

VFXFBXImporter::VFXFBXImporter() {}
VFXFBXImporter::~VFXFBXImporter() {}

void VFXFBXImporter::_clear_state() {
    built_meshes.clear();
    built_skeletons.clear();
    built_animators.clear();
    built_skins.clear();
    built_nodes.clear();
    cluster_to_bone.clear();
    model_to_bone.clear();
    doc.reset();
}

// ============================================================================
// COORDINATE CONVERSION
// ============================================================================

Vector3 VFXFBXImporter::convert_position(const Vector3& fbx_pos) {
    return Vector3(-fbx_pos.x, fbx_pos.y, -fbx_pos.z);
}

Vector3 VFXFBXImporter::convert_normal(const Vector3& fbx_normal) {
    Vector3 n = Vector3(-fbx_normal.x, fbx_normal.y, -fbx_normal.z);
    return n.normalized();
}

Quaternion VFXFBXImporter::convert_rotation(const Quaternion& fbx_rot) {
    return Quaternion(-fbx_rot.x, fbx_rot.y, -fbx_rot.z, fbx_rot.w);
}

Transform3D VFXFBXImporter::convert_transform(const Transform3D& fbx_transform) {
    Basis b = fbx_transform.get_basis();
    Vector3 o = fbx_transform.get_origin();
    Basis converted;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float m_i = (i == 0 || i == 2) ? -1.0f : 1.0f;
            float m_j = (j == 0 || j == 2) ? -1.0f : 1.0f;
            converted[i][j] = m_i * b[i][j] * m_j;
        }
    }
    Vector3 o2 = Vector3(-o.x, o.y, -o.z);
    return Transform3D(converted, o2);
}

Vector3 VFXFBXImporter::_fbx_to_godot_pos(const Vector3& p) {
    return convert_position(p);
}

Vector3 VFXFBXImporter::_fbx_to_godot_n(const Vector3& n) {
    return convert_normal(n);
}

Quaternion VFXFBXImporter::_fbx_to_godot_q(const Quaternion& q) {
    return convert_rotation(q);
}

Transform3D VFXFBXImporter::_fbx_to_godot_transform(const Transform3D& t) {
    return convert_transform(t);
}

// ============================================================================
// FBX IMPORTER HELPERS
// ============================================================================

const vfx_fbx::FBXRecord* VFXFBXImporter::_get_object_record(int64_t obj_id) const {
    if (!doc) return nullptr;
    auto it = doc->objects_by_id.find(obj_id);
    if (it != doc->objects_by_id.end()) return it->second;
    return nullptr;
}

String VFXFBXImporter::_get_object_name(int64_t obj_id) const {
    const vfx_fbx::FBXRecord* rec = _get_object_record(obj_id);
    if (!rec) return "";
    if (rec->properties.size() >= 2) {
        return rec->properties[1].as_string();
    }
    return "";
}

String VFXFBXImporter::_get_object_type(int64_t obj_id) const {
    const vfx_fbx::FBXRecord* rec = _get_object_record(obj_id);
    if (!rec) return "";
    if (rec->properties.size() >= 3) {
        return rec->properties[2].as_string();
    }
    return "";
}

std::vector<int64_t> VFXFBXImporter::_get_children_of_type(int64_t parent_id, const String& type) const {
    std::vector<int64_t> result;
    if (!doc) return result;
    auto it = doc->connections.find(parent_id);
    if (it == doc->connections.end()) return result;
    for (int64_t child_id : it->second) {
        if (_get_object_type(child_id) == type) {
            result.push_back(child_id);
        }
    }
    return result;
}

int64_t VFXFBXImporter::_get_first_child_of_type(int64_t parent_id, const String& type) const {
    auto children = _get_children_of_type(parent_id, type);
    return children.empty() ? 0 : children[0];
}

Transform3D VFXFBXImporter::_get_model_transform(int64_t model_id) const {
    const vfx_fbx::FBXRecord* rec = _get_object_record(model_id);
    if (!rec) return Transform3D();
    
    Transform3D result;
    
    const vfx_fbx::FBXRecord* props70 = rec->find_child("Properties70");
    if (props70) {
        Vector3 lcl_translation;
        Vector3 lcl_rotation_deg;
        Vector3 lcl_scale(1, 1, 1);
        
        for (const auto& p : props70->children) {
            if (p->name != "P" || p->properties.empty()) continue;
            String prop_name = p->properties[0].as_string();
            
            if (prop_name == "Lcl Translation" && p->properties.size() >= 7) {
                lcl_translation = Vector3(
                    p->properties[4].as_float(),
                    p->properties[5].as_float(),
                    p->properties[6].as_float()
                );
            } else if (prop_name == "Lcl Rotation" && p->properties.size() >= 7) {
                lcl_rotation_deg = Vector3(
                    p->properties[4].as_float(),
                    p->properties[5].as_float(),
                    p->properties[6].as_float()
                );
            } else if (prop_name == "Lcl Scaling" && p->properties.size() >= 7) {
                lcl_scale = Vector3(
                    p->properties[4].as_float(),
                    p->properties[5].as_float(),
                    p->properties[6].as_float()
                );
            }
        }
        
        Vector3 rot_rad = lcl_rotation_deg * (3.14159265f / 180.0f);
        Basis rot_basis = Basis::from_euler(rot_rad);
        
        Basis scale_basis;
        scale_basis.scale(lcl_scale);
        
        result.set_basis(rot_basis * scale_basis);
        result.set_origin(lcl_translation);
    }
    
    return result;
}


// ============================================================================
// MESH EXTRACTION
// ============================================================================

Ref<VFXMesh> VFXFBXImporter::_build_mesh(int64_t geom_id, String& out_error) {
    auto it = built_meshes.find(geom_id);
    if (it != built_meshes.end()) return it->second;

    const vfx_fbx::FBXRecord* geom = _get_object_record(geom_id);
    if (!geom || geom->name != "Geometry") {
        out_error = "Invalid geometry object";
        return Ref<VFXMesh>();
    }

    Ref<VFXMesh> mesh;
    mesh.instantiate();

    const vfx_fbx::FBXRecord* verts_rec = geom->find_child("Vertices");
    const vfx_fbx::FBXRecord* indices_rec = geom->find_child("PolygonVertexIndex");
    
    if (!verts_rec || verts_rec->properties.empty()) {
        out_error = "Geometry missing Vertices";
        return Ref<VFXMesh>();
    }
    if (!indices_rec || indices_rec->properties.empty()) {
        out_error = "Geometry missing PolygonVertexIndex";
        return Ref<VFXMesh>();
    }

    PackedVector3Array positions = verts_rec->properties[0].as_vec3_array();
    for (int i = 0; i < positions.size(); i++) {
        positions[i] = _fbx_to_godot_pos(positions[i] * doc->unit_scale);
    }

    PackedInt32Array poly_indices = indices_rec->properties[0].as_int_array();

    PackedVector3Array normals;
    const vfx_fbx::FBXRecord* normals_layer = geom->find_child("LayerElementNormal");
    if (normals_layer) {
        const vfx_fbx::FBXRecord* norms_rec = normals_layer->find_child("Normals");
        if (norms_rec && !norms_rec->properties.empty()) {
            normals = norms_rec->properties[0].as_vec3_array();
            for (int i = 0; i < normals.size(); i++) {
                normals[i] = _fbx_to_godot_n(normals[i]);
            }
        }
    }

    PackedVector2Array uvs;
    const vfx_fbx::FBXRecord* uv_layer = geom->find_child("LayerElementUV");
    if (uv_layer) {
        const vfx_fbx::FBXRecord* uvs_rec = uv_layer->find_child("UV");
        if (uvs_rec && !uvs_rec->properties.empty()) {
            uvs = uvs_rec->properties[0].as_vec2_array();
        }
    }

    std::vector<int> vert_remap;
    vert_remap.resize(positions.size());
    for (int i = 0; i < positions.size(); i++) {
        Vector2 uv = (i < uvs.size()) ? uvs[i] : Vector2();
        vert_remap[i] = mesh->add_vertex(positions[i], uv, Color(1,1,1,1));
    }

    std::vector<int> poly;
    for (int i = 0; i < poly_indices.size(); i++) {
        int idx = poly_indices[i];
        bool is_last = (idx < 0);
        if (is_last) idx = -idx - 1;
        
        poly.push_back(idx);
        
        if (is_last) {
            if (poly.size() >= 3) {
                for (size_t j = 1; j + 1 < poly.size(); j++) {
                    mesh->add_triangle(
                        vert_remap[poly[0]],
                        vert_remap[poly[j]],
                        vert_remap[poly[j+1]]
                    );
                }
            }
            poly.clear();
        }
    }

    for (int64_t skin_id : doc->skin_ids) {
        auto it_conn = doc->connections.find(skin_id);
        if (it_conn == doc->connections.end()) continue;
        
        for (int64_t child_id : it_conn->second) {
            if (std::find(doc->cluster_ids.begin(), doc->cluster_ids.end(), child_id) == doc->cluster_ids.end()) 
                continue;
            
            const vfx_fbx::FBXRecord* cluster = _get_object_record(child_id);
            if (!cluster) continue;
            
            const vfx_fbx::FBXRecord* idx_rec = cluster->find_child("Indexes");
            const vfx_fbx::FBXRecord* w_rec = cluster->find_child("Weights");
            
            if (!idx_rec || !w_rec || idx_rec->properties.empty() || w_rec->properties.empty())
                continue;
            
            PackedInt32Array c_indices = idx_rec->properties[0].as_int_array();
            PackedFloat32Array c_weights = w_rec->properties[0].as_float_array();
            
            auto bone_it = cluster_to_bone.find(child_id);
            if (bone_it == cluster_to_bone.end()) continue;
            int bone_idx = bone_it->second;
            
            for (int i = 0; i < c_indices.size() && i < c_weights.size(); i++) {
                int cp_idx = c_indices[i];
                if (cp_idx < 0 || cp_idx >= (int)vert_remap.size()) continue;
                
                int vfx_vidx = vert_remap[cp_idx];
                float weight = c_weights[i];
                
                int bones[4]; float weights[4];
                mesh->get_vertex_skinning(vfx_vidx, bones, weights);
                
                for (int s = 0; s < 4; s++) {
                    if (weights[s] < 0.001f) {
                        bones[s] = bone_idx;
                        weights[s] = weight;
                        break;
                    }
                }
                mesh->set_vertex_skinning(vfx_vidx, bones, weights);
            }
        }
    }

    int vcount = mesh->get_vertex_count();
    for (int i = 0; i < vcount; i++) {
        mesh->normalize_weights(i);
    }

    mesh->recalculate_normals();
    mesh->recalculate_bounds();
    
    built_meshes[geom_id] = mesh;
    return mesh;
}


// ============================================================================
// SKELETON EXTRACTION
// ============================================================================

Ref<VFXSkeleton> VFXFBXImporter::_build_skeleton(int64_t skin_id, String& out_error) {
    auto it = built_skeletons.find(skin_id);
    if (it != built_skeletons.end()) return it->second;

    auto it_conn = doc->connections.find(skin_id);
    if (it_conn == doc->connections.end()) {
        out_error = "Skin has no clusters";
        return Ref<VFXSkeleton>();
    }

    Ref<VFXSkeleton> skeleton;
    skeleton.instantiate();

    std::vector<int64_t> cluster_ids_ordered;
    for (int64_t child_id : it_conn->second) {
        if (std::find(doc->cluster_ids.begin(), doc->cluster_ids.end(), child_id) != doc->cluster_ids.end()) {
            cluster_ids_ordered.push_back(child_id);
        }
    }

    if (cluster_ids_ordered.empty()) {
        out_error = "No clusters found in skin";
        return Ref<VFXSkeleton>();
    }

    for (size_t i = 0; i < cluster_ids_ordered.size(); i++) {
        cluster_to_bone[cluster_ids_ordered[i]] = (int)i;
    }

    for (size_t i = 0; i < cluster_ids_ordered.size(); i++) {
        int64_t cluster_id = cluster_ids_ordered[i];
        const vfx_fbx::FBXRecord* cluster = _get_object_record(cluster_id);
        if (!cluster) continue;

        int64_t linked_model = 0;
        auto cit = doc->connections.find(cluster_id);
        if (cit != doc->connections.end()) {
            for (int64_t child : cit->second) {
                if (std::find(doc->limb_node_ids.begin(), doc->limb_node_ids.end(), child) != doc->limb_node_ids.end()) {
                    linked_model = child;
                    break;
                }
            }
        }

        String bone_name = linked_model ? _get_object_name(linked_model) : ("bone_" + String::num_int64(i));
        int parent_bone = -1;

        if (linked_model) {
            model_to_bone[linked_model] = (int)i;
            
            auto pit = doc->parent_of.find(linked_model);
            if (pit != doc->parent_of.end()) {
                int64_t parent_model = pit->second;
                auto bit = model_to_bone.find(parent_model);
                if (bit != model_to_bone.end()) {
                    parent_bone = bit->second;
                }
            }
        }

        skeleton->add_bone(bone_name, parent_bone);
    }

    for (size_t i = 0; i < cluster_ids_ordered.size(); i++) {
        int64_t cluster_id = cluster_ids_ordered[i];
        const vfx_fbx::FBXRecord* cluster = _get_object_record(cluster_id);
        if (!cluster) continue;

        const vfx_fbx::FBXRecord* tl_rec = cluster->find_child("TransformLink");
        if (tl_rec && !tl_rec->properties.empty()) {
            PackedFloat32Array mat = tl_rec->properties[0].as_float_array();
            if (mat.size() >= 16) {
                Basis b;
                b[0] = Vector3(mat[0], mat[1], mat[2]);
                b[1] = Vector3(mat[4], mat[5], mat[6]);
                b[2] = Vector3(mat[8], mat[9], mat[10]);
                Vector3 o(mat[12], mat[13], mat[14]);
                Transform3D bind_pose = _fbx_to_godot_transform(Transform3D(b, o));
                skeleton->set_bone_bind_pose((int)i, bind_pose);
            }
        }
    }

    skeleton->update_transforms();
    built_skeletons[skin_id] = skeleton;
    return skeleton;
}


// ============================================================================
// ANIMATION EXTRACTION
// ============================================================================

void VFXFBXImporter::_build_animations(Ref<VFXAnimator> animator, const Ref<VFXSkeleton>& skeleton, String& out_error) {
    if (!doc || doc->anim_stack_ids.empty()) return;
    if (skeleton.is_null() || skeleton->get_bone_count() == 0) return;

    for (int64_t stack_id : doc->anim_stack_ids) {
        String stack_name = _get_object_name(stack_id);
        if (stack_name.is_empty()) stack_name = "Animation";

        auto it = doc->connections.find(stack_id);
        if (it == doc->connections.end()) continue;

        float duration = 0.0f;

        std::vector<int64_t> curve_nodes;
        for (int64_t layer_id : it->second) {
            if (std::find(doc->anim_layer_ids.begin(), doc->anim_layer_ids.end(), layer_id) == doc->anim_layer_ids.end())
                continue;
            
            auto lit = doc->connections.find(layer_id);
            if (lit == doc->connections.end()) continue;
            
            for (int64_t cn_id : lit->second) {
                if (std::find(doc->anim_curve_node_ids.begin(), doc->anim_curve_node_ids.end(), cn_id) != doc->anim_curve_node_ids.end()) {
                    curve_nodes.push_back(cn_id);
                }
            }
        }

        if (curve_nodes.empty()) continue;

        for (int64_t cn_id : curve_nodes) {
            auto cnit = doc->connections.find(cn_id);
            if (cnit == doc->connections.end()) continue;
            
            for (int64_t curve_id : cnit->second) {
                if (std::find(doc->anim_curve_ids.begin(), doc->anim_curve_ids.end(), curve_id) == doc->anim_curve_ids.end())
                    continue;
                
                const vfx_fbx::FBXRecord* curve = _get_object_record(curve_id);
                if (!curve) continue;
                
                const vfx_fbx::FBXRecord* keytime_rec = curve->find_child("KeyTime");
                if (keytime_rec && !keytime_rec->properties.empty()) {
                    PackedFloat32Array times = keytime_rec->properties[0].as_float_array();
                    for (int i = 0; i < times.size(); i++) {
                        float t_sec = times[i] / 46186158000.0f;
                        duration = (t_sec > duration) ? t_sec : duration;
                    }
                }
            }
        }

        if (duration <= 0.0f) duration = 1.0f;
        int clip_idx = animator->create_clip(stack_name, duration, 30.0f);

        for (int64_t cn_id : curve_nodes) {
            const vfx_fbx::FBXRecord* cn = _get_object_record(cn_id);
            if (!cn) continue;

            int bone_idx = -1;
            auto cnit = doc->connections.find(cn_id);
            if (cnit != doc->connections.end()) {
                for (int64_t model_id : cnit->second) {
                    auto bit = model_to_bone.find(model_id);
                    if (bit != model_to_bone.end()) {
                        bone_idx = bit->second;
                        break;
                    }
                }
            }
            if (bone_idx < 0) continue;

            String cn_name = _get_object_name(cn_id);
            bool is_translation = cn_name.contains("T") || cn_name.contains("Translation");
            bool is_rotation = cn_name.contains("R") || cn_name.contains("Rotation");
            bool is_scale = cn_name.contains("S") || cn_name.contains("Scaling");

            if (!is_translation && !is_rotation && !is_scale) {
                const vfx_fbx::FBXRecord* props = cn->find_child("Properties70");
                if (props) {
                    for (const auto& p : props->children) {
                        if (p->properties.empty()) continue;
                        String pname = p->properties[0].as_string();
                        if (pname.contains("d|X") || pname.contains("d|Y") || pname.contains("d|Z")) {
                            is_translation = true;
                        }
                    }
                }
                if (!is_translation && !is_rotation && !is_scale) {
                    is_translation = true;
                }
            }

            auto curve_conn_it = doc->connections.find(cn_id);
            if (curve_conn_it == doc->connections.end()) continue;

            std::vector<int64_t> curves;
            for (int64_t curve_id : curve_conn_it->second) {
                if (std::find(doc->anim_curve_ids.begin(), doc->anim_curve_ids.end(), curve_id) != doc->anim_curve_ids.end()) {
                    curves.push_back(curve_id);
                }
            }

            if (curves.empty()) continue;

            String curve_name = skeleton->get_bone_name(bone_idx) + "_" + 
                (is_translation ? "translation" : (is_rotation ? "rotation" : "scale"));

            int curve_idx = animator->add_curve(clip_idx, curve_name, bone_idx, is_rotation, is_scale);

            // Collect all unique key times from all curves for this node
            std::vector<float> all_times_vec;
            for (int64_t curve_id : curves) {
                const vfx_fbx::FBXRecord* curve = _get_object_record(curve_id);
                if (!curve) continue;
                const vfx_fbx::FBXRecord* kt = curve->find_child("KeyTime");
                if (!kt || kt->properties.empty()) continue;
                PackedFloat32Array t = kt->properties[0].as_float_array();
                for (int i = 0; i < t.size(); i++) {
                    all_times_vec.push_back(t[i] / 46186158000.0f);
                }
            }
            
            std::sort(all_times_vec.begin(), all_times_vec.end());
            all_times_vec.erase(std::unique(all_times_vec.begin(), all_times_vec.end(), 
                [](float a, float b){ return fabs(a - b) < 0.0001f; }), all_times_vec.end());

            for (float t : all_times_vec) {
                Vector3 value;
                
                for (size_t ci = 0; ci < curves.size() && ci < 3; ci++) {
                    int64_t curve_id = curves[ci];
                    const vfx_fbx::FBXRecord* curve = _get_object_record(curve_id);
                    float comp_val = 0.0f;
                    
                    if (curve) {
                        const vfx_fbx::FBXRecord* kt = curve->find_child("KeyTime");
                        const vfx_fbx::FBXRecord* kv = curve->find_child("KeyValueFloat");
                        if (kt && kv && !kt->properties.empty() && !kv->properties.empty()) {
                            PackedFloat32Array ctimes = kt->properties[0].as_float_array();
                            PackedFloat32Array cvals = kv->properties[0].as_float_array();
                            
                            if (ctimes.size() > 0 && cvals.size() > 0) {
                                if (t <= ctimes[0] / 46186158000.0f) {
                                    comp_val = cvals[0];
                                } else if (t >= ctimes[ctimes.size()-1] / 46186158000.0f) {
                                    comp_val = cvals[cvals.size()-1];
                                } else {
                                    for (int i = 0; i < (int)ctimes.size() - 1; i++) {
                                        float t0 = ctimes[i] / 46186158000.0f;
                                        float t1 = ctimes[i+1] / 46186158000.0f;
                                        if (t >= t0 && t <= t1) {
                                            if (t1 - t0 < 0.0001f) {
                                                comp_val = cvals[i];
                                            } else {
                                                float lerp_t = (t - t0) / (t1 - t0);
                                                comp_val = cvals[i] + (cvals[i+1] - cvals[i]) * lerp_t;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    if (ci == 0) value.x = comp_val;
                    else if (ci == 1) value.y = comp_val;
                    else value.z = comp_val;
                }

                if (is_rotation) {
                    Vector3 rot_rad = value * (3.14159265f / 180.0f);
                    Basis b = Basis::from_euler(rot_rad);
                    Quaternion q = b.get_rotation_quaternion();
                    q = _fbx_to_godot_q(q);
                    animator->add_keyframe_quaternion(clip_idx, curve_idx, t, q, VFXAnimator::INTERP_LINEAR);
                } else if (is_translation) {
                    value = _fbx_to_godot_pos(value * doc->unit_scale);
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, value, VFXAnimator::INTERP_LINEAR);
                } else {
                    animator->add_keyframe_vector(clip_idx, curve_idx, t, value, VFXAnimator::INTERP_LINEAR);
                }
            }
        }
    }
}

// ============================================================================
// MAIN IMPORT ENTRY POINT
// ============================================================================

Dictionary VFXFBXImporter::import_fbx(const String& path) {
    Dictionary result;
    result[KEY_SUCCESS] = false;
    result[KEY_ERROR] = "";
    result[KEY_SCENE] = Variant();
    result[KEY_MESH_NODES] = Array();
    result[KEY_MESH] = Variant();
    result[KEY_SKELETON] = Variant();
    result[KEY_SKIN] = Variant();
    result[KEY_ANIMATOR] = Variant();
    result[KEY_MATERIAL_COUNT] = 0;
    result[KEY_ANIMATION_COUNT] = 0;
    result[KEY_NODE_COUNT] = 0;
    result[KEY_MESH_COUNT] = 0;

    _clear_state();

    Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ);
    if (f.is_null()) {
        result[KEY_ERROR] = "Failed to open file: " + path;
        return result;
    }

    PackedByteArray data = f->get_buffer(f->get_length());
    f->close();

    doc = std::make_unique<vfx_fbx::FBXDocument>();
    String error;
    if (!doc->parse(data, error)) {
        result[KEY_ERROR] = error;
        return result;
    }

    result[KEY_NODE_COUNT] = (int)doc->model_ids.size();
    result[KEY_MESH_COUNT] = (int)doc->mesh_model_ids.size();
    result[KEY_ANIMATION_COUNT] = (int)doc->anim_stack_ids.size();

    Ref<VFXScene> scene;
    scene.instantiate();
    scene->create_default_root();

    std::vector<Ref<VFXSceneNode>> vfx_nodes;
    for (int64_t model_id : doc->model_ids) {
        Ref<VFXSceneNode> node;
        node.instantiate();
        node->set_node_name(_get_object_name(model_id));
        node->set_local_transform(_get_model_transform(model_id));
        node->set_node_type(VFXSceneNode::NODE_EMPTY);
        built_nodes[model_id] = node;
        vfx_nodes.push_back(node);
    }

    for (int64_t model_id : doc->model_ids) {
        auto pit = doc->parent_of.find(model_id);
        if (pit != doc->parent_of.end()) {
            int64_t parent_id = pit->second;
            auto nit = built_nodes.find(parent_id);
            if (nit != built_nodes.end()) {
                auto cit = built_nodes.find(model_id);
                if (cit != built_nodes.end()) {
                    nit->second->add_child(cit->second);
                }
            }
        }
    }

    for (int64_t model_id : doc->model_ids) {
        auto pit = doc->parent_of.find(model_id);
        if (pit == doc->parent_of.end() || pit->second == 0) {
            auto nit = built_nodes.find(model_id);
            if (nit != built_nodes.end()) {
                scene->get_root()->add_child(nit->second);
            }
        }
    }

    Ref<VFXMesh> first_mesh;
    Ref<VFXSkeleton> first_skeleton;
    Ref<VFXSkin> first_skin;
    Ref<VFXAnimator> first_animator;
    Array mesh_nodes_array;

    for (int64_t model_id : doc->mesh_model_ids) {
        auto it = doc->connections.find(model_id);
        if (it == doc->connections.end()) continue;

        int64_t geom_id = 0;
        for (int64_t child_id : it->second) {
            if (std::find(doc->geometry_ids.begin(), doc->geometry_ids.end(), child_id) != doc->geometry_ids.end()) {
                geom_id = child_id;
                break;
            }
        }
        if (geom_id == 0) continue;

        String err;
        Ref<VFXMesh> mesh = _build_mesh(geom_id, err);
        if (mesh.is_null()) continue;

        auto nit = built_nodes.find(model_id);
        if (nit == built_nodes.end()) continue;

        nit->second->set_node_type(VFXSceneNode::NODE_MESH);
        nit->second->set_mesh(mesh);
        mesh_nodes_array.append(nit->second);

        if (!first_mesh.is_valid()) first_mesh = mesh;

        int64_t skin_id = 0;
        for (int64_t child_id : it->second) {
            if (std::find(doc->skin_ids.begin(), doc->skin_ids.end(), child_id) != doc->skin_ids.end()) {
                skin_id = child_id;
                break;
            }
        }

        if (skin_id != 0) {
            Ref<VFXSkeleton> skeleton = _build_skeleton(skin_id, err);
            if (skeleton.is_valid()) {
                Ref<VFXSkin> skin;
                skin.instantiate();
                skin->set_mesh(mesh);
                skin->set_skeleton(skeleton);
                skin->auto_weight_from_bones(4);

                nit->second->set_skeleton(skeleton);
                nit->second->set_skin(skin);

                if (!first_skeleton.is_valid()) first_skeleton = skeleton;
                if (!first_skin.is_valid()) first_skin = skin;

                Ref<VFXAnimator> animator;
                animator.instantiate();
                _build_animations(animator, skeleton, err);
                if (animator->get_clip_count() > 0) {
                    nit->second->set_animator(animator);
                    if (!first_animator.is_valid()) first_animator = animator;
                }
            }
        }
    }

    result[KEY_SCENE] = scene;
    result[KEY_MESH_NODES] = mesh_nodes_array;
    if (first_mesh.is_valid()) result[KEY_MESH] = first_mesh;
    if (first_skeleton.is_valid()) result[KEY_SKELETON] = first_skeleton;
    if (first_skin.is_valid()) result[KEY_SKIN] = first_skin;
    if (first_animator.is_valid()) result[KEY_ANIMATOR] = first_animator;
    result[KEY_SUCCESS] = true;
    return result;
}
