        if (bone_hit >= 0) {
            set_selected_bone(bone_hit);
            return bone_hit;
        }
        // Empty click -> deselect bone (mirrors mesh-edit mode behavior)
        clear_selection();

    }
    return -1;
}