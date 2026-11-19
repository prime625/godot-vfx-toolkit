#include "polypixel_vfx_data.h"

using namespace godot;

void PolyPixelVFXData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_format_version", "version"), &PolyPixelVFXData::set_format_version);
    ClassDB::bind_method(D_METHOD("get_format_version"), &PolyPixelVFXData::get_format_version);
    ClassDB::bind_method(D_METHOD("set_compiled_data", "data"), &PolyPixelVFXData::set_compiled_data);
    ClassDB::bind_method(D_METHOD("get_compiled_data"), &PolyPixelVFXData::get_compiled_data);
    ClassDB::bind_method(D_METHOD("set_effect_name", "name"), &PolyPixelVFXData::set_effect_name);
    ClassDB::bind_method(D_METHOD("get_effect_name"), &PolyPixelVFXData::get_effect_name);
    ClassDB::bind_method(D_METHOD("is_valid"), &PolyPixelVFXData::is_valid);
    ClassDB::bind_method(D_METHOD("get_payload_size"), &PolyPixelVFXData::get_payload_size);

    // Storage-only: serialized with the Resource, but not exposed as editable inspector fields.
    ADD_PROPERTY(PropertyInfo(Variant::INT, "format_version", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
            "set_format_version", "get_format_version");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "compiled_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
            "set_compiled_data", "get_compiled_data");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "effect_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
            "set_effect_name", "get_effect_name");
}

void PolyPixelVFXData::set_format_version(int value) {
    format_version = value;
}

int PolyPixelVFXData::get_format_version() const {
    return format_version;
}

void PolyPixelVFXData::set_compiled_data(const PackedByteArray &value) {
    compiled_data = value;
}

PackedByteArray PolyPixelVFXData::get_compiled_data() const {
    return compiled_data;
}

void PolyPixelVFXData::set_effect_name(const String &value) {
    effect_name = value;
}

String PolyPixelVFXData::get_effect_name() const {
    return effect_name;
}

bool PolyPixelVFXData::is_valid() const {
    // Stable file signature: ASCII "PPVFX" followed by the binary payload.
    if (format_version != CURRENT_FORMAT_VERSION || compiled_data.size() < 5) {
        return false;
    }
    return compiled_data[0] == 'P' && compiled_data[1] == 'P' &&
            compiled_data[2] == 'V' && compiled_data[3] == 'F' &&
            compiled_data[4] == 'X';
}

int PolyPixelVFXData::get_payload_size() const {
    return compiled_data.size();
}
