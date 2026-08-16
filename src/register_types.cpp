#include "register_types.h"

#include "vfx_editor_node.h"
#include "vfx_mesh.h"
#include "vfx_skeleton.h"
#include "vfx_skin.h"
#include "vfx_animator.h"
#include "vfx_gltf_exporter.h"
#include "vfx_curve.h"
#include "vfx_uv_editor.h"
#include "vfx_retopology.h"
#include "vfx_texture_painter.h"

void initialize_vfx_toolkit(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        ClassDB::register_class<VFXEditorNode>();
        ClassDB::register_class<VFXMesh>();
        ClassDB::register_class<VFXSkeleton>();
        ClassDB::register_class<VFXSkin>();
        ClassDB::register_class<VFXAnimator>();
        ClassDB::register_class<VFXGLTFExporter>();
        ClassDB::register_class<VFXCurve>();
        ClassDB::register_class<VFXUVEditor>();
        ClassDB::register_class<VFXRetopology>();
        ClassDB::register_class<VFXTexturePainter>();
    }
}

void uninitialize_vfx_toolkit(ModuleInitializationLevel p_level) {
    // Cleanup if needed
}

extern "C" {
GDExtensionBool GDE_EXPORT vfx_toolkit_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization) {

    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_vfx_toolkit);
    init_obj.register_terminator(uninitialize_vfx_toolkit);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
