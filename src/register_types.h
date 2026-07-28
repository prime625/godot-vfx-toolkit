#ifndef VFX_REGISTER_TYPES_H
#define VFX_REGISTER_TYPES_H

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_vfx_toolkit(ModuleInitializationLevel p_level);
void uninitialize_vfx_toolkit(ModuleInitializationLevel p_level);

#endif
