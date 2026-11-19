#ifndef POLYPIXEL_VFX_DATA_H
#define POLYPIXEL_VFX_DATA_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

using namespace godot;

// Runtime-only payload exported by PolyPixel's VFX Graph compiler.
// The graph editor's nodes and authoring metadata are deliberately not stored here.
class PolyPixelVFXData : public Resource {
    GDCLASS(PolyPixelVFXData, Resource)

private:
    static constexpr int CURRENT_FORMAT_VERSION = 1;
    int format_version = CURRENT_FORMAT_VERSION;
    PackedByteArray compiled_data;
    String effect_name;

protected:
    static void _bind_methods();

public:
    void set_format_version(int value);
    int get_format_version() const;

    void set_compiled_data(const PackedByteArray &value);
    PackedByteArray get_compiled_data() const;

    void set_effect_name(const String &value);
    String get_effect_name() const;

    bool is_valid() const;
    int get_payload_size() const;
};

#endif // POLYPIXEL_VFX_DATA_H
