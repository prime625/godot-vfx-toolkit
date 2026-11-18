extends Control


class MaterialTextureDropTarget extends TextureRect:
	var drop_callback: Callable

	func _init() -> void:
		custom_minimum_size = Vector2(48, 48)
		expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED
		mouse_filter = Control.MOUSE_FILTER_STOP
		tooltip_text = "Drop an image file here to assign this texture"

	func _can_drop_data(_at_position: Vector2, data: Variant) -> bool:
		if not (data is Dictionary) or data.get("type", "") != "files":
			return false
		var files: Array = data.get("files", [])
		if files.is_empty():
			return false
		var extension := String(files[0]).get_extension().to_lower()
		return extension in ["png", "jpg", "jpeg", "webp", "bmp", "tga", "exr", "hdr"]

	func _drop_data(_at_position: Vector2, data: Variant) -> void:
		if not _can_drop_data(_at_position, data):
			return
		var files: Array = data.get("files", [])
		if not files.is_empty() and drop_callback.is_valid():
			drop_callback.call(String(files[0]))

class MaterialPreviewDragViewport extends SubViewportContainer:
	var preview_mesh: MeshInstance3D
	var _dragging := false
	var _last_pointer_position := Vector2.ZERO
	var _touch_index := -1

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_STOP
		clip_contents = true

	func _gui_input(event: InputEvent) -> void:
		if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			_dragging = event.pressed
			_last_pointer_position = event.position
			accept_event()
		elif event is InputEventMouseMotion and _dragging:
			_rotate_preview(event.position - _last_pointer_position)
			_last_pointer_position = event.position
			accept_event()
		elif event is InputEventScreenTouch:
			if event.pressed and _touch_index == -1:
				_touch_index = event.index
				_last_pointer_position = event.position
			elif not event.pressed and event.index == _touch_index:
				_touch_index = -1
			accept_event()
		elif event is InputEventScreenDrag and event.index == _touch_index:
			_rotate_preview(event.position - _last_pointer_position)
			_last_pointer_position = event.position
			accept_event()

	func _rotate_preview(delta: Vector2) -> void:
		if not is_instance_valid(preview_mesh):
			return
		preview_mesh.rotation_degrees.y -= delta.x * 0.55
		preview_mesh.rotation_degrees.x = clampf(
			preview_mesh.rotation_degrees.x - delta.y * 0.55,
			-85.0,
			85.0
		)


class CurveViewportOverlay extends Control:
	var curve: VFXCurve
	var camera_3d: Camera3D
	var viewport_size_ref: Vector2i
	var selected_point: int = -1
	var show_all_handles: bool = false

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
		set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	func _screen_pos(world_pos: Vector3) -> Vector2:
		if camera_3d == null or viewport_size_ref.x <= 0 or viewport_size_ref.y <= 0:
			return Vector2(-10000, -10000)
		if camera_3d.is_position_behind(world_pos):
			return Vector2(-10000, -10000)
		var p := camera_3d.unproject_position(world_pos)
		return Vector2(p.x * size.x / viewport_size_ref.x, p.y * size.y / viewport_size_ref.y)

	func _endpoint_grip_screen(index: int) -> Vector2:
		if curve == null or camera_3d == null or curve.get_point_count() < 2:
			return Vector2(-10000, -10000)
		var count := curve.get_point_count()
		var point := curve.get_point(index)
		var neighbour := curve.get_point(1) if index == 0 else curve.get_point(count - 2)
		if camera_3d.is_position_behind(point) or camera_3d.is_position_behind(neighbour):
			return Vector2(-10000, -10000)
		var a := _screen_pos(point)
		var b := _screen_pos(neighbour)
		var direction := (b - a).normalized()
		if direction.length_squared() < 0.001:
			direction = Vector2.RIGHT
		var perpendicular := Vector2(-direction.y, direction.x)
		return a + perpendicular * 42.0

	func _draw() -> void:
		if curve == null or camera_3d == null:
			return
		var samples := curve.tessellate(12)
		if samples.size() >= 2:
			var line := PackedVector2Array()
			for p in samples:
				line.append(_screen_pos(p))
			draw_polyline(line, Color(0.25, 0.85, 1.0, 0.95), 3.0, true)
		if curve.get_point_count() >= 2:
			for endpoint in [0, curve.get_point_count() - 1]:
				var endpoint_pos := _screen_pos(curve.get_point(endpoint))
				var grip_pos := _endpoint_grip_screen(endpoint)
				if grip_pos.x > -1000 and endpoint_pos.x > -1000:
					var grip_color := Color(0.2, 0.9, 0.45) if endpoint == 0 else Color(0.75, 0.55, 1.0)
					draw_line(endpoint_pos, grip_pos, grip_color, 3.0, true)
					draw_circle(grip_pos, 12.0, grip_color.darkened(0.25))
					draw_arc(grip_pos, 17.0, 0.0, TAU, 24, Color(0.02, 0.08, 0.12), 2.5)
					draw_circle(grip_pos, 3.0, Color.WHITE)
		for i in range(curve.get_point_count()):
			var p := curve.get_point(i)
			var screen := _screen_pos(p)
			if screen.x < -1000:
				continue
			if i == selected_point or show_all_handles:
				var hin := _screen_pos(curve.get_handle_in(i))
				var hout := _screen_pos(curve.get_handle_out(i))
				draw_line(screen, hin, Color(1.0, 0.72, 0.2, 0.95), 2.0, true)
				draw_line(screen, hout, Color(1.0, 0.72, 0.2, 0.95), 2.0, true)
				draw_circle(hin, 9.0, Color(1.0, 0.55, 0.15))
				draw_circle(hout, 9.0, Color(1.0, 0.55, 0.15))
				draw_arc(hin, 11.0, 0.0, TAU, 20, Color.BLACK, 1.5)
				draw_arc(hout, 11.0, 0.0, TAU, 20, Color.BLACK, 1.5)
			draw_circle(screen, 12.0 if i == selected_point else 8.0,
				Color(1.0, 0.85, 0.2) if i == selected_point else Color(0.15, 0.9, 1.0))
			draw_arc(screen, 14.0, 0.0, TAU, 24, Color(0.03, 0.08, 0.12), 2.0)


# Screen-space handles for the guided humanoid workflow. The handles are only
# a selection aid: movement continues to use the editor's normal transform gizmo.
class HumanoidJointOverlay extends Control:
	var skeleton: VFXSkeleton
	var camera_3d: Camera3D
	var armature_transform: Transform3D = Transform3D.IDENTITY
	var viewport_size_ref: Vector2i
	var selected_bone: int = -1
	var guide_visible: bool = false

	func _ready() -> void:
		mouse_filter = Control.MOUSE_FILTER_IGNORE
		set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	func _screen_pos(world_pos: Vector3) -> Vector2:
		if camera_3d == null or viewport_size_ref.x <= 0 or viewport_size_ref.y <= 0:
			return Vector2(-10000, -10000)
		if camera_3d.is_position_behind(world_pos):
			return Vector2(-10000, -10000)
		var p := camera_3d.unproject_position(world_pos)
		return Vector2(p.x * size.x / viewport_size_ref.x, p.y * size.y / viewport_size_ref.y)

	func hit_test(screen_pos: Vector2, max_distance: float = 22.0) -> int:
		if not guide_visible or skeleton == null or camera_3d == null:
			return -1
		var best_index := -1
		var best_distance := max_distance
		for i in skeleton.get_bone_count():
			var p := _screen_pos((armature_transform * skeleton.get_bone_model_transform(i)).origin)
			if p.x < -1000:
				continue
			var d := p.distance_to(screen_pos)
			if d < best_distance:
				best_distance = d
				best_index = i
		return best_index

	func _joint_color(bone_name: String) -> Color:
		var n := bone_name.to_lower()
		if "head" in n or "neck" in n:
			return Color(0.20, 0.86, 0.82)
		if "arm" in n or "shoulder" in n or "hand" in n or "forearm" in n:
			return Color(1.0, 0.66, 0.24)
		if "leg" in n or "upleg" in n or "knee" in n or "foot" in n or "toe" in n:
			return Color(0.96, 0.40, 0.34)
		return Color(1.0, 0.83, 0.30)

	func _draw() -> void:
		if not guide_visible or skeleton == null or camera_3d == null:
			return
		for i in skeleton.get_bone_count():
			var bone_name := skeleton.get_bone_name(i)
			var p := _screen_pos((armature_transform * skeleton.get_bone_model_transform(i)).origin)
			if p.x < -1000:
				continue
			var c := _joint_color(bone_name)
			var selected := i == selected_bone
			var radius := 10.0 if selected else 7.0
			if selected:
				draw_circle(p, 17.0, Color(c.r, c.g, c.b, 0.20))
			draw_circle(p, radius, c)
			draw_arc(p, radius + 3.0, 0.0, TAU, 24, Color(0.035, 0.045, 0.06, 0.98), 2.0)
			draw_circle(p, 2.5, Color.WHITE)
			# Labels are deliberately compact so they remain legible on mobile.
			var label := bone_name.replace("mixamorig:", "").replace("mixamorig_", "")
			label = label.replace("Left", "L").replace("Right", "R")
			var font := get_theme_default_font()
			var font_size := 12 if selected else 10
			var label_pos := p + Vector2(12, -8)
			var text_size := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
			draw_rect(Rect2(label_pos - Vector2(3, font_size + 2), text_size + Vector2(7, font_size + 5)), Color(0.025, 0.035, 0.05, 0.82), true)
			draw_string(font, label_pos, label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, Color.WHITE)


const GeometryNodesWorkspaceScript = preload("res://geometry_nodes/geometry_nodes_workspace.gd")

@onready var tab_bar: TabBar = %TabBar
@onready var sub_viewport: SubViewport = %Viewport
@onready var vp_container: SubViewportContainer = %SubViewportContainer
@onready var editor_node : VFXEditorNode = %VFXEditorNode
@onready var camera: Camera3D = %Camera3D
@onready var uv_viewport : VFXUVEditorViewport = %UVEditorViewport
@onready var import_dialog : FileDialog = %ImportFileDialog
@onready var save_dialog : FileDialog = %SaveFileDialog
@onready var menu : MenuButton = %Menu
@onready var scene_tree : VFXSceneTreePanelGDScript = %SceneTreePanel
@onready var left_panel: Panel = %LeftPanel
@onready var right_panel: Panel = %RightPanel
@onready var bottom_panel: Panel = %BottomPanel
@onready var shading_mode: OptionButton = %ShadingMode
@onready var material_container : FoldableContainer = %MaterialFoldableCont
@onready var material_inspector: Control = _find_material_inspector()
@onready var shader_editor: CodeEdit = %ShaderEditor
@onready var undo_btn : TextureButton = %UndoBtn
@onready var redo_btn : TextureButton = %RedoBtn
@onready var delete_btn : TextureButton = %DeleteBtn

# Scene hierarchy history is separate from VFXSkin weight-stroke history.
var _scene_undo_stack: Array[Dictionary] = []
var _scene_redo_stack: Array[Dictionary] = []
const MAX_SCENE_HISTORY := 128
var _applying_scene_history := false


@onready var move_joystick: VirtualJoystickPlus = %LeftJoystick
@onready var rotate_joystick: VirtualJoystickPlus = %RightJoystick

@onready var BoneList: ItemList = %Bonelist
@onready var brush_cursor = get_node_or_null("%BrushCursor")
@onready var texture_painter: VFXTexturePainter = VFXTexturePainter.new()
var paint_texture: ImageTexture        # ONE persistent GPU texture, reused forever
var paint_material: StandardMaterial3D
var is_texture_painting: bool = false

const TEXTURE_TAB := 6                 # index of the Texture tab in TabBar
const WEIGHT_TAB := 2                  # index of the Weight Painting tab
const RIGGING_TAB := 3                 # index of the Rigging tab
const PAINT_PREVIEW_INTERVAL := 0.1    # max 10 texture uploads/sec while stroking
var pending_paint_pos := Vector2(-1, -1)
var _pending_weight_paint := Vector2(-1, -1)   # weight-paint stroke coalescing (tab 2)
var _paint_preview_timer := 0.0

# Gizmo drag feedback (damage-text style live values, see GizmoFeedbackManager.gd)
const GizmoFeedbackManagerScript = preload("res://scenes/GizmoFeedbackManager.gd")

# Retopology
const RETOPO_TAB := 7                  # index of the Retopology tab (after Texture)
var retopo: VFXRetopology
var _retopo_task_id: int = -1
var _retopo_op: String = ""
var _retopo_ok: bool = false
var _retopo_src_faces: int = 0
var _retopo_target_faces: int = 500
var _retopo_edge_length: float = 0.05
var _retopo_feature_angle: float = 45.0
var _retopo_iterations: int = 5
var _retopo_preserve_boundary: bool = true
var _retopo_preserve_features: bool = true
var retopo_progress: ProgressBar
var retopo_status: Label
var retopo_info: Label
var _retopo_buttons: Array = []

# Curve-to-mesh (mobile-first, existing VFXCurve API)
var _curve: VFXCurve
var _curve_mesh_node: VFXSceneNode
var _curve_add_mode: bool = false
var _curve_preview_enabled: bool = true
var _curve_shape: int = 0 # 0 = tube, 1 = ribbon
var _curve_profile: int = 0 # 0 round, 1 square, 2 diamond, 3 octagon
var _curve_profile_rotation: float = 0.0
# Ribbon orientation: 0 = Follow Curve, 1 = World Y-Up (floor-like), 2 = World Vertical (wall-like).
var _curve_ribbon_orientation: int = 0
var _curve_ribbon_tilt: float = 0.0
# Stage 8: per-anchor profile and tilt, taper presets, and independent tube caps.
var _curve_taper_preset: int = 0 # 0 Custom, 1 Linear, 2 Pointed, 3 Bulge, 4 Pinch
var _curve_point_profiles: Array[int] = []
var _curve_point_tilts: Array[float] = []
var _curve_start_cap: int = 0 # 0 Flat, 1 Open, 2 Pointed
var _curve_end_cap: int = 0
var _curve_profile_at_point_picker: OptionButton
var _curve_taper_picker: OptionButton
var _curve_start_cap_picker: OptionButton
var _curve_end_cap_picker: OptionButton
var _curve_ribbon_orientation_picker: OptionButton
var _curve_profile_picker: OptionButton
var _curve_radius: float = 0.08
var _curve_width: float = 0.2
var _curve_longitudinal: int = 24
var _curve_radial: int = 8
var _curve_point_radii: Array[float] = [] # per-anchor radius/width multiplier
var _curve_point_radius_slider: HSlider
var _curve_point_radius_value: Label
var _curve_status: Label
var _curve_add_button: Button
var _curve_overlay: CurveViewportOverlay
var _geometry_bezier_overlay: CurveViewportOverlay
var _geometry_bezier_curve: VFXCurve
var _geometry_bezier_node_id: String = ""
var _curve_selected_point: int = -1
var _curve_selected_handle: int = 0
var _curve_handle_modes: Array[int] = [] # 0 Free, 1 Aligned, 2 Vector, 3 Automatic
var _curve_handle_mode_picker: OptionButton
var _curve_axis_picker: OptionButton
var _curve_move_axis: int = 0 # 0 Free, 1 X, 2 Y, 3 Z
var _curve_drag_axis_origin: Vector3
var _curve_drag_axis_start: Vector3
var _curve_dragging: bool = false
var _curve_drag_plane: Plane
var _curve_drag_start: Vector3
var _curve_drag_original: Vector3
var _curve_drag_original_in: Vector3
var _curve_drag_original_out: Vector3
var _curve_endpoint_drag_start_y: float = 0.0
var _curve_endpoint_drag_original_scale: float = 1.0
var _curve_drag_preview_elapsed: float = 0.0
const CURVE_DRAG_PREVIEW_INTERVAL := 0.08


var skin: VFXSkin
var skeleton : VFXSkeleton
var humanoid_joint_overlay: HumanoidJointOverlay
var _humanoid_guide_active: bool = false
# Humanoid left/right symmetry state. Kept separate from custom rigging.
var humanoid_mirror_toggle: CheckButton
var _humanoid_mirror_enabled: bool = true
var _humanoid_mirror_bone_idx: int = -1
var _humanoid_mirror_counterpart_idx: int = -1
var _humanoid_mirror_start_transform: Transform3D = Transform3D.IDENTITY
var _humanoid_mirror_counterpart_start: Transform3D = Transform3D.IDENTITY
var _humanoid_mirror_drag_active: bool = false
var _humanoid_mirror_center_x: float = 0.0
var animator: VFXAnimator
var anim_panel: VFXAnimationPanel
var axis_gizmo: AxisGizmoOverlay
var gizmo_feedback: GizmoFeedbackManager
var _gf_slot: int = -1
var _gf_mode: int = -1
var _gf_start: Transform3D
var current_bone_idx: int = 0
var is_painting: bool = false

var cam_target := Vector3(0, 1, 0)
var cam_distance := 4.0
var zoom_distance := 4.0
var cam_yaw := 0.0
var cam_pitch := 0.3
@export var cam_move_speed := 3.0
@export var cam_orbit_speed := 2.0

var touch_positions := {}
# Press-echo dedupe: with emulate_touch_from_mouse (or emulate_mouse_from_touch)
# one physical click arrives as BOTH a ScreenTouch and a MouseButton event.
# Without a guard every press handler below fired twice per click — curve add
# used to insert two stacked points per tap, strokes began twice, etc.
var _last_primary_press_ms: int = -1
var _last_primary_press_pos: Vector2 = Vector2(-1e9, -1e9)

# Blender-style viewport context menu and last-operation settings.
const VIEWPORT_LONG_PRESS_SECONDS := 0.58
const VIEWPORT_LONG_PRESS_DRAG_TOLERANCE := 18.0
const MENU_SUBDIVIDE_SELECTED := 300
const MENU_SUBDIVIDE_MESH := 301
const MENU_MATERIAL_SETTINGS := 302
var _viewport_press_active := false
var _viewport_press_elapsed := 0.0
var _viewport_press_position := Vector2.ZERO
var _viewport_long_press_triggered := false
var _viewport_menu_position := Vector2.ZERO
var _viewport_context_menu: PopupMenu
var _mesh_primitive_menu: PopupMenu
var _material_target_node: VFXSceneNode
var _material_standard: StandardMaterial3D
var _material_texture_dialog: FileDialog
var _material_shader_dialog: FileDialog
var _material_save_dialog: FileDialog
var _material_load_dialog: FileDialog
var _material_preview_viewport: SubViewport
var _material_preview_mesh: MeshInstance3D
var _material_color_picker: ColorPickerButton
var _material_roughness_spin: SpinBox
var _material_metallic_spin: SpinBox
var _material_inspector_content: VBoxContainer
var _material_type_option: OptionButton
var _material_share_option: OptionButton
var _material_texture_target := ""
var _material_shader_texture_uniform: StringName = &""
var _material_shader_dialog_for_inspector: FileDialog
var _material_state_by_node: Dictionary = {}
var _material_inspector_building := false
const MATERIAL_MODE_PBR := 0
const MATERIAL_MODE_SHADER := 1
const MATERIAL_MODE_UNSHADED := 2
var _custom_shader_code := """shader_type spatial;
render_mode cull_disabled;

// PolyPixel custom viewport shader preview.
// Edit this code and press Apply to preview it on the model.

uniform vec4 preview_color : source_color = vec4(0.25, 0.75, 1.0, 1.0);

void vertex() {
    // VERTEX is in local space.
}

void fragment() {
    ALBEDO = preview_color.rgb;
    METALLIC = 0.0;
    ROUGHNESS = 0.65;
}
"""
var _last_operation_panel: PanelContainer
var _last_operation_kind := ""
var _last_primitive_name := ""
var _last_primitive_params: Dictionary = {}
var _last_primitive_node: VFXSceneNode
var _last_subdivide_source_mesh: VFXMesh
var _last_subdivide_node: VFXSceneNode
var _last_subdivide_levels := 1

var pinch_start_distance := 0.0
var pinch_start_zoom := 0.0
var _cam_dirty := true   # camera transform only recomputed when something moved
# Last edit mode chosen in the Modeling tab; re-applied when re-entering the tab.
var _modeling_edit_mode: String = "Object"

# -----------------------------------------------------------------
  ##TRANSFORM 3D variable of lineedit
# -----------------------------------------------------------------
@onready var pos_x_label : LineEdit = %pos_x
@onready var pos_y_label : LineEdit = %pos_y
@onready var pos_z_label : LineEdit = %pos_z

@onready var rot_x_label : LineEdit = %rot_x
@onready var rot_y_label : LineEdit = %rot_y
@onready var rot_z_label : LineEdit = %rot_z

@onready var sca_x_label : LineEdit = %sca_x
@onready var sca_y_label : LineEdit = %sca_y
@onready var sca_z_label : LineEdit = %sca_z

@onready var trans_info_parent : Control = %Transform3D #parent to hide so we hide the children when no node is selected for Transform to show

# ------------------------------------------------------------------
# READY / PROCESS
# ------------------------------------------------------------------
func _ready():
	editor_node.bone_joint_radius = 0.015
	editor_node.connect("mesh_changed", _on_mesh_changed)
	editor_node.bone_selection_radius = 0.08
	get_viewport().focus_entered.connect(func(): touch_positions.clear())
	import_dialog.file_selected.connect(_on_import_file_selected)
	menu.get_popup().id_pressed.connect(menu_option)
	editor_node.falloff_mode = VFXEditorNode.FALLOFF_CONSTANT
	# Wireframe = a second geometry pass over every edge on GLES; leave it off
	# until a mode that needs it is selected (edit modes turn it on themselves).
	editor_node.set_show_wireframe(false)

	editor_node.set_selection_mode(VFXEditorNode.SelectionMode.SELECTION_MODE_MULTI)
	editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
	editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_TRANSLATE)
	editor_node.set_camera(camera)
	editor_node.set_select_pixel_tolerance(40)
	editor_node.set_gizmo_select_pixel_tolerance(15)
	editor_node.set_texture_painter(texture_painter)  # C++ auto-syncs mesh on set_vfx_mesh
	if move_joystick:
		move_joystick.joystick_mode = VirtualJoystickPlus.JoystickMode.NORMAL
		move_joystick.visibility_mode = VirtualJoystickPlus.VisibilityMode.VISIBILITY_ALWAYS
	if rotate_joystick:
		rotate_joystick.joystick_mode = VirtualJoystickPlus.JoystickMode.NORMAL
		rotate_joystick.visibility_mode = VirtualJoystickPlus.VisibilityMode.VISIBILITY_ALWAYS

	# === CREATE DEMO CHARACTER IN DIRECT MODE ===
	#editor_node.create_demo_character()
	skin = editor_node.get_vfx_skin()
	skeleton = editor_node.get_vfx_skeleton()
	animator = editor_node.get_vfx_animator()
	var mesh : VFXMesh = editor_node.get_vfx_mesh()
	uv_viewport.set_mesh(mesh)



	# === SCENE TREE SETUP (SHARE ONE SCENE WITH THE EDITOR) ===
	# Do not create a synthetic "New Container" root. The first actual scene
	# object becomes the root; an empty scene stays genuinely empty.
	var shared_scene: VFXScene = editor_node.get_scene()
	if shared_scene == null:
		shared_scene = VFXScene.new()
		editor_node.set_scene(shared_scene)

	# In direct mode, expose the demo mesh as the actual root node so scene
	# raycasts and texture painting can still find it without a wrapper.
	if shared_scene.get_root() == null:
		var demo_mesh: VFXMesh = editor_node.get_vfx_mesh()
		if demo_mesh != null:
			var mesh_node: VFXSceneNode = shared_scene.create_node(
				"Demo",
				VFXSceneNode.NODE_MESH,
				null
			)
			if mesh_node != null:
				mesh_node.set_mesh(demo_mesh)
				_ensure_node_material(mesh_node)
				mesh_node.set_skeleton(skeleton)
				mesh_node.set_skin(skin)
				mesh_node.set_animator(animator)
				editor_node.set_active_scene_node(mesh_node)
				# set_active_scene_node() clears these — restore them.
				editor_node.set_vfx_skin(skin)
				editor_node.set_vfx_skeleton(skeleton)
				editor_node.set_vfx_animator(animator)

	scene_tree.scene = shared_scene
	# Batched multi-select: the C++ side activates the LAST node and keeps the
	# full set for object-mode multi-drag.
	scene_tree.nodes_selected.connect(_on_scene_tree_nodes_selected)
	if not scene_tree.node_visibility_changed.is_connected(_on_scene_tree_visibility_changed):
		scene_tree.node_visibility_changed.connect(_on_scene_tree_visibility_changed)
	#if not scene_tree.node_renamed.is_connected(_on_scene_node_renamed):
		#scene_tree.node_renamed.connect(_on_scene_node_renamed)
	#if not scene_tree.node_deleted.is_connected(_on_scene_node_deleted):
	#	scene_tree.node_deleted.connect(_on_scene_node_deleted)
	#if not scene_tree.node_reparented.is_connected(_on_scene_node_reparented):
		#scene_tree.node_reparented.connect(_on_scene_node_reparented)
	scene_tree.force_rebuild()

	_setup_collision()
	_setup_brush_cursor()
	_setup_persistent_ui()
	_setup_material_inspector_node()

	if not BoneList.item_selected.is_connected(_on_bone_selected):
		BoneList.item_selected.connect(_on_bone_selected)

	_setup_transform_fields()

	_on_tab_selected(tab_bar.current_tab)
	tab_bar.tab_selected.connect(_on_tab_selected)
	vp_container.gui_input.connect(_on_viewport_input)
	_setup_viewport_context_menu()

	_frame_mesh()
	_setup_axis_gizmo()

	# === GIZMO FEEDBACK (damage-text style live drag values) ===
	gizmo_feedback = GizmoFeedbackManagerScript.new()
	gizmo_feedback.feedback_font = get_theme_default_font()
	vp_container.add_child(gizmo_feedback)
	# set_camera AFTER add_child: the manager's _ready() would overwrite it
	# (its default viewport has no camera — the Camera3D lives in the SubViewport)
	gizmo_feedback.set_camera(camera)
	gizmo_feedback.set_anchors_preset(Control.PRESET_FULL_RECT)
	gizmo_feedback.mouse_filter = Control.MOUSE_FILTER_IGNORE

func _setup_axis_gizmo():
	axis_gizmo = AxisGizmoOverlay.new()
	axis_gizmo.camera = camera
	axis_gizmo.snap_to_axis.connect(_on_axis_gizmo_snap)
	axis_gizmo.orbited.connect(_on_axis_gizmo_orbit)
	vp_container.add_child(axis_gizmo)  # overlay on top of the 3D viewport
	axis_gizmo.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	axis_gizmo.position = Vector2(-axis_gizmo.size.x - -35, 60) #😂😂

	# Draw selectable, labelled joint handles above the 3D viewport. Input is
	# still routed through _on_viewport_input so gizmo drags keep their priority.
	humanoid_joint_overlay = HumanoidJointOverlay.new()
	humanoid_joint_overlay.camera_3d = camera
	humanoid_joint_overlay.viewport_size_ref = sub_viewport.size
	vp_container.add_child(humanoid_joint_overlay)
	humanoid_joint_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	humanoid_joint_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE

# ------------------------------------------------------------------
# GIZMO FEEDBACK HELPERS
# ------------------------------------------------------------------
func _gf_begin() -> void:
	_gf_end()  # defensive: never leak a stuck TRACK slot
	_begin_humanoid_mirror_drag()
	_gf_mode = editor_node.get_gizmo_mode()
	_gf_start = editor_node.get_gizmo_transform()
	_gf_slot = gizmo_feedback.begin_track(_gf_mode, _gf_anchor())
	_gf_update()  # show "X : 0 Y : 0 Z : 0" immediately

# ------------------------------------------------------------------
# HUMANOID LEFT/RIGHT SYMMETRY
# ------------------------------------------------------------------
func _humanoid_mirror_name(bone_name: String) -> String:
	if "Left" in bone_name:
		return bone_name.replace("Left", "Right")
	if "Right" in bone_name:
		return bone_name.replace("Right", "Left")
	return ""

func _begin_humanoid_mirror_drag() -> void:
	_end_humanoid_mirror_drag()
	if not _humanoid_guide_active or not _humanoid_mirror_enabled \
			or tab_bar.current_tab != RIGGING_TAB or skeleton == null \
			or current_bone_idx < 0 or current_bone_idx >= skeleton.get_bone_count():
		return
	var counterpart_name := _humanoid_mirror_name(skeleton.get_bone_name(current_bone_idx))
	if counterpart_name.is_empty():
		return # Centre-line bones (hips, spine, neck, head) have no counterpart.
	var counterpart_idx := skeleton.find_bone(counterpart_name)
	if counterpart_idx < 0:
		return
	_humanoid_mirror_bone_idx = current_bone_idx
	_humanoid_mirror_counterpart_idx = counterpart_idx
	_humanoid_mirror_start_transform = skeleton.get_bone_model_transform(current_bone_idx)
	_humanoid_mirror_counterpart_start = skeleton.get_bone_model_transform(counterpart_idx)
	_humanoid_mirror_drag_active = true

func _apply_humanoid_mirror_drag() -> void:
	if not _humanoid_mirror_drag_active or skeleton == null:
		return
	if not _humanoid_guide_active or not _humanoid_mirror_enabled \
			or current_bone_idx != _humanoid_mirror_bone_idx:
		_end_humanoid_mirror_drag()
		return
	if _humanoid_mirror_counterpart_idx < 0 \
			or _humanoid_mirror_counterpart_idx >= skeleton.get_bone_count():
		_end_humanoid_mirror_drag()
		return
	var moved: Transform3D = skeleton.get_bone_model_transform(_humanoid_mirror_bone_idx)
	var delta: Vector3 = moved.origin - _humanoid_mirror_start_transform.origin
	var mirrored_origin: Vector3 = _humanoid_mirror_counterpart_start.origin + Vector3(-delta.x, delta.y, delta.z)
	var mirrored_pose: Transform3D = _humanoid_mirror_counterpart_start
	mirrored_pose.origin = mirrored_origin
	skeleton.set_bone_model_transform(_humanoid_mirror_counterpart_idx, mirrored_pose)
	# Refresh both the skinned mesh and the labelled joint overlay during drag.
	if humanoid_joint_overlay:
		humanoid_joint_overlay.queue_redraw()

func _end_humanoid_mirror_drag() -> void:
	_humanoid_mirror_drag_active = false
	_humanoid_mirror_bone_idx = -1
	_humanoid_mirror_counterpart_idx = -1

func _gf_update() -> void:
	if _gf_slot < 0:
		return
	gizmo_feedback.update_track(_gf_slot, _gf_values(), _gf_anchor())

func _gf_end() -> void:
	_end_humanoid_mirror_drag()
	if _gf_slot < 0:
		return
	_gf_update()                      # flush exact final values
	gizmo_feedback.release(_gf_slot)   # pop animation
	_gf_slot = -1
	_refresh_transform_ui()           # final values after the drag pops

func _gf_anchor() -> Vector3:
	return editor_node.get_gizmo_transform().origin \
		+ Vector3(0, gizmo_feedback.anchor_y_offset, 0)

func _gf_values() -> Vector3:
	var t := editor_node.get_gizmo_transform()
	match _gf_mode:
		VFXEditorNode.GIZMO_ROTATE:
			var d := t.basis.get_euler() - _gf_start.basis.get_euler()
			return Vector3(rad_to_deg(d.x), rad_to_deg(d.y), rad_to_deg(d.z))
		VFXEditorNode.GIZMO_TRANSLATE:
			return t.origin - _gf_start.origin
		VFXEditorNode.GIZMO_SCALE:
			var cs := t.basis.get_scale()
			var ss := _gf_start.basis.get_scale()
			return Vector3(
				cs.x / max(ss.x, 0.0001),
				cs.y / max(ss.y, 0.0001),
				cs.z / max(ss.z, 0.0001))
	return Vector3.ZERO

# ------------------------------------------------------------------
# TRANSFORM 3D FIELDS (show / edit active node transform)
# ------------------------------------------------------------------
const POS_SUFFIX := "m"     # position: 30m
const ROT_SUFFIX := "°"     # rotation: 30°
const SCA_SUFFIX := "×"     # scale: 2× (multiplier — swap for "x" or "" if you prefer)

var _transform_fields: Array[LineEdit] = []
var _syncing_fields := false   # true while refreshing fields programmatically

# Stats labels (vert/tris filled on mesh change; FPS/draw calls at 2 Hz —
# a Label.text change inside a Container relayouts + redraws the whole bar)
const STATS_INTERVAL := 0.5
var _stats_timer := 0.0

func _setup_transform_fields() -> void:
	_transform_fields = [pos_x_label, pos_y_label, pos_z_label,
		rot_x_label, rot_y_label, rot_z_label,
		sca_x_label, sca_y_label, sca_z_label]
	for le in _transform_fields:
		if le == null:
			continue
		if not le.text_submitted.is_connected(_on_transform_field_committed):
			le.text_submitted.connect(_on_transform_field_committed)
		if not le.focus_exited.is_connected(_on_transform_field_committed):
			le.focus_exited.connect(_on_transform_field_committed)
		# Live-apply every keystroke: on touch, tapping the viewport does NOT
		# clear LineEdit focus, so commit-on-blur alone would never fire.
		if not le.text_changed.is_connected(_on_transform_field_changed):
			le.text_changed.connect(_on_transform_field_changed)
	_refresh_transform_ui()

func _fmt_unit(v: float, suffix: String) -> String:
	var r := snappedf(v, 0.01)
	if is_zero_approx(r):
		r = 0.0                       # avoid "-0"
	return str(r) + suffix

func _set_field_text(le: LineEdit, text: String) -> void:
	# Never clobber the field the user is currently typing in; skip redundant
	# sets entirely (each set emits text_changed -> transform rebuild).
	if le and not le.has_focus() and le.text != text:
		_syncing_fields = true
		le.text = text
		_syncing_fields = false

func _refresh_transform_ui() -> void:
	var node: VFXSceneNode = editor_node.get_active_scene_node()
	trans_info_parent.visible = node != null
	if node == null:
		return
	var t := node.get_local_transform()
	var rot := t.basis.get_euler()
	var scl := t.basis.get_scale()
	_set_field_text(pos_x_label, _fmt_unit(t.origin.x, POS_SUFFIX))
	_set_field_text(pos_y_label, _fmt_unit(t.origin.y, POS_SUFFIX))
	_set_field_text(pos_z_label, _fmt_unit(t.origin.z, POS_SUFFIX))
	_set_field_text(rot_x_label, _fmt_unit(rad_to_deg(rot.x), ROT_SUFFIX))
	_set_field_text(rot_y_label, _fmt_unit(rad_to_deg(rot.y), ROT_SUFFIX))
	_set_field_text(rot_z_label, _fmt_unit(rad_to_deg(rot.z), ROT_SUFFIX))
	_set_field_text(sca_x_label, _fmt_unit(scl.x, SCA_SUFFIX))
	_set_field_text(sca_y_label, _fmt_unit(scl.y, SCA_SUFFIX))
	_set_field_text(sca_z_label, _fmt_unit(scl.z, SCA_SUFFIX))

func _parse_field(le: LineEdit, fallback: float) -> float:
	var txt := le.text.strip_edges()
	if txt.is_empty():
		return fallback
	return txt.to_float()   # to_float() ignores the m / ° / × suffix automatically

func _apply_transform_from_ui() -> void:
	var node: VFXSceneNode = editor_node.get_active_scene_node()
	if node == null:
		return
	var t := node.get_local_transform()
	var pos := t.origin
	pos.x = _parse_field(pos_x_label, pos.x)
	pos.y = _parse_field(pos_y_label, pos.y)
	pos.z = _parse_field(pos_z_label, pos.z)
	var rot := t.basis.get_euler()
	rot.x = deg_to_rad(_parse_field(rot_x_label, rad_to_deg(rot.x)))
	rot.y = deg_to_rad(_parse_field(rot_y_label, rad_to_deg(rot.y)))
	rot.z = deg_to_rad(_parse_field(rot_z_label, rad_to_deg(rot.z)))
	var scl := t.basis.get_scale()
	scl.x = _parse_field(sca_x_label, scl.x)
	scl.y = _parse_field(sca_y_label, scl.y)
	scl.z = _parse_field(sca_z_label, scl.z)
	# Rebuild: rotation from euler, then scale — avoids sheared/drifted bases
	var nt := Transform3D(Basis.from_euler(rot).scaled(scl), pos)
	node.set_local_transform(nt)
	# Push the new transform to the 3D view and snap the gizmo to it
	editor_node.refresh_scene()
	editor_node.set_gizmo_transform(node.get_global_transform())

func _on_transform_field_changed(_new_text: String) -> void:
	# Typing drives the node immediately: "9" -> 9, "90" -> 90
	if _syncing_fields:
		return   # programmatic refresh (gizmo drag / selection change), not a user edit
	_apply_transform_from_ui()
	_refresh_transform_ui()

func _on_transform_field_committed(_arg = null) -> void:
	_apply_transform_from_ui()
	_refresh_transform_ui()


func _on_delete_custom_nodes_pressed() -> void:
	print("[EDITOR-DELETE] BUTTON CALLBACK ENTER")

	print("[EDITOR-DELETE] scene_tree valid: ",
		is_instance_valid(scene_tree))

	if not is_instance_valid(scene_tree):
		print("[EDITOR-DELETE] ABORT: scene_tree invalid")
		return

	print("[EDITOR-DELETE] BEFORE delete_selected_nodes()")

	scene_tree.delete_selected_nodes()

	print("[EDITOR-DELETE] AFTER delete_selected_nodes()")


func _setup_persistent_ui():
	var skel_check: CheckButton = %SkelCheck
	if skel_check and not skel_check.toggled.is_connected(_on_skeleton_visibility_toggled):
		skel_check.toggled.connect(_on_skeleton_visibility_toggled)

	var frame_btn: TextureButton = %FrameBtn
	if frame_btn and not frame_btn.pressed.is_connected(_frame_mesh):
		frame_btn.pressed.connect(_frame_mesh)

	var undo_btn: TextureButton = %UndoBtn
	if undo_btn and not undo_btn.pressed.is_connected(_on_undo_pressed):
		undo_btn.pressed.connect(_on_undo_pressed)

	var redo_btn: TextureButton = %RedoBtn
	if redo_btn and not redo_btn.pressed.is_connected(_on_redo_pressed):
		redo_btn.pressed.connect(_on_redo_pressed)

	if delete_btn and not delete_btn.pressed.is_connected(_on_delete_custom_nodes_pressed):
		delete_btn.pressed.connect(_on_delete_custom_nodes_pressed)

	var auto_btn = get_node_or_null("%AutoWeightBtn")
	if auto_btn and not auto_btn.pressed.is_connected(_on_auto_weight_pressed):
		auto_btn.pressed.connect(_on_auto_weight_pressed)

	var clear_btn = get_node_or_null("%ClearWeightsBtn")
	if clear_btn and not clear_btn.pressed.is_connected(_on_clear_weights_pressed):
		clear_btn.pressed.connect(_on_clear_weights_pressed)

	_setup_viewport_shading_controls()

func _setup_viewport_shading_controls() -> void:
	# These controls are created and laid out in the scene tree.
	# ShadingMode item IDs must match the VFXEditorNode VIEWPORT_SHADING_* constants.
	if shading_mode != null:
		if not shading_mode.item_selected.is_connected(_on_viewport_shading_selected):
			shading_mode.item_selected.connect(_on_viewport_shading_selected)

		# Preserve the existing default appearance: Textured mode.
		for i in range(shading_mode.item_count):
			if shading_mode.get_item_id(i) == VFXEditorNode.VIEWPORT_SHADING_TEXTURED:
				shading_mode.select(i)
				break
		editor_node.set_viewport_shading_mode(VFXEditorNode.VIEWPORT_SHADING_TEXTURED)

	if shader_editor != null:
		# Keep scene-authored editor content if it already has code.
		if shader_editor.text.strip_edges().is_empty():
			shader_editor.text = _custom_shader_code
		else:
			_custom_shader_code = shader_editor.text
		if not shader_editor.text_changed.is_connected(_on_shader_editor_text_changed):
			shader_editor.text_changed.connect(_on_shader_editor_text_changed)


func _on_viewport_shading_selected(index: int) -> void:
	if shading_mode == null:
		return
	var mode := shading_mode.get_item_id(index)
	if mode == VFXEditorNode.VIEWPORT_SHADING_CUSTOM_GDSHADER:
		_apply_custom_shader_code()
	editor_node.set_viewport_shading_mode(mode)
	editor_node.refresh_mesh()


func _on_shader_editor_text_changed() -> void:
	# The bottom-panel editor is the source of truth for custom preview code.
	_custom_shader_code = shader_editor.text
	if shading_mode == null or shading_mode.selected < 0:
		return
	if shading_mode.get_item_id(shading_mode.selected) == VFXEditorNode.VIEWPORT_SHADING_CUSTOM_GDSHADER:
		_apply_custom_shader_code()


func _apply_custom_shader_code() -> void:
	if shader_editor == null:
		return
	var shader := Shader.new()
	shader.code = shader_editor.text
	_custom_shader_code = shader_editor.text
	editor_node.set_viewport_custom_shader(shader)
	editor_node.refresh_mesh()


func _process(delta: float):
	if humanoid_joint_overlay:
		humanoid_joint_overlay.skeleton = skeleton
		humanoid_joint_overlay.selected_bone = current_bone_idx
		var rig_node = editor_node.get_active_scene_node()
		humanoid_joint_overlay.armature_transform = rig_node.get_global_transform() if rig_node != null else Transform3D.IDENTITY
		humanoid_joint_overlay.guide_visible = _humanoid_guide_active and tab_bar.current_tab == RIGGING_TAB
		humanoid_joint_overlay.viewport_size_ref = sub_viewport.size
		humanoid_joint_overlay.queue_redraw()
	if _viewport_press_active and (_curve_dragging or editor_node.is_gizmo_dragging() or is_painting or is_texture_painting):
		_viewport_press_active = false
	if _viewport_press_active and not _viewport_long_press_triggered:
		_viewport_press_elapsed += delta
		if _viewport_press_elapsed >= VIEWPORT_LONG_PRESS_SECONDS:
			_viewport_press_active = false
			_viewport_long_press_triggered = true
			_viewport_menu_position = _viewport_press_position
			_open_viewport_context_menu(_viewport_menu_position)

	if _curve_dragging and _curve_preview_enabled:
		_curve_drag_preview_elapsed += delta
		if _curve_drag_preview_elapsed >= CURVE_DRAG_PREVIEW_INTERVAL:
			_curve_drag_preview_elapsed = 0.0
			_curve_rebuild_preview()
	if move_joystick and move_joystick.value != Vector2.ZERO:
		var input := move_joystick.value
		var forward := Vector3(
			sin(cam_yaw) * cos(cam_pitch),
			sin(cam_pitch),
			cos(cam_yaw) * cos(cam_pitch)
		)
		var right := Vector3(cos(cam_yaw), 0.0, -sin(cam_yaw))
		cam_target += (forward * input.y + right * input.x) * cam_move_speed * delta
		_cam_dirty = true

	if rotate_joystick and rotate_joystick.value != Vector2.ZERO:
		var input := rotate_joystick.value
		cam_yaw -= input.x * cam_orbit_speed * delta
		cam_pitch += input.y * cam_orbit_speed * delta
		cam_pitch = clamp(cam_pitch, -PI / 2 + 0.05, PI / 2 - 0.05)
		_cam_dirty = true

	# Texture paint: drain at most ONE buffered point per frame
	if is_texture_painting:
		_do_texture_paint()
		_paint_preview_timer += delta
		if _paint_preview_timer >= PAINT_PREVIEW_INTERVAL:
			_paint_preview_timer = 0.0
			_flush_paint_preview()

	# Retopo job polling
	if _retopo_task_id != -1:
		if WorkerThreadPool.is_task_completed(_retopo_task_id):
			WorkerThreadPool.wait_for_task_completion(_retopo_task_id)
			_retopo_task_id = -1
			_on_retopo_finished()
		elif is_instance_valid(retopo_progress) and retopo:
			retopo_progress.value = retopo.get_progress() * 100.0

	# Weight paint: same coalescing as texture paint — one raycast, one stamp,
	# one refresh_mesh() per frame instead of one per drag event
	if is_painting and _pending_weight_paint.x >= 0:
		_paint_at(_pending_weight_paint)
		_pending_weight_paint = Vector2(-1, -1)

	# Gizmo feedback: live values while dragging, pop on release
	if _gf_slot >= 0:
		if editor_node.is_gizmo_dragging():
			_apply_humanoid_mirror_drag()
			_gf_update()
			_refresh_transform_ui()   # transform fields follow the drag live
		else:
			_gf_end()   # safety net for interrupted drags

	# Stats label at 2 Hz (see STATS_INTERVAL note above)
	_stats_timer += delta
	if _stats_timer >= STATS_INTERVAL:
		_stats_timer = 0.0
		_update_stats_label()

	# Camera: only recompute the transform when something actually moved —
	# a dirty transform every frame blocks view-dependent render caching
	if _cam_dirty or cam_distance != zoom_distance:
		cam_distance = zoom_distance
		_update_camera_transform()
		_cam_dirty = false
		if is_instance_valid(_curve_overlay):
			_curve_overlay.queue_redraw()
		if is_instance_valid(_geometry_bezier_overlay) and _geometry_bezier_overlay.visible:
			_geometry_bezier_overlay.queue_redraw()

# ------------------------------------------------------------------
# CAMERA
# ------------------------------------------------------------------
func _update_camera_transform():
	var offset := Vector3(
		sin(cam_yaw) * cos(cam_pitch),
		sin(cam_pitch),
		cos(cam_yaw) * cos(cam_pitch)
	) * cam_distance
	camera.global_position = cam_target + offset
	camera.look_at(cam_target, Vector3.UP)

func _setup_collision():
	var body := StaticBody3D.new()
	body.name = "MeshCollider"
	editor_node.add_child(body)
	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(1.0, 2.0, 1.0)
	col.shape = box
	body.add_child(col)

func _setup_brush_cursor():
	if brush_cursor:
		brush_cursor.visible = false
		var parent = brush_cursor.get_parent()
		if parent != editor_node:
			if parent:
				parent.remove_child(brush_cursor)
			editor_node.add_child(brush_cursor)
		brush_cursor.visible = true
		if brush_cursor is CanvasItem and "position" in editor_node:
			brush_cursor.position = Vector2(editor_node.position.x, editor_node.position.y)

func _frame_mesh():
	var mesh_ref := editor_node.get_vfx_mesh()
	if mesh_ref == null:
		return
	var bounds = mesh_ref.get_bounds()
	var center := Vector3((bounds[0] + bounds[3]) / 2.0, (bounds[1] + bounds[4]) / 2.0, (bounds[2] + bounds[5]) / 2.0)
	var size := Vector3(bounds[3] - bounds[0], bounds[4] - bounds[1], bounds[5] - bounds[2])
	var max_dim = max(size.x, max(size.y, size.z))
	cam_target = center
	zoom_distance = max(max_dim * 1.5, 0.5)
	cam_distance = zoom_distance
	_update_camera_transform()
	_cam_dirty = false

# ------------------------------------------------------------------
# TAB SYSTEM
# ------------------------------------------------------------------
func _on_tab_selected(idx: int):
	_clear_panels()
	# Tail-pivot Translate is specific to the Rigging workspace.
	editor_node.set_rigging_mode(idx == RIGGING_TAB)
	match idx:
		0:
			_setup_modelling()
			editor_node.set_gizmo_locked(false)
			# Re-apply the mode the buttons show instead of forcing MODE_EDGE.
			# The forced EDGE overrode _ready()'s MODE_OBJECT, so the first
			# mesh tap on app start grabbed edges/verts/faces instead of the object.
			_on_edit_mode_selected(_modeling_edit_mode)
		1:
			_setup_vfx_editor()
			editor_node.set_gizmo_locked(true)
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
			# Object-mode multi-select: taps add nodes, tap-again removes,
			# empty-space tap clears.
			editor_node.set_selection_mode(VFXEditorNode.SELECTION_MODE_MULTI)
		2:
			_setup_weight_painting()
			editor_node.set_gizmo_locked(true)
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
			# Painting taps must not keep adding to the object selection
			editor_node.set_selection_mode(VFXEditorNode.SELECTION_MODE_SINGLE)
		3:
			print("[TAB] switched to Rigging (index 3)")
			_setup_rigging()
			editor_node.set_gizmo_locked(false)
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
			editor_node.set_selection_mode(VFXEditorNode.SELECTION_MODE_MULTI)
		4:
			_setup_uv_editing()
			editor_node.set_gizmo_locked(true)
		5:
			_setup_animation()
			editor_node.set_gizmo_locked(false)
			editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_ROTATE)
			editor_node.set_selection_mode(VFXEditorNode.SELECTION_MODE_MULTI)
		6:
			_setup_texture_painting()
			editor_node.set_gizmo_locked(true)
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
			# Painting taps must not keep adding to the object selection
			editor_node.set_selection_mode(VFXEditorNode.SELECTION_MODE_SINGLE)
		7:
			_setup_retopology()
			editor_node.set_gizmo_locked(true)
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)

func _clear_panels():
	_curve_add_mode = false
	anim_panel = null
	is_painting = false
	_pending_weight_paint = Vector2(-1, -1)
	if skin:
		skin.stroke_end()
	_teardown_texture_painting()
	_teardown_retopology()
	for c in bottom_panel.get_children():
		c.queue_free()

# ------------------------------------------------------------------
# MODELLING TAB
# ------------------------------------------------------------------



func _on_modeling_tool_pressed(tool: String):
	match tool:
		"Extrude":
			editor_node.extrude_selection(0.5)
		"Inset":
			editor_node.inset_selection(0.2)
		"Delete":
			editor_node.delete_selection()
		"Subdivide":
			editor_node.subdivide_selection()
		"Bevel":
			editor_node.bevel_selection(0.1)
		"Flip":
			editor_node.flip_normals()
		"Cleanup":
			editor_node.mesh_cleanup()

# ------------------------------------------------------------------
# RIGGING TAB
# ------------------------------------------------------------------
func _setup_rigging():
	_prepare_bone_context(true)

	if not BoneList.item_activated.is_connected(_on_bonelist_item_activated):
		BoneList.item_activated.connect(_on_bonelist_item_activated)

	# Keep the existing Custom rigging workflow intact. Humanoid is an
	# additional guided workflow that creates the project's Mixamo hierarchy.
	var workflow_row := HBoxContainer.new()
	workflow_row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(workflow_row)

	var workflow_label := Label.new()
	workflow_label.text = "Rig Type"
	workflow_row.add_child(workflow_label)

	var workflow_option := OptionButton.new()
	workflow_option.add_item("Custom")
	workflow_option.add_item("Humanoid (Mixamo)")
	workflow_option.tooltip_text = "Custom keeps manual bone creation. Humanoid creates a standard Mixamo-compatible guide."
	workflow_option.select(0)
	workflow_row.add_child(workflow_option)

	var humanoid_btn := Button.new()
	humanoid_btn.visible = false
	humanoid_btn.text = "Create Humanoid Guide"
	humanoid_btn.tooltip_text = "Create a Mixamo skeleton positioned to the selected character for front-view joint placement."
	humanoid_btn.pressed.connect(_on_create_humanoid_guide_pressed)
	workflow_row.add_child(humanoid_btn)

	humanoid_mirror_toggle = CheckButton.new()
	humanoid_mirror_toggle.text = "Symmetry"
	humanoid_mirror_toggle.tooltip_text = "Mirror left/right humanoid joint movement across the character's local X centre."
	humanoid_mirror_toggle.button_pressed = _humanoid_mirror_enabled
	humanoid_mirror_toggle.visible = _humanoid_guide_active
	humanoid_mirror_toggle.toggled.connect(func(enabled: bool):
		_humanoid_mirror_enabled = enabled
	)
	workflow_row.add_child(humanoid_mirror_toggle)

	workflow_option.item_selected.connect(func(index: int):
		humanoid_btn.visible = index == 1
		humanoid_mirror_toggle.visible = index == 1 and _humanoid_guide_active
	)

	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(row)

	var rename_btn := Button.new()
	rename_btn.text = "Rename Bone"
	rename_btn.pressed.connect(_on_rename_bone_pressed)
	row.add_child(rename_btn)

	var submit_btn := Button.new()
	submit_btn.text = "Submit"
	submit_btn.tooltip_text = "Bind this skeleton to the mesh and calculate automatic bone weights"
	submit_btn.pressed.connect(_on_submit_rig_pressed)
	row.add_child(submit_btn)

# ------------------------------------------------------------------
# HUMANOID AUTO-RIGGER GUIDE
# ------------------------------------------------------------------
var _humanoid_replace_dialog: ConfirmationDialog

func _on_create_humanoid_guide_pressed() -> void:
	var active_node = editor_node.get_active_scene_node()
	var mesh: VFXMesh = editor_node.get_vfx_mesh()
	if active_node == null or mesh == null or mesh.get_vertex_count() == 0:
		push_warning("Select a character mesh before creating a humanoid rig.")
		return

	# Never silently destroy a user's manually-created/custom rig.
	if skeleton != null and skeleton.get_bone_count() > 0:
		if _humanoid_replace_dialog == null:
			_humanoid_replace_dialog = ConfirmationDialog.new()
			_humanoid_replace_dialog.title = "Replace Current Rig?"
			_humanoid_replace_dialog.dialog_text = "Creating a Humanoid guide replaces the current skeleton. Continue?"
			_humanoid_replace_dialog.confirmed.connect(_create_humanoid_guide)
			add_child(_humanoid_replace_dialog)
		_humanoid_replace_dialog.popup_centered()
		return
	_create_humanoid_guide()

func _create_humanoid_guide() -> void:
	var active_node = editor_node.get_active_scene_node()
	var mesh: VFXMesh = editor_node.get_vfx_mesh()
	if active_node == null or mesh == null or mesh.get_vertex_count() == 0:
		return

	# Calculate mesh-local bounds from the actual vertices. The built-in
	# Mixamo template is proportionally fitted to this box, then every joint
	# remains individually editable with the normal Rigging translate gizmo.
	var bounds_min: Vector3 = mesh.get_vertex_position(0)
	var bounds_max: Vector3 = bounds_min
	for i in range(1, mesh.get_vertex_count()):
		var v: Vector3 = mesh.get_vertex_position(i)
		bounds_min.x = minf(bounds_min.x, v.x)
		bounds_min.y = minf(bounds_min.y, v.y)
		bounds_min.z = minf(bounds_min.z, v.z)
		bounds_max.x = maxf(bounds_max.x, v.x)
		bounds_max.y = maxf(bounds_max.y, v.y)
		bounds_max.z = maxf(bounds_max.z, v.z)

	var size: Vector3 = bounds_max - bounds_min
	if size.y <= 0.0001:
		push_warning("The selected mesh has no usable height for humanoid fitting.")
		return

	skeleton = VFXSkeleton.new()
	skeleton.create_mixamo_skeleton()
	skeleton.update_transforms()
	var template_min_y := INF
	var template_max_y := -INF
	var template_min_x := INF
	var template_max_x := -INF
	for i in skeleton.get_bone_count():
		var p: Vector3 = skeleton.get_bone_model_transform(i).origin
		template_min_y = minf(template_min_y, p.y)
		template_max_y = maxf(template_max_y, p.y)
		template_min_x = minf(template_min_x, p.x)
		template_max_x = maxf(template_max_x, p.x)

	var height_scale: float = size.y / maxf(template_max_y - template_min_y, 0.001)
	var width_scale: float = size.x / maxf(template_max_x - template_min_x, 0.001)
	# Keep proportions mostly natural: fit height first, but prevent the arms
	# from extending far outside unusually narrow/wide meshes.
	width_scale = clampf(width_scale, height_scale * 0.55, height_scale * 1.35)
	var center := (bounds_min + bounds_max) * 0.5
	_humanoid_mirror_center_x = center.x
	for i in skeleton.get_bone_count():
		var pose: Transform3D = skeleton.get_bone_model_transform(i)
		var source: Vector3 = pose.origin
		var fitted := Vector3(
			center.x + source.x * width_scale,
			bounds_min.y + (source.y - template_min_y) * height_scale,
			center.z + source.z * height_scale
		)
		skeleton.set_bone_model_transform(i, Transform3D(pose.basis, fitted))

	skeleton.update_transforms()
	# The template ships with default bind poses. Re-capture them after fitting
	# so Submit does not skin against the template's old, unscaled positions.
	for i in skeleton.get_bone_count():
		skeleton.set_bone_bind_pose(i, skeleton.get_bone_model_transform(i))
	active_node.set_skeleton(skeleton)
	editor_node.set_vfx_skeleton(skeleton)
	_new_bones_need_bind = false
	_rigging_submit_done = false
	_humanoid_guide_active = true
	if humanoid_mirror_toggle:
		humanoid_mirror_toggle.visible = true
		humanoid_mirror_toggle.button_pressed = _humanoid_mirror_enabled
	if skin:
		skin.clear_weights()
		skin.set_skeleton(skeleton)

	# Show the character straight-on from its front (+Z), centred on the mesh.
	cam_target = center
	cam_yaw = 0.0
	cam_pitch = 0.0
	cam_distance = maxf(size.y * 2.4, 2.0)
	zoom_distance = cam_distance
	_update_camera_transform()
	_cam_dirty = false
	_after_skeleton_edit(skeleton.find_bone("mixamorig_Head"))
	print("Humanoid guide created: Mixamo hierarchy fitted to character. Adjust joints, then Submit to bind and auto-weight.")

# ------------------------------------------------------------------
# WEIGHT PAINTING
# ------------------------------------------------------------------
func _setup_weight_painting():
	_prepare_bone_context(true)

	if skin:
		var radius_slider: HSlider = %RadiusSlider
		radius_slider.min_value = 0.01
		radius_slider.max_value = 2.0
		radius_slider.step = 0.01
		radius_slider.value = skin.get_brush_radius()
		if not radius_slider.value_changed.is_connected(skin.set_brush_radius):
			radius_slider.value_changed.connect(skin.set_brush_radius)

		var strength_slider: HSlider = %StrengthSlider
		strength_slider.min_value = 0.01
		strength_slider.max_value = 5.0
		strength_slider.step = 0.01
		strength_slider.value = skin.get_brush_strength()
		if not strength_slider.value_changed.is_connected(skin.set_brush_strength):
			strength_slider.value_changed.connect(skin.set_brush_strength)

		var falloff_opt: OptionButton = %FalloffOption
		if falloff_opt.item_count == 0:
			falloff_opt.add_item("Linear")
			falloff_opt.add_item("Smooth")
			falloff_opt.add_item("Spherical")
		falloff_opt.selected = skin.get_brush_falloff()
		if not falloff_opt.item_selected.is_connected(skin.set_brush_falloff):
			falloff_opt.item_selected.connect(skin.set_brush_falloff)

		var mode_opt: OptionButton = %ModeOption
		if mode_opt.item_count == 0:
			mode_opt.add_item("Add")
			mode_opt.add_item("Subtract")
			mode_opt.add_item("Replace")
			mode_opt.add_item("Smooth")
			mode_opt.add_item("Blur")
		mode_opt.selected = skin.get_brush_mode()
		if not mode_opt.item_selected.is_connected(skin.set_brush_mode):
			mode_opt.item_selected.connect(skin.set_brush_mode)

		var norm_check: CheckButton = %NormCheck
		norm_check.button_pressed = skin.get_brush_normalize()
		if not norm_check.toggled.is_connected(skin.set_brush_normalize):
			norm_check.toggled.connect(skin.set_brush_normalize)

		var mirror_check: CheckButton = %MirrorCheck
		mirror_check.button_pressed = skin.get_mirror_x()
		if not mirror_check.toggled.is_connected(skin.set_mirror_x):
			mirror_check.toggled.connect(skin.set_mirror_x)

	var bottom_row := HBoxContainer.new()
	bottom_row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(bottom_row)

	var shade_group := ButtonGroup.new()
	for mode in ["Solid", "Wire", "Normal", "Weight"]:
		var btn := Button.new()
		btn.text = mode
		btn.toggle_mode = true
		btn.button_group = shade_group
		btn.pressed.connect(_on_shade_selected.bind(mode))
		bottom_row.add_child(btn)
		if mode == "Weight":
			btn.button_pressed = true

	var focus_bone_btn := Button.new()
	focus_bone_btn.text = "Focus Bone"
	focus_bone_btn.pressed.connect(_focus_selected_bone)
	bottom_row.add_child(focus_bone_btn)

	var mesh_ref = editor_node.get_vfx_mesh()
	var vis_label = get_node_or_null("%VisLabel")
	if vis_label and skeleton and skeleton.get_bone_count() > 0:
		vis_label.text = "Visualize: " + skeleton.get_bone_name(current_bone_idx)


# ------------------------------------------------------------------
# BONE
# ------------------------------------------------------------------
func _on_bone_selected(index: int) -> void:
	if not skeleton:
		return
	current_bone_idx = index
	BoneList.select(index, true)
	editor_node.set_selected_bone(index)
	if skin:
		skin.set_brush_bone(index)
	editor_node.set_visualize_bone(index)
	editor_node.refresh_mesh()
	var vis_label = get_node_or_null("%VisLabel")
	if vis_label:
		vis_label.text = "Visualize: " + skeleton.get_bone_name(index)

func _focus_selected_bone():
	if not skeleton or current_bone_idx < 0:
		return
	var bone_pos = skeleton.get_bone_model_transform(current_bone_idx).origin
	cam_target = bone_pos
	_update_camera_transform()
	_cam_dirty = false

# ------------------------------------------------------------------
# RIGGING BONE TABLE (Prisma3D-style non-auto rigger)
# ------------------------------------------------------------------
var rename_dialog: ConfirmationDialog
var rename_edit: LineEdit
var _rename_bone_idx: int = -1
var _new_bones_need_bind: bool = false
var _rigging_submit_done: bool = false

# Bones created by the rigging table have identity bind poses; capture the
# current model transforms as bind poses the first time weights are built,
# otherwise skinning (model * inverse_bind) drags verts to the origin.
func _capture_bind_pose_if_needed() -> void:
	if not _new_bones_need_bind or skeleton == null:
		return
	skeleton.update_transforms()
	for i in skeleton.get_bone_count():
		var bind: Transform3D = skeleton.get_bone_bind_pose(i)
		if bind == Transform3D.IDENTITY:
			skeleton.set_bone_bind_pose(i, skeleton.get_bone_model_transform(i))
	_new_bones_need_bind = false

func _unique_bone_name(base: String) -> String:
	var name := base
	var n := 1
	while skeleton and skeleton.find_bone(name) >= 0:
		name = "%s%03d" % [base, n]
		n += 1
	return name

func _on_add_bone_pressed():
	# No skeleton yet -> start a fresh rig on the active node
	if skeleton == null:
		skeleton = VFXSkeleton.new()
		var node := editor_node.get_active_scene_node()
		if node:
			node.set_skeleton(skeleton)
		editor_node.set_vfx_skeleton(skeleton)

	var parent_id := -1
	if skeleton.get_bone_count() > 0 and current_bone_idx >= 0 \
			and current_bone_idx < skeleton.get_bone_count():
		parent_id = current_bone_idx

	var new_id: int = skeleton.add_bone(_unique_bone_name("Bone"), parent_id)
	if new_id < 0:
		return
	_new_bones_need_bind = true   # capture bind pose when skinning starts
	_rigging_submit_done = false
	_after_skeleton_edit(new_id)

# Prisma3D-style placement: a tap on the mesh creates a joint at the hit point.
# The newly-created bone becomes selected, so the existing transform gizmo
# immediately appears. Its parent is the previously selected bone.
func _add_bone_at_mesh_hit(ray_origin: Vector3, ray_dir: Vector3) -> bool:
	var active_node = editor_node.get_active_scene_node()
	print("[RIG-ADD] active_node=", active_node)
	if active_node == null:
		return false

	var mesh: VFXMesh = editor_node.get_vfx_mesh()
	print("[RIG-ADD] mesh=", mesh, " verts=", mesh.get_vertex_count() if mesh else 0)
	if mesh == null:
		return false

	# VFXEditorNode.raycast_mesh() expects a WORLD-space ray. Its native
	# implementation transforms that ray into the editor node's mesh space.
	# Do not pre-transform through the scene node; that caused the ray to miss.
	var hit = editor_node.raycast_mesh(ray_origin, ray_dir, 100000.0)
	print("[RIG-ADD] raycast hit=", hit, " is_Vector3=", hit is Vector3)
	if not (hit is Vector3):
		return false

	if skeleton == null:
		skeleton = VFXSkeleton.new()
		active_node.set_skeleton(skeleton)
		editor_node.set_vfx_skeleton(skeleton)

	var parent_id := -1
	if skeleton.get_bone_count() > 0 and current_bone_idx >= 0 \
			and current_bone_idx < skeleton.get_bone_count():
		parent_id = current_bone_idx

	# Flush the selected parent's latest pose before adding a child. The native
	# setter also refreshes transforms, but doing it here makes the intended
	# model-space placement explicit and keeps all rigging paths consistent.
	skeleton.update_transforms()
	var new_id := skeleton.add_bone(_unique_bone_name("Bone"), parent_id)
	if new_id < 0:
		return false

	# Place the joint halfway through the mesh thickness along the tapped
	# screen ray, rather than on the front surface. This matches the centered
	# joint placement used by Prisma3D while preserving the tapped screen X/Y.
	# Both raycasts return the same mesh-local/model coordinate space.
	var far_origin: Vector3 = ray_origin + ray_dir.normalized() * 100000.0
	var back_hit = editor_node.raycast_mesh(far_origin, -ray_dir.normalized(), 100000.0)
	var placement: Vector3 = hit
	if back_hit is Vector3:
		placement = (hit + back_hit) * 0.5
	skeleton.set_bone_model_transform(new_id, Transform3D(Basis.IDENTITY, placement))
	skeleton.update_transforms()
	_new_bones_need_bind = true
	_rigging_submit_done = false
	_after_skeleton_edit(new_id)
	return true

func _on_submit_rig_pressed() -> void:
	var active_node = editor_node.get_active_scene_node()
	var mesh: VFXMesh = editor_node.get_vfx_mesh()
	if active_node == null or mesh == null:
		push_warning("Select a mesh before submitting the rig.")
		return
	if skeleton == null or skeleton.get_bone_count() == 0:
		push_warning("Add at least one bone before submitting the rig.")
		return

	# Attach the skeleton and a skin resource to this scene node, then build
	# automatic weights from the final placed bone positions.
	active_node.set_skeleton(skeleton)
	editor_node.set_vfx_skeleton(skeleton)
	if skin == null:
		skin = VFXSkin.new()
	if skin:
		skin.set_mesh(mesh)
		skin.set_skeleton(skeleton)
		if active_node.has_method("get_global_transform"):
			skin.set_mesh_transform(active_node.get_global_transform())
		active_node.set_skin(skin)
		editor_node.set_vfx_skin(skin)

	# Submitting (including a re-submit after moving joints) establishes the
	# current placement as the new rest pose before recalculating skin weights.
	skeleton.update_transforms()
	for bone_idx in skeleton.get_bone_count():
		skeleton.set_bone_bind_pose(bone_idx, skeleton.get_bone_model_transform(bone_idx))
	_new_bones_need_bind = false
	if skin:
		skin.auto_weight_from_bones(4)
		skin.set_brush_bone(maxi(current_bone_idx, 0))
		_rigging_submit_done = true
		editor_node.refresh_mesh()
		print("Rig submitted: ", skeleton.get_bone_count(), " bones bound and auto-weighted.")

func _on_remove_bone_pressed():
	if not skeleton or current_bone_idx < 0 \
			or current_bone_idx >= skeleton.get_bone_count():
		return
	var removed := current_bone_idx
	if not skeleton.remove_bone(removed, true):
		return

	# Bone indices shifted -> old weights point at wrong bones. Same remedy
	# as retopo: clear and re-auto-weight if we still can.
	if skin:
		skin.clear_weights()
		var m := editor_node.get_vfx_mesh()
		if m != null and m.get_vertex_count() > 0 and skeleton.get_bone_count() > 0:
			_capture_bind_pose_if_needed()
			skin.auto_weight_from_bones(4)

	_after_skeleton_edit(min(removed, skeleton.get_bone_count() - 1))

func _on_rename_bone_pressed():
	if not skeleton or current_bone_idx < 0 \
			or current_bone_idx >= skeleton.get_bone_count():
		return
	_ensure_rename_dialog()
	_rename_bone_idx = current_bone_idx
	rename_edit.text = skeleton.get_bone_name(_rename_bone_idx)
	rename_dialog.popup_centered()
	rename_edit.select_all()
	rename_edit.grab_focus()

func _on_bonelist_item_activated(index: int) -> void:
	current_bone_idx = index
	_on_rename_bone_pressed()

func _ensure_rename_dialog() -> void:
	if is_instance_valid(rename_dialog):
		return
	rename_dialog = ConfirmationDialog.new()
	rename_dialog.title = "Rename Bone"
	rename_edit = LineEdit.new()
	rename_edit.custom_minimum_size = Vector2(240, 0)
	rename_edit.placeholder_text = "Bone name"
	rename_dialog.add_child(rename_edit)
	rename_dialog.register_text_enter(rename_edit)   # Enter confirms too
	rename_dialog.confirmed.connect(_on_rename_confirmed)
	add_child(rename_dialog)

func _on_rename_confirmed() -> void:
	if not skeleton or _rename_bone_idx < 0 \
			or _rename_bone_idx >= skeleton.get_bone_count():
		return
	var new_name := rename_edit.text.strip_edges()
	if new_name.is_empty():
		return
	if not skeleton.rename_bone(_rename_bone_idx, new_name):
		push_warning("Rename failed: name already in use")
		return
	_after_skeleton_edit(_rename_bone_idx)

func _after_skeleton_edit(select_idx: int) -> void:
	if skeleton == null or skeleton.get_bone_count() == 0:
		BoneList.clear()
		current_bone_idx = -1
		_humanoid_guide_active = false
		editor_node.set_selected_bone(-1)
		editor_node.set_visualize_bone(-1)
		editor_node.refresh_mesh()
		return
	select_idx = clampi(select_idx, 0, skeleton.get_bone_count() - 1)
	_prepare_bone_context(false)
	BoneList.select(select_idx)
	_on_bone_selected(select_idx)
	editor_node.refresh_mesh()

# ------------------------------------------------------------------
# WEIGHT PAINTING HELPERS
# ------------------------------------------------------------------
func _paint_at(screen_pos: Vector2):
	var from := camera.project_ray_origin(screen_pos)
	var to := from + camera.project_ray_normal(screen_pos) * 100.0
	var space := editor_node.get_world_3d().direct_space_state
	var query := PhysicsRayQueryParameters3D.new()
	query.from = from
	query.to = to
	query.collide_with_bodies = true
	query.collision_mask = 1
	var result := space.intersect_ray(query)

	if result:
		if brush_cursor:
			brush_cursor.visible = true
			if brush_cursor is Node3D:
				brush_cursor.global_position = result.position
				if skin:
					var radius = skin.get_brush_radius()
					brush_cursor.scale = Vector3(radius, radius, radius)
			elif brush_cursor is CanvasItem:
				brush_cursor.global_position = camera.unproject_position(result.position)
				if skin:
					var radius = skin.get_brush_radius()
					brush_cursor.scale = Vector2(radius, radius)

		if skin:
			skin.paint_at(result.position, get_process_delta_time())
			editor_node.refresh_mesh()
	else:
		if brush_cursor:
			brush_cursor.visible = false

func _on_skeleton_visibility_toggled(v: bool):
	editor_node.set_show_skeleton(v)

func _push_scene_history(action: Dictionary) -> void:
	if _applying_scene_history:
		return
	_scene_undo_stack.append(action)
	if _scene_undo_stack.size() > MAX_SCENE_HISTORY:
		_scene_undo_stack.pop_front()
	_scene_redo_stack.clear()

func _on_scene_node_renamed(node: VFXSceneNode, old_name: String, new_name: String) -> void:
	_push_scene_history({"type": "rename", "node": node, "old": old_name, "new": new_name})

func _on_scene_node_deleted(node: VFXSceneNode, old_parent: VFXSceneNode) -> void:
	_push_scene_history({"type": "delete", "node": node, "parent": old_parent})

func _on_scene_node_reparented(node: VFXSceneNode, old_parent: VFXSceneNode, new_parent: VFXSceneNode) -> void:
	_push_scene_history({"type": "reparent", "node": node, "old_parent": old_parent, "new_parent": new_parent})

func _apply_scene_history(action: Dictionary, undoing: bool) -> void:
	var kind: String = action.get("type", "")
	var node: VFXSceneNode = action.get("node")
	if node == null:
		return
	_applying_scene_history = true
	match kind:
		"rename":
			node.node_name = action.old if undoing else action.new
		"delete":
			var parent: VFXSceneNode = action.parent
			if undoing:
				if parent != null and not parent.has_child(node):
					parent.add_child(node)
			else:
				var current_parent: VFXSceneNode = node.get_parent_ref()
				if current_parent != null:
					current_parent.remove_child(node)
		"reparent":
			var destination: VFXSceneNode = action.old_parent if undoing else action.new_parent
			if destination != null:
				var current_parent: VFXSceneNode = node.get_parent_ref()
				var world: Transform3D = node.get_global_transform()
				if current_parent != null:
					current_parent.remove_child(node)
				destination.add_child(node)
				node.set_local_transform(destination.get_global_transform().affine_inverse() * world)
	_applying_scene_history = false
	if scene_tree != null:
		scene_tree.force_rebuild()
	if editor_node != null:
		editor_node.refresh_scene()

func _on_undo_pressed():
	if not _scene_undo_stack.is_empty():
		var action: Dictionary = _scene_undo_stack.pop_back()
		_apply_scene_history(action, true)
		_scene_redo_stack.append(action)
	elif skin and skin.can_undo():
		skin.undo()
		editor_node.refresh_mesh()

func _on_redo_pressed():
	if not _scene_redo_stack.is_empty():
		var action: Dictionary = _scene_redo_stack.pop_back()
		_apply_scene_history(action, false)
		_scene_undo_stack.append(action)
	elif skin and skin.can_redo():
		skin.redo()
		editor_node.refresh_mesh()

func _on_auto_weight_pressed():
	if skin:
		_capture_bind_pose_if_needed()
		skin.auto_weight_from_bones(4)
		editor_node.refresh_mesh()

func _on_clear_weights_pressed():
	if skin:
		skin.clear_weights()
		editor_node.refresh_mesh()

# ------------------------------------------------------------------
# SHADING
# ------------------------------------------------------------------
func _on_shade_selected(mode: String):
	match mode:
		"Solid":
			editor_node.set_show_weights(false)
		"Wire":
			editor_node.set_show_weights(false)
		"Normal":
			editor_node.set_show_weights(false)
		"Weight":
			editor_node.set_show_weights(true)
			editor_node.set_visualize_bone(current_bone_idx)
	editor_node.refresh_mesh()

# ------------------------------------------------------------------
# IMPORT / EXPORT
# ------------------------------------------------------------------
func _on_import_pressed():
	var user_path := "res://scorp.glb"
	
	# If there's no file in user:// yet, copy the default one from res:// there first
	if not FileAccess.file_exists(user_path):
		var src := FileAccess.get_file_as_bytes("res://minion.glb")
		if src.is_empty():
			push_error("minion.glb not found in res://")
			return
		var dst := FileAccess.open(user_path, FileAccess.WRITE)
		dst.store_buffer(src)
		dst.close()
		print("Copied default to: ", OS.get_user_data_dir())
	
	_on_import_file_selected(user_path)

func _on_import_file_selected(path: String):
	var importer := VFXGLBImporter.new()
	var result = importer.import_glb(path, true)

	if not result.success:
		push_error("GLB import failed: " + result.error)
		return

	# === CLEAR OLD DEMO DATA FIRST ===
	skeleton = null
	skin = null
	animator = null
	editor_node.clear_vfx_animation_context()
	editor_node.set_vfx_skeleton(null)
	editor_node.set_vfx_skin(null)
	editor_node.set_vfx_animator(null)
	BoneList.clear()

	# Use the full scene from the importer
	var imported_scene : VFXScene = result.scene
	var imported_root : VFXSceneNode = imported_scene.get_root()
	
	# Check if we already have a scene
	# Check if we already have a scene
	var existing_scene : VFXScene = editor_node.get_scene()
	if existing_scene != null and existing_scene.get_root() != null:
		var existing_root : VFXSceneNode = existing_scene.get_root()
		while imported_root.get_child_count() > 0:
			var child : VFXSceneNode = imported_root.get_child(0)
			imported_root.remove_child(child)
			existing_root.add_child(child)
		scene_tree.scene = existing_scene
		editor_node.set_scene(existing_scene)  # <-- ADD THIS: forces immediate visual sync
	else:
		editor_node.set_scene(imported_scene)
		scene_tree.scene = imported_scene
	scene_tree.force_rebuild()
	

	# DEBUG: print tree hierarchy
	#print("=== IMPORTED TREE ===")
	_print_tree_recursive(imported_scene.get_root(), 0)

	# Activate the first mesh node
	var mesh_nodes : Array = result.mesh_nodes
	#print("Mesh nodes count: ", mesh_nodes.size())
	for i in range(mesh_nodes.size()):
		var n : VFXSceneNode = mesh_nodes[i]
		if n.has_mesh():
			_ensure_node_material(n)
		print("  [", i, "] ", n.get_node_name(), " mesh=", n.has_mesh())

	if mesh_nodes.size() > 0:
		var first_node : VFXSceneNode = mesh_nodes[0]
		if first_node.has_mesh():
			var m := first_node.get_mesh()
			if m != null and m.get_vertex_count() > 0:
				editor_node.set_vfx_mesh(m)
				# FIX: use the node's real global transform (imported GLB nodes
				# often carry rotations, e.g. Blender's +90° X) — IDENTITY makes
				# the painter raycast in the wrong space and nothing gets stamped.
				texture_painter.set_mesh_transform(first_node.get_global_transform())
				uv_viewport.set_mesh(m)

				# DEBUG: verify the importer kept the UVs. If uvs.size() != verts,
				# every stamp lands at UV(0,0) and the model looks unpaintable.
				var uvs := m.get_uvs()
				print("IMPORT: verts=", m.get_vertex_count(), " uvs=", uvs.size())
				if uvs.size() >= 3:
					print("  uv[0..2]=", uvs[0], " ", uvs[1], " ", uvs[2])
			else:
				editor_node.set_vfx_mesh(null)
		if first_node.has_skeleton():
			skeleton = first_node.get_skeleton()
			editor_node.set_vfx_skeleton(skeleton)
		if first_node.has_skin():
			skin = first_node.get_skin()
			editor_node.set_vfx_skin(skin)
		if first_node.has_animator():
			animator = first_node.get_animator()
			editor_node.set_vfx_animator(animator)
		if not first_node.has_animator():
			var a := VFXAnimator.new()
			first_node.set_animator(a)
			animator = first_node.get_animator()
			editor_node.set_vfx_animator(animator)


	_frame_mesh()
	editor_node.refresh_mesh()

	_prepare_bone_context(true)

	if is_instance_valid(anim_panel):
		anim_panel.sync_from_editor()

func _print_tree_recursive(node: VFXSceneNode, depth: int):
	if node == null:
		return
	var indent := ""
	for i in range(depth):
		indent += "  "
	#print(indent, node.get_node_name(), " [type=", node.get_node_type(), " mesh=", node.has_mesh(), " children=", node.get_child_count(), "]")
	for i in range(node.get_child_count()):
		_print_tree_recursive(node.get_child(i), depth + 1)


func _route_save_dialog(handler: Callable, filter: String, filter_name: String, title: String, file_name: String):
	for conn in save_dialog.file_selected.get_connections():
		save_dialog.file_selected.disconnect(conn.callable)
	save_dialog.file_mode = FileDialog.FILE_MODE_SAVE_FILE
	save_dialog.title = title
	save_dialog.current_file = file_name
	save_dialog.clear_filters()
	save_dialog.add_filter(filter, filter_name)
	save_dialog.file_selected.connect(handler)
	save_dialog.popup_centered(Vector2(800, 600))

func _on_export_pressed():
	_route_save_dialog(_on_export_file_selected, "*.glb", "GLB Binary", "Export GLB", "model.glb")

func _on_export_file_selected(path: String):
	var ok := editor_node.export_glb(path)
	if not ok:
		push_error("GLB export failed: " + path)

# ------------------------------------------------------------------
# OTHER TABS
# ------------------------------------------------------------------
func _setup_vfx_editor():
	pass

func _setup_uv_editing():
	pass

func _setup_animation():
	# Important: animation must show the deformed mesh, not weight colors.
	editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
	editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_ROTATE)
	editor_node.set_show_skeleton(true)
	editor_node.set_show_weights(false)

	_prepare_bone_context(true)
	editor_node.refresh_mesh()

	anim_panel = VFXAnimationPanel.new()
	anim_panel.set_anchors_preset(PRESET_FULL_RECT)
	bottom_panel.add_child(anim_panel)
	anim_panel.setup(
		editor_node,
		_get_current_skeleton,
		_get_current_animator,
		_get_active_scene_node
	)

func _get_current_skeleton() -> VFXSkeleton:
	return skeleton


func _get_current_animator() -> VFXAnimator:
	return animator


func _get_active_scene_node() -> VFXSceneNode:
	return editor_node.get_active_scene_node()

func _on_rotate_btn_pressed() -> void:
	editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_ROTATE)
	%RotateBtn.self_modulate = Color.CYAN
	%MoveBtn.self_modulate = Color.WHITE
	%ScaleBtn.self_modulate = Color.WHITE
	
func _on_move_btn_pressed() -> void:
	editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_TRANSLATE)
	%RotateBtn.self_modulate = Color.WHITE
	%MoveBtn.self_modulate = Color.CYAN
	%ScaleBtn.self_modulate = Color.WHITE
	
func _on_scale_btn_pressed() -> void:
	editor_node.set_gizmo_mode(VFXEditorNode.GIZMO_SCALE)
	%RotateBtn.self_modulate = Color.WHITE
	%MoveBtn.self_modulate = Color.WHITE
	%ScaleBtn.self_modulate = Color.CYAN

# ------------------------------------------------------------------
# VIEWPORT INPUT
# ------------------------------------------------------------------
func _viewport_event_pos(event_pos: Vector2) -> Vector2:
	var container_size := Vector2(vp_container.size)
	var viewport_size := Vector2(sub_viewport.size)
	if container_size.x == 0 or container_size.y == 0:
		return event_pos
	return event_pos * (viewport_size / container_size)

func _on_viewport_input(event: InputEvent):
	# === DEFENSIVE: clear stale touches ===
	if event is InputEventScreenTouch and event.pressed:

		# If this index already exists, a previous release was missed.
		# Clear it so we don't falsely enter pinch mode.
		if touch_positions.has(event.index):
			touch_positions.erase(event.index)
		# If we somehow accumulated more than 2 entries, wipe them.
		if touch_positions.size() >= 2:
			touch_positions.clear()
	var ray_origin := Vector3()
	var ray_dir := Vector3()
	var event_pos := Vector2(-1, -1)

	if event is InputEventScreenDrag:
		event_pos = _viewport_event_pos(event.position)
	elif event is InputEventMouseMotion:
		event_pos = _viewport_event_pos(event.position)
	elif event is InputEventScreenTouch:
		event_pos = _viewport_event_pos(event.position)
	elif event is InputEventMouseButton:
		event_pos = _viewport_event_pos(event.position)

	if event_pos.x >= 0:
		ray_origin = camera.project_ray_origin(event_pos)
		ray_dir = camera.project_ray_normal(event_pos)

	# === DEFENSIVE: swallow the touch/mouse echo of one physical press ===
	# A single click can be delivered twice (ScreenTouch + emulated MouseButton,
	# or MouseButton + emulated touch). Only primary presses are deduped; the
	# 120 ms / 8 px window still lets a genuine fast double-click in a
	# different spot through. Non-primary touches (pinch 2nd finger) are
	# untouched.
	var is_primary_press = (event is InputEventScreenTouch and event.pressed and event.index == 0) or \
		(event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT)
	if is_primary_press:
		var press_now := Time.get_ticks_msec()
		if press_now - _last_primary_press_ms < 120 and event_pos.distance_to(_last_primary_press_pos) < 8.0:
			return
		_last_primary_press_ms = press_now
		_last_primary_press_pos = event_pos

	# Right-click opens the same context menu immediately on desktop.
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
		if tab_bar.current_tab == 0 and not _curve_add_mode and not _curve_dragging:
			_viewport_press_active = false
			_viewport_long_press_triggered = false
			_viewport_menu_position = event.position
			_open_viewport_context_menu(_viewport_menu_position)
			return

	# A primary press starts a long-press candidate. Curve point placement,
	# active curve drags, painting tabs, and multi-touch gestures are excluded.
	if is_primary_press:
		_viewport_press_active = tab_bar.current_tab == 0 and not _curve_add_mode and not _curve_dragging
		_viewport_press_elapsed = 0.0
		_viewport_press_position = event.position
		_viewport_long_press_triggered = false

	# Movement beyond the tolerance is a drag, not a long press.
	if _viewport_press_active and (event is InputEventScreenDrag or event is InputEventMouseMotion):
		if event.position.distance_to(_viewport_press_position) > VIEWPORT_LONG_PRESS_DRAG_TOLERANCE:
			_viewport_press_active = false

	# Consume the release after a long press so it cannot also select/paint.
	var is_primary_release = (event is InputEventScreenTouch and not event.pressed and event.index == 0) or \
		(event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT)
	if is_primary_release:
		if _viewport_long_press_triggered:
			_viewport_long_press_triggered = false
			_viewport_press_active = false
			if event is InputEventScreenTouch:
				touch_positions.erase(event.index)
			return
		_viewport_press_active = false
		_viewport_press_elapsed = 0.0

	# A second finger cancels a pending long press and starts the normal pinch path.
	if event is InputEventScreenTouch and event.pressed and event.index != 0:
		_viewport_press_active = false

	# Curve editing takes priority over mesh selection in the Modelling tab.
	# Add mode must work even when the curve is empty (first control point).
	# Editing mode only needs the overlay once at least one point exists.
	if tab_bar.current_tab == 0 and _curve != null and (_curve_add_mode or _curve.get_point_count() > 0):
		if (event is InputEventScreenDrag or event is InputEventMouseMotion) and _curve_dragging:
			_curve_update_edit(ray_origin, ray_dir, event_pos)
			return
		if event is InputEventScreenTouch and event.pressed and event.index == 0:
			if _curve_add_mode:
				_curve_add_point_from_ray(ray_origin, ray_dir)
				return
			if _curve_begin_edit(event_pos, ray_origin, ray_dir):
				_viewport_press_active = false
				return
		elif event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
			if _curve_add_mode:
				_curve_add_point_from_ray(ray_origin, ray_dir)
				return
			if _curve_begin_edit(event_pos, ray_origin, ray_dir):
				_viewport_press_active = false
				return
		if (event is InputEventScreenTouch and not event.pressed) or \
			(event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT):
			_curve_end_edit()
			if _curve_add_mode or _curve_selected_point >= 0:
				return
	if _curve_add_mode and tab_bar.current_tab == 0:
		return

	# Active gizmo drag
	if editor_node.is_gizmo_dragging():
		if event is InputEventScreenDrag or event is InputEventMouseMotion:
			editor_node.on_touch_drag(ray_origin, ray_dir)
			return
		elif (event is InputEventScreenTouch and not event.pressed) or \
			(event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT):
			editor_node.on_touch_up()
			_gf_end()
			return

	# Touch / mouse down
	if (event is InputEventScreenTouch and event.pressed) or \
		(event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT):

		if event is InputEventScreenTouch:
			touch_positions[event.index] = event.position
			if touch_positions.size() == 2:
				if editor_node.is_gizmo_dragging():
					editor_node.on_touch_up()
					_gf_end()
				is_painting = false
				is_texture_painting = false
				_pending_weight_paint = Vector2(-1, -1)
				pending_paint_pos = Vector2(-1, -1)
				if skin:
					skin.stroke_end()
				texture_painter.stroke_end()
				var keys = touch_positions.keys()
				pinch_start_distance = touch_positions[keys[0]].distance_to(touch_positions[keys[1]])
				pinch_start_zoom = zoom_distance
				return

		# Rigging has its own hit-test priority. The generic on_touch_down()
		# selects a scene node when it hits the mesh, so calling it first makes
		# the mesh-selection path consume the tap before bone creation can run.
		if tab_bar.current_tab == RIGGING_TAB:
			# Let an existing transform gizmo win first, so the selected bone can
			# still be moved without accidentally creating another bone.
			var gizmo_axis := editor_node.screen_raycast_gizmo(event_pos)
			print("[RIG] gizmo_axis=", gizmo_axis)
			if gizmo_axis != -1:
				var gizmo_result := editor_node.on_touch_down(ray_origin, ray_dir, event_pos)
				print("[RIG] gizmo_result=", gizmo_result)
				if gizmo_result == -2:
					_gf_begin()
					return

			# The guided humanoid's labelled screen-space joints are easier to hit
			# than thin bone lines, especially on touch screens. Gizmo hit-testing
			# above intentionally wins so an active axis remains draggable.
			if _humanoid_guide_active and humanoid_joint_overlay:
				var joint_hit_pos := event_pos * (Vector2(vp_container.size) / Vector2(sub_viewport.size))
				var joint_hit := humanoid_joint_overlay.hit_test(joint_hit_pos)
				if joint_hit >= 0:
					_on_bone_selected(joint_hit)
					return

			# Existing bones are selectable independently of the mesh surface.
			var bone_hit: int = editor_node.raycast_bone(ray_origin, ray_dir)
			print("[RIG] bone_hit=", bone_hit)
			if bone_hit >= 0:
				current_bone_idx = bone_hit
				_on_bone_selected(bone_hit)
				return

			# A mesh hit adds a bone; do not pass that same tap to object selection.
			var added := _add_bone_at_mesh_hit(ray_origin, ray_dir)
			print("[RIG] _add_bone_at_mesh_hit returned ", added)
			if added:
				return
			# In rigging mode, taps that miss everything should do NOTHING —
			# never fall through to object/vertex/edge selection.
			print("[RIG] tap missed gizmo/bone/mesh — swallowing, no object selection")
			return

		# Non-rigging tabs retain the normal editor selection/gizmo behavior.
		var result := editor_node.on_touch_down(ray_origin, ray_dir, event_pos)
		if result == -2:
			_gf_begin()
			return
		elif result >= 0:
			current_bone_idx = result
			_on_bone_selected(result)
			return

		# The native viewport picker updates VFXEditorNode's selection directly;
		# it does not emit the Scene Tree panel's nodes_selected signal. Mirror
		# that selection here so the tree highlight and GDScript inspectors stay
		# in sync with the object selected in the viewport. A -1 result with no
		# native selection means the user clicked empty space and deselected.
		if result == VFXEditorNode.SCENE_NODE_HIT or (result == -1 and editor_node.get_selected_scene_nodes().is_empty()):
			scene_tree.sync_selection_from_nodes(
				editor_node.get_selected_scene_nodes(),
				editor_node.get_active_scene_node()
			)

		var is_primary = (event is InputEventMouseButton) or (event is InputEventScreenTouch and event.index == 0)
		if tab_bar.current_tab == WEIGHT_TAB and is_primary:
			is_painting = true
			if skin:
				skin.stroke_begin()
			_paint_at(event_pos)
			return
		if tab_bar.current_tab == TEXTURE_TAB and is_primary:
			is_texture_painting = true
			_paint_preview_timer = 0.0
			texture_painter.stroke_begin()
			_queue_texture_paint(event.position)
			return

	# Touch / mouse up
	elif (event is InputEventScreenTouch and not event.pressed) or \
		(event is InputEventMouseButton and not event.pressed and event.button_index == MOUSE_BUTTON_LEFT):

		if event is InputEventScreenTouch:
			touch_positions.erase(event.index)
			if touch_positions.size() < 2:
				pinch_start_distance = 0.0

		if is_painting:
			is_painting = false
			if _pending_weight_paint.x >= 0:
				_paint_at(_pending_weight_paint)   # drain the last buffered point
				_pending_weight_paint = Vector2(-1, -1)
			if skin:
				skin.stroke_end()

		if is_texture_painting:
			is_texture_painting = false
			texture_painter.stroke_end()
			_do_texture_paint()       # drain the last buffered point
			_flush_paint_preview()    # always end with a fresh upload
			if brush_cursor:
				brush_cursor.visible = false

		if editor_node.is_gizmo_dragging():
			editor_node.on_touch_up()
			_gf_end()
		return

	# Drag (pinch or paint)
	elif event is InputEventScreenDrag or event is InputEventMouseMotion:
		if event is InputEventScreenDrag:
			touch_positions[event.index] = event.position

		if touch_positions.size() == 2:
			var keys = touch_positions.keys()
			var p0 = touch_positions[keys[0]]
			var p1 = touch_positions[keys[1]]
			var current_dist = p0.distance_to(p1)
			if pinch_start_distance > 10.0 and current_dist > 0.0:
				var ratio = current_dist / pinch_start_distance
				zoom_distance = clamp(pinch_start_zoom * ratio, 0.5, 2000000.0)
				_cam_dirty = true
			return

		var is_primary_drag = event is InputEventMouseMotion or (event is InputEventScreenDrag and event.index == 0)
		if is_painting and is_primary_drag and tab_bar.current_tab == WEIGHT_TAB:
			_pending_weight_paint = event_pos   # latest only; drained in _process
		elif is_texture_painting and is_primary_drag and tab_bar.current_tab == TEXTURE_TAB:
			_queue_texture_paint(event.position)

func _on_bonelist_item_selected(index: int) -> void:
	editor_node.set_selected_bone(index)

# ------------------------------------------------------------------
# BLENDER-STYLE VIEWPORT CONTEXT MENU + PARAMETRIC MESH CREATION
# ------------------------------------------------------------------
func _setup_viewport_context_menu() -> void:
	_viewport_context_menu = PopupMenu.new()
	_viewport_context_menu.name = "ViewportContextMenu"
	_viewport_context_menu.id_pressed.connect(_on_viewport_context_action)
	add_child(_viewport_context_menu)

	_mesh_primitive_menu = PopupMenu.new()
	_mesh_primitive_menu.name = "MeshPrimitiveMenu"
	_mesh_primitive_menu.id_pressed.connect(_on_mesh_primitive_menu_pressed)
	_viewport_context_menu.add_child(_mesh_primitive_menu)

	var primitive_names := ["Plane", "Cube", "UV Sphere", "Ico Sphere", "Cylinder", "Cone", "Torus", "Circle"]
	for i in primitive_names.size():
		_mesh_primitive_menu.add_item(primitive_names[i], 100 + i)

	_viewport_context_menu.add_submenu_item("Add Mesh", _mesh_primitive_menu.name)
	_viewport_context_menu.add_separator()
	_viewport_context_menu.add_item("Subdivide Selected Faces", MENU_SUBDIVIDE_SELECTED)
	_viewport_context_menu.add_item("Subdivide Mesh", MENU_SUBDIVIDE_MESH)
	_viewport_context_menu.add_separator()
	_viewport_context_menu.add_item("Material Settings…", MENU_MATERIAL_SETTINGS)

func _open_viewport_context_menu(local_position: Vector2) -> void:
	if not is_instance_valid(_viewport_context_menu):
		return
	_viewport_menu_position = local_position
	var canvas_position: Vector2 = vp_container.get_global_transform_with_canvas() * local_position
	var window_position := Vector2(get_window().position)
	_viewport_context_menu.position = Vector2i(window_position + canvas_position)
	_viewport_context_menu.popup()

func _on_viewport_context_action(id: int) -> void:
	match id:
		MENU_SUBDIVIDE_SELECTED:
			editor_node.subdivide_selection()
			_show_operation_message("Subdivide Selected Faces",
				"Applied one subdivision pass to the selected faces. Select faces again to repeat.")
		MENU_SUBDIVIDE_MESH:
			_begin_subdivide_mesh_operation()
		MENU_MATERIAL_SETTINGS:
			_show_material_settings_panel()

func _on_mesh_primitive_menu_pressed(id: int) -> void:
	var names := ["Plane", "Cube", "Sphere", "IcoSphere", "Cylinder", "Cone", "Torus", "Circle"]
	var index := id - 100
	if index < 0 or index >= names.size():
		return
	_create_mesh_primitive(names[index], _viewport_menu_position)

func _viewport_world_position(local_position: Vector2) -> Vector3:
	var viewport_position := _viewport_event_pos(local_position)
	var ray_origin := camera.project_ray_origin(viewport_position)
	var ray_direction := camera.project_ray_normal(viewport_position)
	# Place new objects on the world ground plane; use the orbit target if
	# the view ray does not intersect it.
	var ground := Plane(Vector3.UP, 0.0)
	var hit = ground.intersects_ray(ray_origin, ray_direction)
	if hit != null:
		return hit
	return cam_target

func _create_mesh_primitive(primitive_name: String, viewport_position: Vector2) -> void:
	var params: Dictionary = VFXPrimitives.get_default_params(_primitive_type_from_name(primitive_name))
	var generated: VFXMesh = VFXPrimitives.create_named(primitive_name, params)
	if generated == null:
		push_warning("Could not create mesh primitive: " + primitive_name)
		return

	var scene: VFXScene = editor_node.get_scene()
	if scene == null or scene.get_root() == null:
		_show_operation_message("Add " + primitive_name, "No active scene is available.")
		return

	var node: VFXSceneNode = scene.create_node(primitive_name, VFXSceneNode.NODE_MESH, scene.get_root())
	if node == null:
		_show_operation_message("Add " + primitive_name, "Could not create a scene node.")
		return

	node.set_mesh(generated)
	var primitive_material := StandardMaterial3D.new()
	primitive_material.albedo_color = Color(0.58, 0.58, 0.58, 1.0)
	primitive_material.roughness = 0.78
	# Match Blender's default solid viewport: show both sides.
	primitive_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	primitive_material.vertex_color_use_as_albedo = true
	node.set_material(primitive_material)
	node.set_local_transform(Transform3D(Basis.IDENTITY, _viewport_world_position(viewport_position)))
	if not node.has_animator():
		node.set_animator(VFXAnimator.new())

	_last_primitive_name = primitive_name
	_last_primitive_params = params.duplicate(true)
	_last_primitive_node = node
	_last_subdivide_source_mesh = null
	_last_subdivide_node = null
	_last_operation_kind = "primitive"

	editor_node.set_active_scene_node(node)
	editor_node.set_vfx_mesh(generated)
	if node.has_animator():
		animator = node.get_animator()
		editor_node.set_vfx_animator(animator)
	uv_viewport.set_mesh(generated)
	texture_painter.set_mesh_transform(node.get_global_transform())
	editor_node.refresh_mesh()
	scene_tree.force_rebuild()
	_refresh_transform_ui()
	_show_primitive_last_operation_panel()

func _primitive_type_from_name(primitive_name: String) -> int:
	match primitive_name:
		"Plane": return 0
		"Cube": return 1
		"Sphere", "UV Sphere": return 2
		"IcoSphere", "Ico Sphere": return 3
		"Cylinder": return 4
		"Cone": return 5
		"Torus": return 6
		"Circle": return 7
	return 1

func _show_primitive_last_operation_panel() -> void:
	_clear_last_operation_panel()
	if not is_instance_valid(_last_primitive_node):
		return
	_last_operation_panel = PanelContainer.new()
	_last_operation_panel.name = "LastOperationPanel"
	_last_operation_panel.z_index = 90
	_last_operation_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_last_operation_panel.custom_minimum_size = Vector2(270, 0)
	vp_container.add_child(_last_operation_panel)

	var outer := VBoxContainer.new()
	outer.add_theme_constant_override("separation", 6)
	_last_operation_panel.add_child(outer)

	var header := HBoxContainer.new()
	outer.add_child(header)
	var title := Label.new()
	title.text = "Add " + _last_primitive_name
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close_button := Button.new()
	close_button.text = "×"
	close_button.custom_minimum_size = Vector2(36, 34)
	close_button.pressed.connect(_clear_last_operation_panel)
	header.add_child(close_button)

	var help := Label.new()
	help.text = "Adjust the new mesh. Changes update it live."
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	outer.add_child(help)

	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size.y = minf(300.0, maxf(120.0, vp_container.size.y * 0.42))
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	outer.add_child(scroll)
	var fields := VBoxContainer.new()
	fields.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(fields)

	for key in _last_primitive_params.keys():
		var value = _last_primitive_params[key]
		var row := HBoxContainer.new()
		fields.add_child(row)
		var label := Label.new()
		label.text = _primitive_parameter_label(str(key))
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)

		if value is bool:
			var toggle := CheckButton.new()
			toggle.button_pressed = value
			toggle.toggled.connect(_on_last_primitive_bool_changed.bind(str(key)))
			row.add_child(toggle)
		else:
			var spin := SpinBox.new()
			spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			if _is_integer_primitive_parameter(str(key)):
				spin.min_value = 1
				spin.max_value = _primitive_integer_max(str(key))
				spin.step = 1
				spin.rounded = true
			else:
				spin.min_value = 0.001
				spin.max_value = 100.0
				spin.step = 0.01
				spin.rounded = false
			spin.value = float(value)
			spin.value_changed.connect(_on_last_primitive_number_changed.bind(str(key)))
			row.add_child(spin)

	call_deferred("_position_last_operation_panel")

func _primitive_parameter_label(key: String) -> String:
	match key:
		"size": return "Size"
		"subdivisions": return "Subdivisions"
		"radius": return "Radius"
		"segments": return "Segments"
		"rings": return "Rings"
		"radius_top": return "Top Radius"
		"radius_bottom": return "Bottom Radius"
		"depth": return "Depth"
		"sides": return "Sides"
		"cap_top": return "Cap Top"
		"cap_bottom": return "Cap Bottom"
		"radius1": return "Radius 1"
		"radius2": return "Radius 2"
		"major_radius": return "Major Radius"
		"minor_radius": return "Minor Radius"
		"major_segments": return "Major Segments"
		"minor_segments": return "Minor Segments"
	return key.capitalize()

func _is_integer_primitive_parameter(key: String) -> bool:
	return key in ["subdivisions", "segments", "rings", "sides", "major_segments", "minor_segments"]

func _primitive_integer_max(key: String) -> float:
	if key == "subdivisions":
		return 8.0 if _last_primitive_name == "IcoSphere" else 64.0
	if key in ["segments", "major_segments"]:
		return 128.0
	if key in ["rings", "minor_segments"]:
		return 96.0
	return 128.0

func _on_last_primitive_number_changed(value: float, key: String) -> void:
	if _last_operation_kind != "primitive" or not is_instance_valid(_last_primitive_node):
		return
	if _is_integer_primitive_parameter(key):
		_last_primitive_params[key] = int(round(value))
	else:
		_last_primitive_params[key] = value
	_rebuild_last_primitive()

func _on_last_primitive_bool_changed(value: bool, key: String) -> void:
	if _last_operation_kind != "primitive" or not is_instance_valid(_last_primitive_node):
		return
	_last_primitive_params[key] = value
	_rebuild_last_primitive()

func _rebuild_last_primitive() -> void:
	var generated: VFXMesh = VFXPrimitives.create_named(_last_primitive_name, _last_primitive_params)
	if generated == null:
		return
	_last_primitive_node.set_mesh(generated)
	editor_node.set_vfx_mesh(generated)
	uv_viewport.set_mesh(generated)
	texture_painter.set_mesh_transform(_last_primitive_node.get_global_transform())
	editor_node.refresh_mesh()

func _begin_subdivide_mesh_operation() -> void:
	var node: VFXSceneNode = editor_node.get_active_scene_node()
	if node == null or not node.has_mesh():
		_show_operation_message("Subdivide Mesh", "Select a mesh object first.")
		return
	_last_subdivide_source_mesh = _duplicate_vfx_mesh(node.get_mesh())
	_last_subdivide_node = node
	_last_subdivide_levels = 1
	_last_primitive_node = null
	_last_operation_kind = "subdivide"
	_rebuild_subdivide_mesh()
	_show_subdivide_last_operation_panel()

func _rebuild_subdivide_mesh() -> void:
	if _last_subdivide_source_mesh == null or not is_instance_valid(_last_subdivide_node):
		return
	var generated := _duplicate_vfx_mesh(_last_subdivide_source_mesh)
	for level in range(_last_subdivide_levels):
		var faces_before := generated.get_face_count()
		for face_index in range(faces_before):
			generated.subdivide_face(face_index)
	generated.recalculate_normals()
	_last_subdivide_node.set_mesh(generated)
	editor_node.set_vfx_mesh(generated)
	uv_viewport.set_mesh(generated)
	texture_painter.set_mesh_transform(_last_subdivide_node.get_global_transform())
	editor_node.refresh_mesh()

func _show_subdivide_last_operation_panel() -> void:
	_clear_last_operation_panel()
	_last_operation_panel = PanelContainer.new()
	_last_operation_panel.name = "LastOperationPanel"
	_last_operation_panel.z_index = 90
	_last_operation_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_last_operation_panel.custom_minimum_size = Vector2(270, 0)
	vp_container.add_child(_last_operation_panel)

	var outer := VBoxContainer.new()
	outer.add_theme_constant_override("separation", 6)
	_last_operation_panel.add_child(outer)
	var header := HBoxContainer.new()
	outer.add_child(header)
	var title := Label.new()
	title.text = "Subdivide Mesh"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close_button := Button.new()
	close_button.text = "×"
	close_button.custom_minimum_size = Vector2(36, 34)
	close_button.pressed.connect(_clear_last_operation_panel)
	header.add_child(close_button)
	var note := Label.new()
	note.text = "Subdivides every face on the active object. This is a live last-operation preview."
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	outer.add_child(note)
	var row := HBoxContainer.new()
	outer.add_child(row)
	var label := Label.new()
	label.text = "Levels"
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(label)
	var spin := SpinBox.new()
	spin.min_value = 1
	var live_faces := _last_subdivide_source_mesh.get_live_face_count()
	spin.max_value = 3.0 if live_faces < 5000 else (2.0 if live_faces < 20000 else 1.0)
	spin.step = 1
	spin.rounded = true
	spin.value = _last_subdivide_levels
	spin.value_changed.connect(func(value: float):
		_last_subdivide_levels = int(value)
		_rebuild_subdivide_mesh()
	)
	row.add_child(spin)
	call_deferred("_position_last_operation_panel")

func _show_operation_message(title_text: String, message_text: String) -> void:
	_clear_last_operation_panel()
	_last_operation_panel = PanelContainer.new()
	_last_operation_panel.name = "LastOperationPanel"
	_last_operation_panel.z_index = 90
	_last_operation_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_last_operation_panel.custom_minimum_size = Vector2(270, 0)
	vp_container.add_child(_last_operation_panel)
	var outer := VBoxContainer.new()
	outer.add_theme_constant_override("separation", 6)
	_last_operation_panel.add_child(outer)
	var header := HBoxContainer.new()
	outer.add_child(header)
	var title := Label.new()
	title.text = title_text
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close_button := Button.new()
	close_button.text = "×"
	close_button.custom_minimum_size = Vector2(36, 34)
	close_button.pressed.connect(_clear_last_operation_panel)
	header.add_child(close_button)
	var message := Label.new()
	message.text = message_text
	message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	outer.add_child(message)
	call_deferred("_position_last_operation_panel")

func _position_last_operation_panel() -> void:
	if not is_instance_valid(_last_operation_panel):
		return
	_last_operation_panel.reset_size()
	var panel_size := _last_operation_panel.get_combined_minimum_size()
	var max_width := maxf(180.0, vp_container.size.x - 24.0)
	panel_size.x = minf(max_width, maxf(270.0, panel_size.x))
	_last_operation_panel.size = panel_size
	_last_operation_panel.position = Vector2(12.0,
		maxf(12.0, vp_container.size.y - panel_size.y - 12.0))

func _clear_last_operation_panel() -> void:
	if is_instance_valid(_last_operation_panel):
		_last_operation_panel.queue_free()
	_last_operation_panel = null


# ------------------------------------------------------------------
# PER-NODE MATERIALS
# ------------------------------------------------------------------
func _show_material_settings_panel() -> void:
	var node: VFXSceneNode = editor_node.get_active_scene_node()
	if node == null or not node.has_mesh():
		_show_operation_message("Material Settings", "Select a mesh scene node first.")
		return

	_material_target_node = node
	var existing: Material = node.get_material()
	if existing == null and node.get_mesh() != null:
		existing = node.get_mesh().material
	if existing is StandardMaterial3D:
		_material_standard = (existing as StandardMaterial3D).duplicate(true)
	else:
		_material_standard = StandardMaterial3D.new()
		_material_standard.albedo_color = Color(0.72, 0.76, 0.82, 1.0)
		_material_standard.roughness = 0.72

	_clear_last_operation_panel()
	_last_operation_panel = PanelContainer.new()
	_last_operation_panel.name = "MaterialSettingsPanel"
	_last_operation_panel.z_index = 90
	_last_operation_panel.mouse_filter = Control.MOUSE_FILTER_STOP
	_last_operation_panel.custom_minimum_size = Vector2(290, 0)
	vp_container.add_child(_last_operation_panel)

	var outer := VBoxContainer.new()
	outer.add_theme_constant_override("separation", 7)
	_last_operation_panel.add_child(outer)

	var header := HBoxContainer.new()
	outer.add_child(header)
	var title := Label.new()
	title.text = "Material · " + node.get_node_name()
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var close_button := Button.new()
	close_button.text = "×"
	close_button.custom_minimum_size = Vector2(36, 34)
	close_button.pressed.connect(_clear_last_operation_panel)
	header.add_child(close_button)

	var info := Label.new()
	info.text = "Material is stored on this scene node. It can be a Standard or Shader material."
	info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	outer.add_child(info)

	var color_row := HBoxContainer.new()
	outer.add_child(color_row)
	var color_label := Label.new()
	color_label.text = "Base Color"
	color_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	color_row.add_child(color_label)
	_material_color_picker = ColorPickerButton.new()
	_material_color_picker.color = _material_standard.albedo_color
	_material_color_picker.custom_minimum_size = Vector2(76, 34)
	_material_color_picker.color_changed.connect(_on_material_color_changed)
	color_row.add_child(_material_color_picker)

	var rough_row := HBoxContainer.new()
	outer.add_child(rough_row)
	var rough_label := Label.new()
	rough_label.text = "Roughness"
	rough_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	rough_row.add_child(rough_label)
	_material_roughness_spin = SpinBox.new()
	_material_roughness_spin.min_value = 0.0
	_material_roughness_spin.max_value = 1.0
	_material_roughness_spin.step = 0.01
	_material_roughness_spin.value = _material_standard.roughness
	_material_roughness_spin.value_changed.connect(_on_material_roughness_changed)
	rough_row.add_child(_material_roughness_spin)

	var metal_row := HBoxContainer.new()
	outer.add_child(metal_row)
	var metal_label := Label.new()
	metal_label.text = "Metallic"
	metal_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	metal_row.add_child(metal_label)
	_material_metallic_spin = SpinBox.new()
	_material_metallic_spin.min_value = 0.0
	_material_metallic_spin.max_value = 1.0
	_material_metallic_spin.step = 0.01
	_material_metallic_spin.value = _material_standard.metallic
	_material_metallic_spin.value_changed.connect(_on_material_metallic_changed)
	metal_row.add_child(_material_metallic_spin)

	var texture_button := Button.new()
	texture_button.text = "Load Albedo Texture…"
	texture_button.pressed.connect(_open_material_texture_dialog)
	outer.add_child(texture_button)

	var shader_button := Button.new()
	shader_button.text = "Use .gdshader…"
	shader_button.pressed.connect(_open_material_shader_dialog)
	outer.add_child(shader_button)

	var apply_button := Button.new()
	apply_button.text = "Apply Standard Material"
	apply_button.pressed.connect(_apply_standard_material_to_node)
	outer.add_child(apply_button)

	var reset_button := Button.new()
	reset_button.text = "Clear Node Override"
	reset_button.pressed.connect(_clear_node_material_override)
	outer.add_child(reset_button)

	if _material_texture_dialog == null:
		_material_texture_dialog = FileDialog.new()
		_material_texture_dialog.name = "MaterialTextureDialog"
		_material_texture_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
		_material_texture_dialog.access = FileDialog.ACCESS_FILESYSTEM
		_material_texture_dialog.add_filter("*.png, *.jpg, *.jpeg, *.webp, *.bmp", "Texture Images")
		_material_texture_dialog.file_selected.connect(_on_material_texture_selected)
		add_child(_material_texture_dialog)
	if _material_shader_dialog == null:
		_material_shader_dialog = FileDialog.new()
		_material_shader_dialog.name = "MaterialShaderDialog"
		_material_shader_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
		_material_shader_dialog.access = FileDialog.ACCESS_FILESYSTEM
		_material_shader_dialog.add_filter("*.gdshader", "Godot Shader")
		_material_shader_dialog.file_selected.connect(_on_material_shader_selected)
		add_child(_material_shader_dialog)

	_apply_standard_material_to_node()
	call_deferred("_position_last_operation_panel")


func _apply_standard_material_to_node() -> void:
	if not is_instance_valid(_material_target_node) or _material_standard == null:
		return
	_material_target_node.set_material(_material_standard)
	editor_node.refresh_mesh()


func _on_material_color_changed(color: Color) -> void:
	if _material_standard == null:
		return
	_material_standard.albedo_color = color
	_apply_standard_material_to_node()


func _on_material_roughness_changed(value: float) -> void:
	if _material_standard == null:
		return
	_material_standard.roughness = value
	_apply_standard_material_to_node()


func _on_material_metallic_changed(value: float) -> void:
	if _material_standard == null:
		return
	_material_standard.metallic = value
	_apply_standard_material_to_node()


func _open_material_texture_dialog() -> void:
	if _material_texture_dialog != null:
		_material_texture_dialog.popup_centered(Vector2i(760, 560))


func _on_material_texture_selected(path: String) -> void:
	var image := Image.load_from_file(path)
	if image != null and not image.is_empty():
		_material_standard.albedo_texture = ImageTexture.create_from_image(image)
		_apply_standard_material_to_node()
	else:
		push_warning("Could not load texture: " + path)


func _open_material_shader_dialog() -> void:
	if _material_shader_dialog != null:
		_material_shader_dialog.popup_centered(Vector2i(760, 560))


func _on_material_shader_selected(path: String) -> void:
	var shader_code := FileAccess.get_file_as_string(path)
	if not shader_code.is_empty():
		var shader_resource := Shader.new()
		shader_resource.code = shader_code
		var shader_material := ShaderMaterial.new()
		shader_material.shader = shader_resource
		_material_target_node.set_material(shader_material)
		editor_node.refresh_mesh()
		_show_operation_message("Shader Material", "Applied " + path.get_file() + " to " + _material_target_node.get_node_name() + ".")
	else:
		push_warning("Could not read Godot shader: " + path)


func _clear_node_material_override() -> void:
	if is_instance_valid(_material_target_node):
		_material_target_node.set_material(null)
		editor_node.refresh_mesh()
		_show_operation_message("Material Override Cleared", "The node now uses its mesh's imported material, if one exists.")



func _ensure_node_material(node: VFXSceneNode) -> void:
	if node == null or not node.has_mesh():
		return
	if node.get_material() != null:
		return
	var mesh: VFXMesh = node.get_mesh()
	if mesh != null and mesh.material != null:
		node.set_material(mesh.material)
	else:
		var material := StandardMaterial3D.new()
		# Neutral gray fallback, close to Blender's default solid viewport color.
		material.albedo_color = Color(0.58, 0.58, 0.58, 1.0)
		material.roughness = 0.78
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		material.vertex_color_use_as_albedo = true
		node.set_material(material)


# ------------------------------------------------------------------
# MATERIAL INSPECTOR
# ------------------------------------------------------------------

# ------------------------------------------------------------------
func _is_material_inspector_candidate(candidate: Node) -> bool:
	if not is_instance_valid(candidate):
		return false
	# Do not rely on the global class_name cache: some projects may not have
	# refreshed Godot's global script class cache after adding this script.
	return candidate.has_method("configure") \
		and candidate.has_method("set_target_node") \
		and candidate.has_method("clear_target")

func _find_material_inspector() -> Control:
	if not is_instance_valid(material_container):
		return null
	# First prefer the documented node name, anywhere below the foldable.
	var named := material_container.find_child("MaterialInspector", true, false)
	if named is Control and _is_material_inspector_candidate(named):
		return named as Control
	# Fall back to the script API, so a wrapper/content node or a slightly
	# different child name cannot make the inspector disappear.
	for candidate in material_container.find_children("*", "Control", true, false):
		if _is_material_inspector_candidate(candidate):
			return candidate as Control
	return null

func _setup_material_inspector_node() -> void:
	if not is_instance_valid(material_container):
		push_warning("MaterialFoldableCont was not found.")
		return
	material_container.visible = false
	# Resolve again at setup time in case the foldable container creates or
	# reparents its content controls during its own initialization.
	if not is_instance_valid(material_inspector):
		material_inspector = _find_material_inspector()
	if not is_instance_valid(material_inspector):
		push_warning("Material inspector script not found under MaterialFoldableCont. Attach material_inspector.gd to a Control child; it must expose configure(), set_target_node(), and clear_target().")
		return
	material_inspector.call(
		"configure",
		editor_node,
		scene_tree,
		Callable(self, "_ensure_node_material"),
		Callable(self, "_show_operation_message")
	)

func _update_material_inspector(node: VFXSceneNode) -> void:
	if not is_instance_valid(material_container):
		return
	if node == null or not node.has_mesh():
		material_container.visible = false
		if is_instance_valid(material_inspector):
			material_inspector.call("clear_target")
		return
	if not is_instance_valid(material_inspector):
		material_inspector = _find_material_inspector()
	if not is_instance_valid(material_inspector):
		push_warning("A mesh is selected, but no MaterialInspector script node was found under MaterialFoldableCont. Check that material_inspector.gd is attached to a Control child.")
		return
	material_container.visible = true
	material_inspector.call("set_target_node", node)



# SCENE TREE
# ------------------------------------------------------------------
func _on_scene_tree_nodes_selected(nodes: Array) -> void:
	if nodes.is_empty():
		if is_instance_valid(material_container):
			material_container.visible = false
		_material_target_node = null
		if is_instance_valid(material_inspector):
			material_inspector.call("clear_target")
		# An empty selection is meaningful after deleting the active node. Clear
		# the native selection as well as the viewport overlays immediately.
		if editor_node != null:
			editor_node.set_scene_nodes_selected([])
			editor_node.refresh_scene()
		if is_instance_valid(texture_painter):
			texture_painter.set_mesh_transform(Transform3D.IDENTITY)
		if is_instance_valid(uv_viewport):
			uv_viewport.set_mesh(null)
		skeleton = null
		skin = null
		animator = editor_node.get_vfx_animator() if editor_node != null else null
		BoneList.clear()
		current_bone_idx = -1
		if is_instance_valid(anim_panel):
			anim_panel.sync_from_editor()
		_refresh_transform_ui()
		return
	var node: VFXSceneNode = nodes[nodes.size() - 1]   # last tapped = active
	_update_material_inspector(node)

	# C++ side: wires mesh/skin/skeleton/animator from the ACTIVE node and
	# stores the whole list for object-mode multi-drag.
	editor_node.set_scene_nodes_selected(nodes)

	# === GDScript-side context follows the ACTIVE node ===
	skeleton = null
	skin = null
	animator = null
	BoneList.clear()

	# Selecting an object means "edit the object": drop any bone selection.
	# Otherwise the selected bone keeps owning the gizmo (bone branch has
	# priority in gizmo_drag) and drags keep moving the bone even though
	# the user clicked an object node.
	current_bone_idx = -1
	editor_node.set_selected_bone(-1)

	if node.has_mesh():
		_ensure_node_material(node)
		# FIX: real global transform instead of hardcoded IDENTITY (see import handler)
		texture_painter.set_mesh_transform(node.get_global_transform())
		uv_viewport.set_mesh(node.get_mesh())
	if node.has_skeleton():
		skeleton = node.get_skeleton()
	if node.has_skin():
		skin = node.get_skin()
		editor_node.set_vfx_skin(skin)
	# Keep the current playback target when selecting a node that has no
	# animator. The native editor owns playback independently of selection.
	animator = editor_node.get_vfx_animator()

	_prepare_bone_context(true)

	editor_node.refresh_mesh()
	if is_instance_valid(anim_panel):
		anim_panel.sync_from_editor()
	_refresh_transform_ui()

func _on_scene_tree_visibility_changed(node: VFXSceneNode):
	editor_node.refresh_scene()   # no print() here: logcat writes are slow on Android


# ------------------------------------------------------------------
# FALLOFF
# ------------------------------------------------------------------
func _falloff(index):
	match index :
		0 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_CONSTANT
		1 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_INVERSE_SQUARE
		2 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_LINEAR
		3 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_RANDOM
		4 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_ROOT
		5 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_SHARP
		6 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_SMOOTH
		7 : editor_node.falloff_mode = VFXEditorNode.FALLOFF_SPHERE

func menu_option(id:int):
	match id :
		0 : _on_import_pressed()
		1 : _on_export_file_selected(&"res://vfx_export.glb")


func _setup_modelling():
	%ModelingFalloff.item_selected.connect(_falloff)

	var mode_row := HBoxContainer.new()
	mode_row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(mode_row)

	var mode_group := ButtonGroup.new()
	for mode_name in ["Object", "Vertex", "Edge", "Face"]:
		var btn := Button.new()
		btn.text = mode_name
		btn.toggle_mode = true
		btn.button_group = mode_group
		btn.pressed.connect(_on_edit_mode_selected.bind(mode_name))
		mode_row.add_child(btn)
		if mode_name == _modeling_edit_mode:
			btn.button_pressed = true

	var tool_row := HBoxContainer.new()
	tool_row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(tool_row)

	_setup_curve_tools()


# ------------------------------------------------------------------
# CURVE TO MESH — mobile-first controls using the existing native API
# ------------------------------------------------------------------
func _setup_curve_tools() -> void:
	_curve = editor_node.get_active_curve()
	if _curve == null:
		_curve = VFXCurve.new()
		editor_node.set_active_curve(_curve)

	_curve_overlay = CurveViewportOverlay.new()
	_curve_overlay.name = "CurveViewportOverlay"
	_curve_overlay.curve = _curve
	_curve_overlay.camera_3d = camera
	_curve_overlay.viewport_size_ref = sub_viewport.size
	_curve_overlay.z_index = 50
	vp_container.add_child(_curve_overlay)

	# Geometry Nodes Bezier preview is a separate overlay and never touches the legacy curve tool.
	_geometry_bezier_curve = VFXCurve.new()
	_geometry_bezier_overlay = CurveViewportOverlay.new()
	_geometry_bezier_overlay.name = "GeometryNodesBezierViewportOverlay"
	_geometry_bezier_overlay.curve = _geometry_bezier_curve
	_geometry_bezier_overlay.camera_3d = camera
	_geometry_bezier_overlay.viewport_size_ref = sub_viewport.size
	_geometry_bezier_overlay.show_all_handles = true
	_geometry_bezier_overlay.visible = false
	_geometry_bezier_overlay.z_index = 51
	vp_container.add_child(_geometry_bezier_overlay)

	var scroll := ScrollContainer.new()
	scroll.name = "CurveToolsScroll"
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.custom_minimum_size.y = 220
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	bottom_panel.add_child(scroll)

	var section := VBoxContainer.new()
	section.add_theme_constant_override("separation", 6)
	section.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(section)

	var title := Label.new()
	title.text = "CURVE TO MESH"
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	section.add_child(title)

	var action_row := HBoxContainer.new()
	action_row.add_theme_constant_override("separation", 6)
	section.add_child(action_row)
	_curve_add_button = _curve_make_button("＋ Add points", _toggle_curve_add_mode)
	_curve_add_button.toggle_mode = true
	action_row.add_child(_curve_add_button)
	action_row.add_child(_curve_make_button("Clear", _curve_clear))

	var shape_row := HBoxContainer.new()
	shape_row.add_theme_constant_override("separation", 6)
	section.add_child(shape_row)
	shape_row.add_child(_curve_make_button("Tube", func(): _curve_set_shape(0)))
	shape_row.add_child(_curve_make_button("Ribbon", func(): _curve_set_shape(1)))

	var profile_row := HBoxContainer.new()
	profile_row.add_theme_constant_override("separation", 6)
	section.add_child(profile_row)
	var profile_label := Label.new()
	profile_label.text = "Tube profile"
	profile_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	profile_row.add_child(profile_label)
	_curve_profile_picker = OptionButton.new()
	_curve_profile_picker.custom_minimum_size = Vector2(150, 44)
	_curve_profile_picker.add_item("Round", 0)
	_curve_profile_picker.add_item("Square", 1)
	_curve_profile_picker.add_item("Diamond", 2)
	_curve_profile_picker.add_item("Octagon", 3)
	_curve_profile_picker.item_selected.connect(func(id: int):
		_curve_profile = id
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)
	profile_row.add_child(_curve_profile_picker)
	_curve_profile_picker.select(_curve_profile)

	_curve_add_slider(section, "Profile rotation", -180, 180, 1, _curve_profile_rotation,
		func(v: float):
			_curve_profile_rotation = v
			if _curve_preview_enabled:
				_curve_rebuild_preview()
	)

	# Global taper shaping, layered over the existing per-point radius values.
	var taper_row := HBoxContainer.new()
	taper_row.add_theme_constant_override("separation", 6)
	section.add_child(taper_row)
	var taper_label := Label.new()
	taper_label.text = "Taper preset"
	taper_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	taper_row.add_child(taper_label)
	_curve_taper_picker = OptionButton.new()
	_curve_taper_picker.custom_minimum_size = Vector2(150, 44)
	for item in ["Custom", "Linear", "Pointed", "Bulge", "Pinch"]:
		_curve_taper_picker.add_item(item)
	_curve_taper_picker.item_selected.connect(func(id: int):
		_curve_taper_preset = id
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)
	taper_row.add_child(_curve_taper_picker)
	_curve_taper_picker.select(_curve_taper_preset)

	# Per-point cross-section profile selector.
	var point_profile_row := HBoxContainer.new()
	point_profile_row.add_theme_constant_override("separation", 6)
	section.add_child(point_profile_row)
	var point_profile_label := Label.new()
	point_profile_label.text = "Selected point profile"
	point_profile_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	point_profile_row.add_child(point_profile_label)
	_curve_profile_at_point_picker = OptionButton.new()
	_curve_profile_at_point_picker.custom_minimum_size = Vector2(150, 44)
	for item in ["Round", "Square", "Diamond", "Octagon"]:
		_curve_profile_at_point_picker.add_item(item)
	_curve_profile_at_point_picker.item_selected.connect(func(id: int):
		_curve_set_selected_point_profile(id)
	)
	point_profile_row.add_child(_curve_profile_at_point_picker)
	_curve_profile_at_point_picker.select(_curve_profile)

	_curve_add_slider(section, "Selected point tilt", -180, 180, 1, 0.0,
		func(v: float):
			_curve_set_selected_point_tilt(v)
	)

	# Independent caps for the tube's two ends.
	var caps_row := HBoxContainer.new()
	caps_row.add_theme_constant_override("separation", 6)
	section.add_child(caps_row)
	var start_cap_label := Label.new()
	start_cap_label.text = "Start cap"
	caps_row.add_child(start_cap_label)
	_curve_start_cap_picker = OptionButton.new()
	for item in ["Flat", "Open", "Pointed"]:
		_curve_start_cap_picker.add_item(item)
	_curve_start_cap_picker.item_selected.connect(func(id: int):
		_curve_start_cap = id
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)
	caps_row.add_child(_curve_start_cap_picker)
	var end_cap_label := Label.new()
	end_cap_label.text = "End cap"
	caps_row.add_child(end_cap_label)
	_curve_end_cap_picker = OptionButton.new()
	for item in ["Flat", "Open", "Pointed"]:
		_curve_end_cap_picker.add_item(item)
	_curve_end_cap_picker.item_selected.connect(func(id: int):
		_curve_end_cap = id
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)
	caps_row.add_child(_curve_end_cap_picker)
	_curve_start_cap_picker.select(_curve_start_cap)
	_curve_end_cap_picker.select(_curve_end_cap)

	# Ribbon orientation controls. Both modes follow the curve in 3D; they
	# differ only in how the ribbon's width axis is oriented around its tangent.
	var ribbon_orientation_row := HBoxContainer.new()
	ribbon_orientation_row.add_theme_constant_override("separation", 6)
	section.add_child(ribbon_orientation_row)
	var ribbon_orientation_label := Label.new()
	ribbon_orientation_label.text = "Ribbon orientation"
	ribbon_orientation_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ribbon_orientation_row.add_child(ribbon_orientation_label)
	_curve_ribbon_orientation_picker = OptionButton.new()
	_curve_ribbon_orientation_picker.custom_minimum_size = Vector2(150, 44)
	_curve_ribbon_orientation_picker.add_item("Follow Curve", 0)
	_curve_ribbon_orientation_picker.add_item("World Y-Up", 1)
	_curve_ribbon_orientation_picker.add_item("World Vertical", 2)
	_curve_ribbon_orientation_picker.item_selected.connect(func(id: int):
		_curve_ribbon_orientation = id
		if _curve_preview_enabled and _curve_shape == 1:
			_curve_rebuild_preview()
	)
	ribbon_orientation_row.add_child(_curve_ribbon_orientation_picker)
	_curve_ribbon_orientation_picker.select(_curve_ribbon_orientation)

	_curve_add_slider(section, "Ribbon tilt", -180, 180, 1, _curve_ribbon_tilt,
		func(v: float):
			_curve_ribbon_tilt = v
			if _curve_preview_enabled and _curve_shape == 1:
				_curve_rebuild_preview()
	)

	var handle_row := HBoxContainer.new()
	handle_row.add_theme_constant_override("separation", 6)
	section.add_child(handle_row)
	var handle_label := Label.new()
	handle_label.text = "Handle mode"
	handle_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	handle_row.add_child(handle_label)
	_curve_handle_mode_picker = OptionButton.new()
	_curve_handle_mode_picker.custom_minimum_size = Vector2(150, 44)
	_curve_handle_mode_picker.add_item("Free", 0)
	_curve_handle_mode_picker.add_item("Aligned", 1)
	_curve_handle_mode_picker.add_item("Vector", 2)
	_curve_handle_mode_picker.add_item("Automatic", 3)
	_curve_handle_mode_picker.item_selected.connect(_curve_apply_selected_handle_mode)
	handle_row.add_child(_curve_handle_mode_picker)

	var axis_row := HBoxContainer.new()
	axis_row.add_theme_constant_override("separation", 6)
	section.add_child(axis_row)
	var axis_label := Label.new()
	axis_label.text = "Move constraint"
	axis_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	axis_row.add_child(axis_label)
	_curve_axis_picker = OptionButton.new()
	_curve_axis_picker.custom_minimum_size = Vector2(150, 44)
	_curve_axis_picker.add_item("Free", 0)
	_curve_axis_picker.add_item("World X", 1)
	_curve_axis_picker.add_item("World Y", 2)
	_curve_axis_picker.add_item("World Z", 3)
	_curve_axis_picker.item_selected.connect(func(id: int): _curve_move_axis = id)
	axis_row.add_child(_curve_axis_picker)

	var preview_check := CheckButton.new()
	preview_check.text = "Live preview"
	preview_check.button_pressed = _curve_preview_enabled
	preview_check.custom_minimum_size.y = 48
	preview_check.toggled.connect(func(enabled: bool):
		_curve_preview_enabled = enabled
		if enabled:
			_curve_rebuild_preview()
	)
	section.add_child(preview_check)

	_curve_add_slider(section, "Radius", 0.005, 0.5, 0.005, _curve_radius,
		func(v: float): _curve_radius = v)
	_curve_add_slider(section, "Ribbon width", 0.005, 1.0, 0.005, _curve_width,
		func(v: float): _curve_width = v)

	var point_radius_row := VBoxContainer.new()
	point_radius_row.add_theme_constant_override("separation", 1)
	section.add_child(point_radius_row)
	var point_radius_header := HBoxContainer.new()
	point_radius_row.add_child(point_radius_header)
	var point_radius_title := Label.new()
	point_radius_title.text = "Selected point scale"
	point_radius_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	point_radius_header.add_child(point_radius_title)
	_curve_point_radius_value = Label.new()
	point_radius_header.add_child(_curve_point_radius_value)
	_curve_point_radius_slider = HSlider.new()
	_curve_point_radius_slider.min_value = 0.01
	_curve_point_radius_slider.max_value = 10.0
	_curve_point_radius_slider.step = 0.01
	_curve_point_radius_slider.value = 1.0
	_curve_point_radius_slider.custom_minimum_size.y = 36
	_curve_point_radius_slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	point_radius_row.add_child(_curve_point_radius_slider)
	_curve_point_radius_slider.value_changed.connect(_curve_set_selected_point_radius)
	_curve_point_radius_slider.drag_ended.connect(func(_changed: bool):
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)
	_curve_update_point_radius_control()
	_curve_add_slider(section, "Curve detail", 4, 96, 1, _curve_longitudinal,
		func(v: float): _curve_longitudinal = int(v))
	_curve_add_slider(section, "Tube sides", 3, 16, 1, _curve_radial,
		func(v: float): _curve_radial = int(v))

	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", 6)
	section.add_child(footer)
	_curve_status = Label.new()
	_curve_status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_curve_status.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	footer.add_child(_curve_status)
	footer.add_child(_curve_make_button("Build mesh", _curve_build_mesh))
	footer.add_child(_curve_make_button("Geometry Nodes", _open_geometry_nodes_workspace))
	_curve_update_status()

func _open_geometry_nodes_workspace() -> void:
	# This is a separate workspace. The legacy curve tool and its active VFXCurve stay intact.
	var existing := get_node_or_null("GeometryNodesWorkspace") as Control
	if is_instance_valid(existing):
		existing.move_to_front()
		return
	var workspace := GeometryNodesWorkspaceScript.new()
	workspace.name = "GeometryNodesWorkspace"
	workspace.configure(editor_node, scene_tree)
	workspace.closed.connect(_on_geometry_nodes_workspace_closed)
	workspace.bezier_anchors_changed.connect(_on_geometry_bezier_anchors_changed)
	workspace.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	workspace.z_index = 200
	add_child(workspace)


func _on_geometry_nodes_workspace_closed() -> void:
	_geometry_bezier_node_id = ""
	if is_instance_valid(_geometry_bezier_overlay):
		_geometry_bezier_overlay.visible = false


func _on_geometry_bezier_selection_changed(node_id: String, anchors: Array) -> void:
	_geometry_bezier_node_id = node_id
	_geometry_bezier_overlay.visible = not node_id.is_empty() and anchors.size() >= 2
	_update_geometry_bezier_overlay(anchors)


func _on_geometry_bezier_anchors_changed(node_id: String, anchors: Array) -> void:
	if node_id == _geometry_bezier_node_id:
		_update_geometry_bezier_overlay(anchors)


func _update_geometry_bezier_overlay(anchors: Array) -> void:
	if not is_instance_valid(_geometry_bezier_overlay):
		return
	var preview_curve := VFXCurve.new()
	for item in anchors:
		if not item is Dictionary:
			continue
		var point: Vector3 = item.get("position", Vector3.ZERO)
		var handle_in: Vector3 = item.get("handle_in", point)
		var handle_out: Vector3 = item.get("handle_out", point)
		preview_curve.add_point(point, handle_in, handle_out)
	_geometry_bezier_curve = preview_curve
	_geometry_bezier_overlay.curve = preview_curve
	_geometry_bezier_overlay.camera_3d = camera
	_geometry_bezier_overlay.viewport_size_ref = sub_viewport.size
	_geometry_bezier_overlay.queue_redraw()


func _curve_sync_bezier_to_workspace() -> void:
	# The bottom-panel curve tool is independent from Geometry Nodes.
	pass


func _curve_make_button(caption: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = caption
	button.custom_minimum_size = Vector2(0, 48)
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.pressed.connect(callback)
	return button

func _curve_add_slider(parent: Control, caption: String, min_value: float,
		max_value: float, step: float, initial: float, setter: Callable) -> void:
	var row := VBoxContainer.new()
	row.add_theme_constant_override("separation", 1)
	parent.add_child(row)
	var header := HBoxContainer.new()
	row.add_child(header)
	var label := Label.new()
	label.text = caption
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(label)
	var value_label := Label.new()
	value_label.text = str(initial)
	header.add_child(value_label)
	var slider := HSlider.new()
	slider.min_value = min_value
	slider.max_value = max_value
	slider.step = step
	slider.value = initial
	slider.custom_minimum_size.y = 32
	row.add_child(slider)
	slider.value_changed.connect(func(value: float):
		setter.call(value)
		value_label.text = "%.3f" % value if step < 1.0 else str(int(value))
	)
	# Rebuild once when the finger/mouse releases the slider, not on every
	# intermediate event. This avoids repeatedly generating geometry on mobile.
	slider.drag_ended.connect(func(_changed: bool):
		if _curve_preview_enabled:
			_curve_rebuild_preview()
	)

func _toggle_curve_add_mode() -> void:
	_curve_add_mode = not _curve_add_mode
	if is_instance_valid(_curve_add_button):
		_curve_add_button.text = "Tap viewport…" if _curve_add_mode else "＋ Add points"
		_curve_add_button.button_pressed = _curve_add_mode
	_curve_update_status()

func _curve_ensure_handle_modes() -> void:
	if _curve == null:
		_curve_handle_modes.clear()
		return
	while _curve_handle_modes.size() < _curve.get_point_count():
		_curve_handle_modes.append(3)
	while _curve_handle_modes.size() > _curve.get_point_count():
		_curve_handle_modes.pop_back()

func _curve_get_handle_mode(index: int) -> int:
	_curve_ensure_handle_modes()
	if index < 0 or index >= _curve_handle_modes.size():
		return 3
	return _curve_handle_modes[index]

func _curve_set_handle_mode(index: int, mode: int) -> void:
	if _curve == null or index < 0 or index >= _curve.get_point_count():
		return
	_curve_ensure_handle_modes()
	_curve_handle_modes[index] = clampi(mode, 0, 3)
	_curve_apply_handle_mode(index, mode)
	_curve_sync_overlay()
	_curve_sync_bezier_to_workspace()
	if _curve_preview_enabled:
		_curve_rebuild_preview()

func _curve_apply_selected_handle_mode(mode: int) -> void:
	if _curve_selected_point >= 0:
		_curve_set_handle_mode(_curve_selected_point, mode)

func _curve_apply_handle_mode(index: int, mode: int) -> void:
	var p := _curve.get_point(index)
	var count := _curve.get_point_count()
	if mode == 0:
		return
	if mode == 1:
		var incoming := _curve.get_handle_in(index)
		var outgoing := _curve.get_handle_out(index)
		var axis := (outgoing - p).normalized()
		if axis.is_zero_approx():
			axis = (p - incoming).normalized()
		if axis.is_zero_approx():
			axis = Vector3.RIGHT
		var in_length := p.distance_to(incoming)
		var out_length := p.distance_to(outgoing)
		_curve.set_handle_in(index, p - axis * in_length)
		_curve.set_handle_out(index, p + axis * out_length)
	elif mode == 2:
		if index > 0:
			var prev := _curve.get_point(index - 1)
			_curve.set_handle_in(index, p - (p - prev) / 3.0)
		else:
			_curve.set_handle_in(index, p)
		if index < count - 1:
			var next := _curve.get_point(index + 1)
			_curve.set_handle_out(index, p + (next - p) / 3.0)
		else:
			_curve.set_handle_out(index, p)
	elif mode == 3:
		var tangent := Vector3.ZERO
		if index == 0 and count > 1:
			tangent = (_curve.get_point(1) - p) / 3.0
		elif index == count - 1 and count > 1:
			tangent = (p - _curve.get_point(index - 1)) / 3.0
		elif count > 2:
			tangent = (_curve.get_point(index + 1) - _curve.get_point(index - 1)) / 6.0
		_curve.set_handle_in(index, p - tangent)
		_curve.set_handle_out(index, p + tangent)

func _curve_sync_overlay() -> void:
	if not is_instance_valid(_curve_overlay):
		return
	_curve_overlay.curve = _curve
	_curve_overlay.camera_3d = camera
	_curve_overlay.viewport_size_ref = sub_viewport.size
	_curve_overlay.selected_point = _curve_selected_point
	_curve_overlay.queue_redraw()

func _curve_pick(screen_pos: Vector2) -> Dictionary:
	if _curve == null or camera == null:
		return {}
	var best_distance := 1.0e20
	var best := {}
	if _curve.get_point_count() >= 2:
		for endpoint in [0, _curve.get_point_count() - 1]:
			var endpoint_point := _curve.get_point(endpoint)
			var neighbour := _curve.get_point(1) if endpoint == 0 else _curve.get_point(_curve.get_point_count() - 2)
			if camera.is_position_behind(endpoint_point) or camera.is_position_behind(neighbour):
				continue
			var a := camera.unproject_position(endpoint_point)
			var b := camera.unproject_position(neighbour)
			var direction := (b - a).normalized()
			if direction.length_squared() < 0.001:
				direction = Vector2.RIGHT
			var grip := a + Vector2(-direction.y, direction.x) * 42.0
			var grip_distance := screen_pos.distance_squared_to(grip)
			if grip_distance <= 34.0 * 34.0 and grip_distance < best_distance:
				best_distance = grip_distance
				best = {"point": endpoint, "handle": 3 if endpoint == 0 else 4}
	if not best.is_empty():
		return best
	for i in range(_curve.get_point_count()):
		var world_point := _curve.get_point(i)
		if not camera.is_position_behind(world_point):
			var d := screen_pos.distance_squared_to(camera.unproject_position(world_point))
			if d < best_distance and d <= 26.0 * 26.0:
				best_distance = d
				best = {"point": i, "handle": 0}
	if _curve_selected_point >= 0 and _curve_selected_point < _curve.get_point_count():
		var handle_data := [
			{"position": _curve.get_handle_in(_curve_selected_point), "handle": 1},
			{"position": _curve.get_handle_out(_curve_selected_point), "handle": 2}
		]
		for item in handle_data:
			var wp: Vector3 = item.position
			if camera.is_position_behind(wp):
				continue
			var d := screen_pos.distance_squared_to(camera.unproject_position(wp))
			if d < best_distance and d <= 30.0 * 30.0:
				best_distance = d
				best = {"point": _curve_selected_point, "handle": item.handle}
	return best

func _curve_begin_edit(screen_pos: Vector2, ray_origin: Vector3, ray_dir: Vector3) -> bool:
	var picked := _curve_pick(screen_pos)
	if picked.is_empty():
		_curve_selected_point = -1
		_curve_sync_overlay()
		return false
	_curve_selected_point = int(picked.point)
	_curve_selected_handle = int(picked.handle)
	_curve_ensure_handle_modes()
	_curve_update_point_radius_control()
	if is_instance_valid(_curve_handle_mode_picker):
		_curve_handle_mode_picker.select(_curve_get_handle_mode(_curve_selected_point))
	var index := _curve_selected_point
	if _curve_selected_handle == 3 or _curve_selected_handle == 4:
		_curve_ensure_point_radii()
		_curve_endpoint_drag_start_y = screen_pos.y
		_curve_endpoint_drag_original_scale = _curve_point_radii[index]
		_curve_dragging = true
		_curve_drag_preview_elapsed = CURVE_DRAG_PREVIEW_INTERVAL
		_curve_sync_overlay()
		return true
	var anchor: Vector3 = _curve.get_point(index)
	if _curve_selected_handle == 1:
		anchor = _curve.get_handle_in(index)
	elif _curve_selected_handle == 2:
		anchor = _curve.get_handle_out(index)
	var plane_normal := camera.global_transform.basis.z.normalized()
	_curve_drag_plane = Plane(plane_normal, plane_normal.dot(anchor))
	_curve_drag_axis_origin = anchor
	if _curve_move_axis == 0:
		var hit: Variant = _curve_drag_plane.intersects_ray(ray_origin, ray_dir)
		if hit == null:
			_curve_sync_overlay()
			return false
		_curve_drag_start = hit
	else:
		var axis_start: Variant = _curve_closest_point_on_drag_axis(ray_origin, ray_dir)
		if axis_start == null:
			_curve_sync_overlay()
			return false
		_curve_drag_axis_start = axis_start
	_curve_drag_original = _curve.get_point(index)
	_curve_drag_original_in = _curve.get_handle_in(index)
	_curve_drag_original_out = _curve.get_handle_out(index)
	_curve_dragging = true
	_curve_drag_preview_elapsed = CURVE_DRAG_PREVIEW_INTERVAL
	_curve_sync_overlay()
	return true

func _curve_update_edit(ray_origin: Vector3, ray_dir: Vector3, screen_pos: Vector2) -> void:
	if not _curve_dragging or _curve_selected_point < 0:
		return
	if _curve_selected_handle == 3 or _curve_selected_handle == 4:
		var vertical_delta := _curve_endpoint_drag_start_y - screen_pos.y
		var scale_factor := pow(1.015, vertical_delta)
		_curve_point_radii[_curve_selected_point] = clampf(_curve_endpoint_drag_original_scale * scale_factor, 0.01, 10.0)
		_curve_update_point_radius_control()
		_curve_sync_overlay()
		_curve_drag_preview_elapsed = 0.0
		return
	var delta := Vector3.ZERO
	if _curve_move_axis == 0:
		var hit: Variant = _curve_drag_plane.intersects_ray(ray_origin, ray_dir)
		if hit == null:
			return
		delta = hit - _curve_drag_start
	else:
		var axis_now: Variant = _curve_closest_point_on_drag_axis(ray_origin, ray_dir)
		if axis_now == null:
			return
		delta = axis_now - _curve_drag_axis_start
	var index := _curve_selected_point
	match _curve_selected_handle:
		1:
			var moved_in := _curve_drag_original_in + delta
			_curve.set_handle_in(index, moved_in)
			if _curve_get_handle_mode(index) == 1:
				var axis_in := (moved_in - _curve_drag_original).normalized()
				_curve.set_handle_out(index, _curve_drag_original - axis_in * _curve_drag_original_out.distance_to(_curve_drag_original))
			else:
				_curve_handle_modes[index] = 0
				if is_instance_valid(_curve_handle_mode_picker):
					_curve_handle_mode_picker.select(0)
		2:
			var moved_out := _curve_drag_original_out + delta
			_curve.set_handle_out(index, moved_out)
			if _curve_get_handle_mode(index) == 1:
				var axis_out := (moved_out - _curve_drag_original).normalized()
				_curve.set_handle_in(index, _curve_drag_original - axis_out * _curve_drag_original_in.distance_to(_curve_drag_original))
			else:
				_curve_handle_modes[index] = 0
				if is_instance_valid(_curve_handle_mode_picker):
					_curve_handle_mode_picker.select(0)
		_:
			_curve.set_point(index, _curve_drag_original + delta)
			_curve.set_handle_in(index, _curve_drag_original_in + delta)
			_curve.set_handle_out(index, _curve_drag_original_out + delta)
			if _curve_get_handle_mode(index) == 2 or _curve_get_handle_mode(index) == 3:
				_curve_apply_handle_mode(index, _curve_get_handle_mode(index))
	_curve_sync_overlay()
	_curve_sync_bezier_to_workspace()
	_curve_drag_preview_elapsed = 0.0

func _curve_closest_point_on_drag_axis(ray_origin: Vector3, ray_dir: Vector3) -> Variant:
	# Find the closest point between the camera ray and the selected world-axis line.
	# This avoids the view-dependent distortion caused by projecting an axis-constrained
	# drag onto the normal camera-facing editing plane.
	var axis := Vector3.ZERO
	match _curve_move_axis:
		1:
			axis = Vector3.RIGHT
		2:
			axis = Vector3.UP
		3:
			axis = Vector3.BACK
		_:
			return null
	var direction := ray_dir.normalized()
	var w := ray_origin - _curve_drag_axis_origin
	var b := direction.dot(axis)
	var d := direction.dot(w)
	var e := axis.dot(w)
	var denominator := 1.0 - b * b
	if absf(denominator) < 0.0001:
		# The axis points almost directly into the camera; screen movement cannot
		# determine a stable distance along it from this viewing angle.
		return null
	var ray_t := (b * e - d) / denominator
	var axis_t := (e - b * d) / denominator
	if ray_t < 0.0:
		return null
	return _curve_drag_axis_origin + axis * axis_t


func _curve_end_edit() -> void:
	if not _curve_dragging:
		return
	_curve_dragging = false
	if _curve_selected_handle == 3 or _curve_selected_handle == 4:
		_curve_update_point_radius_control()
	if _curve_preview_enabled:
		_curve_rebuild_preview()
	_curve_sync_overlay()
	_curve_sync_bezier_to_workspace()
	var workspace := get_node_or_null("GeometryNodesWorkspace")
	if is_instance_valid(workspace) and workspace.has_method("finish_viewport_bezier_sync"):
		workspace.call("finish_viewport_bezier_sync")

func _curve_add_point_from_ray(ray_origin: Vector3, ray_dir: Vector3) -> void:
	if _curve == null:
		_curve = VFXCurve.new()
		editor_node.set_active_curve(_curve)
	var placement_plane := Plane(Vector3.UP, cam_target.y)
	var hit: Variant = placement_plane.intersects_ray(ray_origin, ray_dir)
	if hit == null:
		return
	var p: Vector3 = hit
	_curve_append_point(p)
	_curve_selected_point = _curve.get_point_count() - 1
	_curve_selected_handle = 0
	_curve_sync_overlay()
	_curve_update_point_radius_control()
	_curve_update_status()
	if _curve_preview_enabled:
		_curve_rebuild_preview()
	_curve_sync_bezier_to_workspace()

func _curve_append_point(p: Vector3) -> void:
	var previous_count := _curve.get_point_count()
	var new_index := _curve.add_point(p, p, p)
	_curve_handle_modes.append(3)
	_curve_point_radii.append(1.0)
	_curve_point_profiles.append(_curve_profile)
	_curve_point_tilts.append(0.0)
	if previous_count > 0:
		var previous := _curve.get_point(previous_count - 1)
		var delta := p - previous
		var handle_offset := delta / 3.0
		_curve.set_handle_out(previous_count - 1, previous + handle_offset)
		_curve.set_handle_in(new_index, p - handle_offset)

func _curve_undo_point() -> void:
	if _curve == null or _curve.get_point_count() == 0:
		return
	var kept := PackedVector3Array()
	for i in range(_curve.get_point_count() - 1):
		kept.append(_curve.get_point(i))
	_curve_rebuild_from_positions(kept)
	_curve_handle_modes.clear()
	_curve_point_radii.clear()
	_curve_point_profiles.clear()
	_curve_point_tilts.clear()
	for i in range(_curve.get_point_count()):
		_curve_handle_modes.append(3)
		_curve_point_radii.append(1.0)
		_curve_point_profiles.append(_curve_profile)
		_curve_point_tilts.append(0.0)
	_curve_selected_point = mini(_curve_selected_point, _curve.get_point_count() - 1)
	_curve_sync_overlay()
	_curve_sync_bezier_to_workspace()
	_curve_finish_bezier_workspace_sync()
	_curve_update_status()
	if _curve_preview_enabled:
		_curve_rebuild_preview()

func _curve_clear() -> void:
	if _curve == null:
		return
	_curve.clear()
	_curve_handle_modes.clear()
	_curve_point_radii.clear()
	_curve_point_profiles.clear()
	_curve_point_tilts.clear()
	_curve_selected_point = -1
	_curve_dragging = false
	_curve_sync_overlay()
	_curve_update_point_radius_control()
	_curve_remove_preview_node()
	_curve_sync_bezier_to_workspace()
	_curve_finish_bezier_workspace_sync()
	_curve_update_status()

func _curve_finish_bezier_workspace_sync() -> void:
	var workspace := get_node_or_null("GeometryNodesWorkspace")
	if is_instance_valid(workspace) and workspace.has_method("finish_viewport_bezier_sync"):
		workspace.call("finish_viewport_bezier_sync")

func _curve_rebuild_from_positions(positions: PackedVector3Array) -> void:
	_curve = VFXCurve.new()
	_curve_point_radii.clear()
	_curve_point_profiles.clear()
	_curve_point_tilts.clear()
	for p in positions:
		_curve_append_point(p)
	editor_node.set_active_curve(_curve)
	_curve_sync_overlay()

func _curve_set_shape(shape: int) -> void:
	_curve_shape = shape
	if _curve_preview_enabled:
		_curve_rebuild_preview()

func _curve_ensure_stage8_point_data() -> void:
	if _curve == null:
		_curve_point_profiles.clear()
		_curve_point_tilts.clear()
		return
	while _curve_point_profiles.size() < _curve.get_point_count():
		_curve_point_profiles.append(_curve_profile)
	while _curve_point_profiles.size() > _curve.get_point_count():
		_curve_point_profiles.pop_back()
	while _curve_point_tilts.size() < _curve.get_point_count():
		_curve_point_tilts.append(0.0)
	while _curve_point_tilts.size() > _curve.get_point_count():
		_curve_point_tilts.pop_back()

func _curve_set_selected_point_profile(profile_id: int) -> void:
	if _curve == null or _curve_selected_point < 0 or _curve_selected_point >= _curve.get_point_count():
		return
	_curve_ensure_stage8_point_data()
	_curve_point_profiles[_curve_selected_point] = clampi(profile_id, 0, 3)
	if _curve_preview_enabled:
		_curve_rebuild_preview()

func _curve_set_selected_point_tilt(value: float) -> void:
	if _curve == null or _curve_selected_point < 0 or _curve_selected_point >= _curve.get_point_count():
		return
	_curve_ensure_stage8_point_data()
	_curve_point_tilts[_curve_selected_point] = clampf(value, -180.0, 180.0)
	if _curve_preview_enabled:
		_curve_rebuild_preview()

func _curve_taper_multiplier(t: float) -> float:
	var x := clampf(t, 0.0, 1.0)
	match _curve_taper_preset:
		1: # Linear taper from full start to zero end.
			return maxf(0.01, 1.0 - x)
		2: # Pointed at both ends, widest at center.
			return maxf(0.01, sin(PI * x))
		3: # Gentle central bulge.
			return 0.65 + 0.35 * sin(PI * x)
		4: # Pinched center, fuller ends.
			return 0.35 + 0.65 * absf(cos(PI * x))
	return 1.0

func _curve_profile_radius(profile_id: int, angle: float) -> float:
	var c := absf(cos(angle))
	var sn := absf(sin(angle))
	match profile_id:
		1: # Square
			return 1.0 / maxf(0.001, maxf(c, sn))
		2: # Diamond
			return 1.0 / maxf(0.001, c + sn)
		3: # Octagon approximation using an eight-sided radial boundary.
			var sector := PI / 4.0
			var local_angle := fposmod(angle + sector * 0.5, sector) - sector * 0.5
			return cos(sector * 0.5) / maxf(0.001, cos(local_angle))
	return 1.0

func _curve_make_mesh() -> VFXMesh:
	if _curve == null or _curve.get_point_count() < 2:
		return null
	return _curve_make_variable_radius_mesh()

func _curve_make_variable_radius_mesh() -> VFXMesh:
	var point_count := _curve.get_point_count()
	if point_count < 2:
		return null
	_curve_ensure_point_radii()
	_curve_ensure_stage8_point_data()
	var samples_per_span := maxi(2, _curve_longitudinal)
	var sample_count := (point_count - 1) * samples_per_span + 1
	var mesh := VFXMesh.new()
	var centers := PackedVector3Array()
	var tangents := PackedVector3Array()
	var scales := PackedFloat32Array()

	for sample_index in range(sample_count):
		var global_t := float(sample_index) / float(sample_count - 1)
		centers.append(_curve.evaluate(global_t))
		tangents.append(_curve.evaluate_tangent(global_t).normalized())
		var span_float := global_t * float(point_count - 1)
		var span_index := mini(int(floor(span_float)), point_count - 2)
		var local_t := 1.0 if sample_index == sample_count - 1 else span_float - float(span_index)
		var scale := lerpf(_curve_point_radii[span_index], _curve_point_radii[span_index + 1], local_t)
		scales.append(scale * _curve_taper_multiplier(global_t))

	if _curve_shape == 0:
		_curve_build_variable_tube(mesh, centers, tangents, scales)
	else:
		_curve_build_variable_ribbon(mesh, centers, tangents, scales)
	mesh.recalculate_normals()
	return mesh

func _curve_build_variable_tube(mesh: VFXMesh, centers: PackedVector3Array,
		tangents: PackedVector3Array, scales: PackedFloat32Array) -> void:
	# Fixed eight samples around each ring allow smooth transitions between
	# round, square, diamond and octagonal cross-sections along one curve.
	const RING_SEGMENTS := 8
	var sample_count := centers.size()
	var profile_offset := deg_to_rad(_curve_profile_rotation)
	for i in range(sample_count):
		var tangent := tangents[i].normalized()
		if tangent.is_zero_approx():
			tangent = Vector3.FORWARD
		var reference := Vector3.UP
		if absf(tangent.dot(reference)) > 0.92:
			reference = Vector3.RIGHT
		var side := tangent.cross(reference).normalized()
		var normal := side.cross(tangent).normalized()
		var global_t := float(i) / float(sample_count - 1)
		var span_float := global_t * float(_curve.get_point_count() - 1)
		var span := mini(int(floor(span_float)), _curve.get_point_count() - 2)
		var local_t := 1.0 if i == sample_count - 1 else span_float - float(span)
		var p0 := _curve_point_profiles[span]
		var p1 := _curve_point_profiles[span + 1]
		var tilt := deg_to_rad(lerpf(_curve_point_tilts[span], _curve_point_tilts[span + 1], local_t))
		side = side.rotated(tangent, tilt)
		normal = normal.rotated(tangent, tilt)
		var radius := maxf(0.0001, _curve_radius * scales[i])
		for ring in range(RING_SEGMENTS):
			var angle := TAU * float(ring) / float(RING_SEGMENTS) + profile_offset
			var shape_radius := lerpf(_curve_profile_radius(p0, angle), _curve_profile_radius(p1, angle), local_t)
			var radial := (side * cos(angle) + normal * sin(angle)) * shape_radius
			var uv := Vector2(float(ring) / float(RING_SEGMENTS), global_t)
			mesh.add_vertex(centers[i] + radial * radius, uv)

	for i in range(sample_count - 1):
		for ring in range(RING_SEGMENTS):
			var next_ring := (ring + 1) % RING_SEGMENTS
			var a := i * RING_SEGMENTS + ring
			var b := i * RING_SEGMENTS + next_ring
			var c := (i + 1) * RING_SEGMENTS + next_ring
			var d := (i + 1) * RING_SEGMENTS + ring
			mesh.add_quad(a, b, c, d)

	# End caps are independently selectable.
	var start_base := 0
	var end_base := (sample_count - 1) * RING_SEGMENTS
	if _curve_start_cap != 1:
		if _curve_start_cap == 2:
			var start_tip := mesh.add_vertex(centers[0] - tangents[0].normalized() * _curve_radius * scales[0] * 1.5)
			for ring in range(RING_SEGMENTS):
				mesh.add_triangle(start_tip, (ring + 1) % RING_SEGMENTS, ring)
		else:
			var start_center := mesh.add_vertex(centers[0])
			for ring in range(RING_SEGMENTS):
				mesh.add_triangle(start_center, (ring + 1) % RING_SEGMENTS, ring)
	if _curve_end_cap != 1:
		if _curve_end_cap == 2:
			var end_tip := mesh.add_vertex(centers[sample_count - 1] + tangents[sample_count - 1].normalized() * _curve_radius * scales[sample_count - 1] * 1.5)
			for ring in range(RING_SEGMENTS):
				mesh.add_triangle(end_tip, end_base + ring, end_base + (ring + 1) % RING_SEGMENTS)
		else:
			var end_center := mesh.add_vertex(centers[sample_count - 1])
			for ring in range(RING_SEGMENTS):
				mesh.add_triangle(end_center, end_base + ring, end_base + (ring + 1) % RING_SEGMENTS)

func _curve_build_variable_ribbon(mesh: VFXMesh, centers: PackedVector3Array,
		tangents: PackedVector3Array, scales: PackedFloat32Array) -> void:
	var sample_count := centers.size()
	var previous_side := Vector3.ZERO
	var tilt_radians := deg_to_rad(_curve_ribbon_tilt)
	for i in range(sample_count):
		var tangent := tangents[i].normalized()
		if tangent.is_zero_approx():
			tangent = Vector3.FORWARD

		var side := Vector3.ZERO
		if _curve_ribbon_orientation == 1:
			# World Y-Up: width is horizontal (perpendicular to world up),
			# so a mostly horizontal path produces a floor-like ribbon.
			var world_up := Vector3.UP
			if absf(tangent.dot(world_up)) > 0.985:
				world_up = Vector3.RIGHT
			side = tangent.cross(world_up).normalized()
		elif _curve_ribbon_orientation == 2:
			# World Vertical: project world up onto the plane perpendicular
			# to the tangent. This makes the ribbon's width rise vertically,
			# producing a wall-like strip independent of the camera.
			var world_up := Vector3.UP
			side = world_up - tangent * world_up.dot(tangent)
			if side.length_squared() < 0.0001:
				# At a vertical tangent, world up cannot define a width axis.
				# Preserve the previous frame where possible to avoid a 180-degree
				# snap; use a fixed horizontal axis only at the first sample.
				side = previous_side - tangent * previous_side.dot(tangent)
				if side.length_squared() < 0.0001:
					var fallback := Vector3.RIGHT
					side = fallback - tangent * fallback.dot(tangent)
			side = side.normalized()
			if not previous_side.is_zero_approx() and side.dot(previous_side) < 0.0:
				side = -side
		else:
			# Parallel transport minimizes unwanted twist and reduces frame flipping.
			if i == 0 or previous_side.is_zero_approx():
				var reference := Vector3.UP
				if absf(tangent.dot(reference)) > 0.92:
					reference = Vector3.RIGHT
				side = tangent.cross(reference).normalized()
			else:
				side = previous_side - tangent * previous_side.dot(tangent)
				if side.length_squared() < 0.000001:
					var fallback := Vector3.UP
					if absf(tangent.dot(fallback)) > 0.92:
						fallback = Vector3.RIGHT
					side = tangent.cross(fallback).normalized()
				else:
					side = side.normalized()
				if side.dot(previous_side) < 0.0:
					side = -side

		if not side.is_zero_approx() and absf(tilt_radians) > 0.0001:
			side = side.rotated(tangent, tilt_radians).normalized()
		previous_side = side

		var global_t := float(i) / float(sample_count - 1)
		var span_float := global_t * float(_curve.get_point_count() - 1)
		var span := mini(int(floor(span_float)), _curve.get_point_count() - 2)
		var local_t := 1.0 if i == sample_count - 1 else span_float - float(span)
		var point_tilt := deg_to_rad(lerpf(_curve_point_tilts[span], _curve_point_tilts[span + 1], local_t))
		if not side.is_zero_approx() and absf(point_tilt) > 0.0001:
			side = side.rotated(tangent, point_tilt).normalized()
		var half_width := maxf(0.0001, _curve_width * scales[i] * 0.5)
		var v := global_t
		mesh.add_vertex(centers[i] - side * half_width, Vector2(0.0, v))
		mesh.add_vertex(centers[i] + side * half_width, Vector2(1.0, v))
	for i in range(sample_count - 1):
		var a := i * 2
		mesh.add_quad(a, a + 1, a + 3, a + 2)

func _curve_ensure_point_radii() -> void:
	if _curve == null:
		_curve_point_radii.clear()
		return
	while _curve_point_radii.size() < _curve.get_point_count():
		_curve_point_radii.append(1.0)
	while _curve_point_radii.size() > _curve.get_point_count():
		_curve_point_radii.pop_back()

func _curve_update_point_radius_control() -> void:
	if not is_instance_valid(_curve_point_radius_slider):
		return
	_curve_ensure_point_radii()
	var valid_selection := _curve != null and _curve_selected_point >= 0 and _curve_selected_point < _curve.get_point_count()
	_curve_point_radius_slider.editable = valid_selection
	if valid_selection:
		_curve_ensure_stage8_point_data()
		var value := _curve_point_radii[_curve_selected_point]
		_curve_point_radius_slider.set_value_no_signal(value)
		if is_instance_valid(_curve_profile_at_point_picker):
			_curve_profile_at_point_picker.select(_curve_point_profiles[_curve_selected_point])
		_curve_point_radius_value.text = "%.2fx" % value
	else:
		_curve_point_radius_slider.set_value_no_signal(1.0)
		_curve_point_radius_value.text = "Select a point"

func _curve_set_selected_point_radius(value: float) -> void:
	if _curve == null or _curve_selected_point < 0 or _curve_selected_point >= _curve.get_point_count():
		return
	_curve_ensure_point_radii()
	_curve_point_radii[_curve_selected_point] = clampf(value, 0.01, 10.0)
	if is_instance_valid(_curve_point_radius_value):
		_curve_point_radius_value.text = "%.2fx" % value
	# Rebuild on slider release for mobile performance; this updates status immediately.
	_curve_update_status()

func _curve_get_or_create_mesh_node() -> VFXSceneNode:
	if is_instance_valid(_curve_mesh_node):
		return _curve_mesh_node
	var scene: VFXScene = editor_node.get_scene()
	if scene == null:
		return null
	var root: VFXSceneNode = scene.get_root()
	if root == null:
		return null
	_curve_mesh_node = scene.create_node("CurveMesh", VFXSceneNode.NODE_MESH, root)
	if _curve_mesh_node == null:
		return null
	scene_tree.force_rebuild()
	return _curve_mesh_node

func _curve_apply_generated_mesh(generated: VFXMesh) -> bool:
	if generated == null:
		return false
	var mesh_node := _curve_get_or_create_mesh_node()
	if mesh_node == null:
		return false
	# Keep the generated object separate from the user's currently edited mesh.
	# Preview updates only replace this dedicated CurveMesh node's geometry.
	mesh_node.set_mesh(generated)
	editor_node.set_active_scene_node(mesh_node)
	editor_node.refresh_mesh()
	scene_tree.force_rebuild()
	return true

func _curve_remove_preview_node() -> void:
	if not is_instance_valid(_curve_mesh_node):
		return
	var scene: VFXScene = editor_node.get_scene()
	if scene:
		if editor_node.get_active_scene_node() == _curve_mesh_node:
			editor_node.set_active_scene_node(null)
		scene.delete_node(_curve_mesh_node)
		scene_tree.force_rebuild()
	_curve_mesh_node = null

func _curve_rebuild_preview() -> void:
	var generated := _curve_make_mesh()
	if generated == null:
		if _curve != null and _curve.get_point_count() < 2:
			_curve_remove_preview_node()
		_curve_update_status()
		return
	if not _curve_apply_generated_mesh(generated):
		_curve_update_status("Could not create preview node")
		return
	_curve_update_status()

func _curve_build_mesh() -> void:
	var generated := _curve_make_mesh()
	if generated == null:
		_curve_update_status("Add at least 2 points")
		return
	if not _curve_apply_generated_mesh(generated):
		_curve_update_status("Could not create mesh node")
		return
	_frame_mesh()
	_curve_update_status("Mesh generated")

func _curve_update_status(prefix: String = "") -> void:
	if not is_instance_valid(_curve_status):
		return
	var count := _curve.get_point_count() if _curve != null else 0
	var mode := "Tube" if _curve_shape == 0 else "Ribbon"
	if _curve_shape == 0:
		var profile_names := ["Round", "Square", "Diamond", "Octagon"]
		mode += " · " + profile_names[clampi(_curve_profile, 0, profile_names.size() - 1)]
	var state := "%d points · %s" % [count, mode]
	if _curve_add_mode:
		state += " · tap viewport to place"
	elif count >= 2:
		state += " · drag green/purple end grips to taper"
	if prefix != "":
		state = prefix + " · " + state
	_curve_status.text = state


func _on_edit_mode_selected(mode: String):
	_modeling_edit_mode = mode
	match mode:
		"Object":
			editor_node.set_edit_mode(VFXEditorNode.MODE_OBJECT)
			editor_node.set_show_wireframe(false)
		"Vertex":
			editor_node.set_edit_mode(VFXEditorNode.MODE_VERTEX)
			editor_node.set_show_wireframe(true)
		"Edge":
			editor_node.set_edit_mode(VFXEditorNode.MODE_EDGE)
			editor_node.set_show_wireframe(true)
		"Face":
			editor_node.set_edit_mode(VFXEditorNode.MODE_FACE)
			editor_node.set_show_wireframe(true)


# ------------------------------------------------------------------
# TEXTURE PAINTING
# ------------------------------------------------------------------
func _teardown_texture_painting():
	if is_texture_painting:
		is_texture_painting = false
		texture_painter.stroke_end()
	pending_paint_pos = Vector2(-1, -1)
	_paint_preview_timer = 0.0
	_flush_paint_preview()
	editor_node.set_paint_material(null)
	if brush_cursor:
		brush_cursor.visible = false

func _ensure_paint_canvas():
	# Lazy, once-per-session 1024px canvas (good mobile balance)
	if texture_painter.get_texture() == null:
		texture_painter.create_texture(1024, 1024, Color(1, 1, 1, 1))
	var img: Image = texture_painter.get_texture()
	if img == null:
		return
	# Reuse one ImageTexture. Never call get_image_texture() per stroke:
	# it reallocates AND re-uploads the full texture every time.
	if paint_texture == null or paint_texture.get_size() != Vector2(img.get_size()):
		paint_texture = ImageTexture.create_from_image(img)
	if paint_material == null:
		paint_material = StandardMaterial3D.new()
		paint_material.roughness = 0.85
		paint_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	paint_material.albedo_texture = paint_texture

func _flush_paint_preview():
	if paint_texture and texture_painter.get_texture():
		texture_painter.apply_to_image_texture(paint_texture)

func _queue_texture_paint(screen_pos: Vector2):
	# Buffer only the LATEST position; drag events can outrun the frame rate
	pending_paint_pos = screen_pos

func _do_texture_paint():
	if pending_paint_pos.x < 0:
		return
	var event_pos := _viewport_event_pos(pending_paint_pos)
	var ray_origin := camera.project_ray_origin(event_pos)
	var ray_dir := camera.project_ray_normal(event_pos)
	# FIX: if the scene raycast misses, don't paint with a stale/wrong transform
	if not _sync_painter_to_hit(ray_origin, ray_dir):
		pending_paint_pos = Vector2(-1, -1)
		return
	# 3D-filtered stamping: brush radius/position are interpreted in screen
	# space, so they must track the camera every frame
	texture_painter.set_camera_transform(camera.global_transform)
	texture_painter.paint_at(ray_origin, ray_dir, event_pos)
	pending_paint_pos = Vector2(-1, -1)

	if brush_cursor:
		brush_cursor.visible = true
		# Screen-locked cursor size (same math the C++ gizmo scaling uses)
		var screen_px: float = clampf(texture_painter.get_brush_radius() * 0.6, 6.0, 160.0)
		var vp_h: float = maxf(float(sub_viewport.size.y), 1.0)
		var world_r: float = cam_distance * tan(deg_to_rad(camera.fov) * 0.5) * 2.0 * (screen_px / vp_h)
		var cursor_pos: Vector3 = ray_origin + ray_dir * cam_distance
		if brush_cursor is Node3D:
			brush_cursor.global_position = cursor_pos
			brush_cursor.scale = Vector3(world_r, world_r, world_r)
		elif brush_cursor is CanvasItem:
			brush_cursor.global_position = camera.unproject_position(cursor_pos)
			brush_cursor.scale = Vector2(world_r, world_r)

func _setup_texture_painting():
	_ensure_paint_canvas()
	_flush_paint_preview()
	editor_node.set_show_weights(false)
	editor_node.set_paint_material(paint_material)
	texture_painter.set_viewport_size(Vector2(sub_viewport.size))
	texture_painter.set_camera_fov(camera.fov)

	var col := VBoxContainer.new()
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(col)

	var row1 := HBoxContainer.new()
	row1.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_child(row1)

	var color_picker := ColorPickerButton.new()
	color_picker.text = "Color"
	color_picker.custom_minimum_size = Vector2(72, 48)   # touch-friendly
	color_picker.color = texture_painter.get_brush_color()
	color_picker.color_changed.connect(texture_painter.set_brush_color)
	row1.add_child(color_picker)

	var mode_opt := OptionButton.new()
	mode_opt.add_item("Paint")
	mode_opt.add_item("Erase")
	mode_opt.add_item("Smudge")
	mode_opt.selected = texture_painter.get_brush_mode()
	mode_opt.item_selected.connect(texture_painter.set_brush_mode)
	row1.add_child(mode_opt)

	var top_check := CheckButton.new()
	top_check.text = "Top"
	top_check.button_pressed = texture_painter.get_paint_top_only()
	top_check.toggled.connect(texture_painter.set_paint_top_only)
	row1.add_child(top_check)

	var row2 := HBoxContainer.new()
	row2.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_child(row2)
	_add_paint_slider(row2, "Radius", 1.0, 128.0, 1.0, texture_painter.get_brush_radius(), _on_brush_radius_changed)
	_add_paint_slider(row2, "Hard", 0.0, 1.0, 0.01, texture_painter.get_brush_hardness(), texture_painter.set_brush_hardness)
	_add_paint_slider(row2, "Opacity", 0.0, 1.0, 0.01, texture_painter.get_brush_opacity(), texture_painter.set_brush_opacity)

	var row3 := HBoxContainer.new()
	row3.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_child(row3)

	var clear_btn := Button.new()
	clear_btn.text = "Clear"
	clear_btn.pressed.connect(_on_clear_texture_pressed)
	row3.add_child(clear_btn)

	var export_btn := Button.new()
	export_btn.text = "Export"
	export_btn.pressed.connect(_on_export_texture_pressed)
	row3.add_child(export_btn)

	var shade_row := HBoxContainer.new()
	shade_row.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(shade_row)
	var shade_group := ButtonGroup.new()
	for mode in ["Solid", "Wire"]:
		var btn := Button.new()
		btn.text = mode
		btn.toggle_mode = true
		btn.button_group = shade_group
		btn.pressed.connect(_on_texture_shade_selected.bind(mode))
		shade_row.add_child(btn)
		if mode == "Solid":
			btn.button_pressed = true

func _on_brush_radius_changed(v: float) -> void:
	# Legacy texture-space radius + new screen-space radius (kept in sync with
	# the brush cursor math below: screen_px = radius * 0.6)
	texture_painter.set_brush_radius(v)
	texture_painter.set_brush_screen_radius(clampf(v * 0.6, 6.0, 160.0))

func _add_paint_slider(parent: Control, label_text: String, min_v: float, max_v: float, step: float, value: float, setter: Callable) -> HSlider:
	var box := VBoxContainer.new()
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	parent.add_child(box)
	var label := Label.new()
	label.text = label_text
	box.add_child(label)
	var slider := HSlider.new()
	slider.min_value = min_v
	slider.max_value = max_v
	slider.step = step
	slider.value = value
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.value_changed.connect(setter)
	box.add_child(slider)
	return slider

func _on_texture_shade_selected(mode: String):
	match mode:
		"Solid":
			editor_node.set_show_wireframe(false)
		"Wire":
			editor_node.set_show_wireframe(true)
	editor_node.refresh_mesh()

func _on_clear_texture_pressed():
	texture_painter.clear_texture(Color(1, 1, 1, 1))
	_flush_paint_preview()

func _on_export_texture_pressed():
	_flush_paint_preview()
	_route_save_dialog(_on_texture_save_path_selected, "*.jpg", "JPEG Image", "Export Texture", "painted_texture.jpg")

func _on_texture_save_path_selected(path: String):
	if not texture_painter.export_jpg(path):
		push_error("Failed to export texture: " + path)



func _on_mesh_changed(mesh):
	if mesh != null:
		uv_viewport.set_mesh(mesh)
		var vert_label = get_node_or_null("%VerticeLabel")
		if vert_label:
			vert_label.text = "Vertices : " + str(mesh.get_live_vertex_count())
		var tris_label = get_node_or_null("%TrisLabel")
		if tris_label:
			tris_label.text = "Triangles : " + str(mesh.get_live_face_count())


func _update_stats_label() -> void:
	# Called from _process at STATS_INTERVAL — do NOT call every frame:
	# a Label text change inside a Container invalidates min-size and
	# relayouts + redraws the whole top bar.
	var stats_label = get_node_or_null("%StatsLabel")
	if stats_label == null:
		return
	var fps := Performance.get_monitor(Performance.TIME_FPS)
	var calls := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
	stats_label.text = "FPS : %d   Draw Calls : %d" % [int(fps), int(calls)]


func _on_axis_gizmo_snap(dir: Vector3):
	# dir = where the camera should sit relative to cam_target
	cam_yaw = atan2(dir.x, dir.z)
	cam_pitch = clamp(asin(clamp(dir.y, -1.0, 1.0)), -PI/2 + 0.05, PI/2 - 0.05)
	_update_camera_transform()
	_cam_dirty = false

func _on_axis_gizmo_orbit(relative: Vector2):
	cam_yaw -= relative.x * 0.01
	cam_pitch = clamp(cam_pitch + relative.y * 0.01, -PI/2 + 0.05, PI/2 - 0.05)
	_cam_dirty = true

# ------------------------------------------------------------------
# RETOPOLOGY
# ------------------------------------------------------------------
func _teardown_retopology():
	_retopo_buttons.clear()
	if is_instance_valid(retopo_progress):
		retopo_progress = null
	if is_instance_valid(retopo_status):
		retopo_status = null
	if is_instance_valid(retopo_info):
		retopo_info = null

func _setup_retopology():
	var mesh := editor_node.get_vfx_mesh()
	var faces := mesh.get_live_face_count() if mesh else 0

	var col := VBoxContainer.new()
	col.alignment = BoxContainer.ALIGNMENT_CENTER
	bottom_panel.add_child(col)

	# Info line
	retopo_info = Label.new()
	retopo_info.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	col.add_child(retopo_info)
	_update_retopo_info()

	# Progress + status (visible only while a job runs)
	retopo_progress = ProgressBar.new()
	retopo_progress.min_value = 0
	retopo_progress.max_value = 100
	retopo_progress.custom_minimum_size = Vector2(200, 18)
	retopo_progress.visible = false
	col.add_child(retopo_progress)

	retopo_status = Label.new()
	retopo_status.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	retopo_status.visible = false
	col.add_child(retopo_status)

	if mesh == null or faces == 0:
		var no_mesh := Label.new()
		no_mesh.text = "No mesh to retopologize"
		col.add_child(no_mesh)
		return

	_retopo_target_faces = clampi(_retopo_target_faces, 10, faces)

	# --- Parameters ---
	var grid := GridContainer.new()
	grid.columns = 2
	col.add_child(grid)

	_add_retopo_slider(grid, "Target Faces", 10, max(faces, 11), 1, _retopo_target_faces,
		func(v): _retopo_target_faces = int(v))
	_add_retopo_slider(grid, "Edge Len", 0.005, 0.2, 0.005, _retopo_edge_length,
		func(v): _retopo_edge_length = v)
	_add_retopo_slider(grid, "Feature °", 10, 90, 1, _retopo_feature_angle,
		func(v): _retopo_feature_angle = v)
	_add_retopo_slider(grid, "Iterations", 1, 10, 1, _retopo_iterations,
		func(v): _retopo_iterations = int(v))

	var checks := HBoxContainer.new()
	checks.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_child(checks)

	var b_check := CheckButton.new()
	b_check.text = "Boundary"
	b_check.button_pressed = _retopo_preserve_boundary
	b_check.toggled.connect(func(v): _retopo_preserve_boundary = v)
	checks.add_child(b_check)

	var f_check := CheckButton.new()
	f_check.text = "Features"
	f_check.button_pressed = _retopo_preserve_features
	f_check.toggled.connect(func(v): _retopo_preserve_features = v)
	checks.add_child(f_check)

	# --- Operations ---
	var row := HBoxContainer.new()
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	col.add_child(row)

	_add_retopo_button(row, "Retopo", "retopologize")
	_add_retopo_button(row, "Remesh", "remesh")
	_add_retopo_button(row, "Simplify", "simplify")
	_add_retopo_button(row, "Decimate", "decimate")
	_add_retopo_button(row, "Relax", "relax")

func _update_retopo_info():
	if not is_instance_valid(retopo_info):
		return
	var mesh := editor_node.get_vfx_mesh()
	if mesh:
		retopo_info.text = "Faces: %d   Verts: %d" % [mesh.get_live_face_count(), mesh.get_live_vertex_count()]
	else:
		retopo_info.text = "No mesh"

func _add_retopo_slider(parent: Control, label_text: String, min_v: float, max_v: float, step: float, value: float, setter: Callable) -> HSlider:
	var label := Label.new()
	label.text = label_text
	parent.add_child(label)
	var slider := HSlider.new()
	slider.min_value = min_v
	slider.max_value = max_v
	slider.step = step
	slider.value = value
	slider.custom_minimum_size = Vector2(140, 32)
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.value_changed.connect(setter)
	parent.add_child(slider)
	return slider

func _add_retopo_button(parent: Control, text: String, op: String) -> Button:
	var btn := Button.new()
	btn.text = text
	btn.custom_minimum_size = Vector2(0, 48)   # touch target
	btn.pressed.connect(_start_retopo_job.bind(op))
	parent.add_child(btn)
	_retopo_buttons.append(btn)
	return btn

func _set_retopo_ui_running(running: bool):
	for btn in _retopo_buttons:
		if is_instance_valid(btn):
			btn.disabled = running
	if is_instance_valid(retopo_progress):
		retopo_progress.visible = running
		if running:
			retopo_progress.value = 0.0
	if is_instance_valid(retopo_status):
		retopo_status.visible = running
		if running:
			retopo_status.text = "Working…"

func _duplicate_vfx_mesh(mesh: VFXMesh) -> VFXMesh:
	# serialize()/deserialize() are stubs in this build, so rebuild from arrays.
	# cleanup() first so no dead vertices/faces leak into the copy.
	#editor_node.mesh_cleanup()
	var copy := VFXMesh.new()
	copy.set_material(mesh.get_material())
	var positions := mesh.get_positions()
	var normals := mesh.get_normals()
	var uvs := mesh.get_uvs()
	var colors := mesh.get_colors()
	var has_uv := uvs.size() == positions.size()
	var has_col := colors.size() == positions.size()
	for i in positions.size():
		copy.add_vertex(positions[i],
			uvs[i] if has_uv else Vector2(),
			colors[i] if has_col else Color(1, 1, 1, 1))
	var indices := mesh.get_indices()
	var i := 0
	while i + 2 < indices.size():
		copy.add_triangle(indices[i], indices[i + 1], indices[i + 2])
		i += 3
	if normals.size() == positions.size():
		for j in positions.size():
			copy.set_vertex_normal(j, normals[j])
	return copy

func _start_retopo_job(op: String):
	if _retopo_task_id != -1:
		return                                  # one job at a time
	var src := editor_node.get_vfx_mesh()
	if src == null or src.get_live_face_count() == 0:
		return

	var work := _duplicate_vfx_mesh(src)      # owned ONLY by the worker from here on
	if work.get_vertex_count() == 0:
		return

	_retopo_src_faces = src.get_live_face_count()
	_retopo_op = op
	_retopo_ok = false

	retopo = VFXRetopology.new()
	retopo.set_mesh(work)
	retopo.set_feature_angle(_retopo_feature_angle)
	retopo.set_target_edge_length(_retopo_edge_length)
	retopo.set_preserve_boundary(_retopo_preserve_boundary)
	retopo.set_preserve_features(_retopo_preserve_features)
	retopo.set_max_iterations(_retopo_iterations)

	_set_retopo_ui_running(true)
	_retopo_task_id = WorkerThreadPool.add_task(_run_retopo_job)

func _run_retopo_job():
	# Runs on a WorkerThreadPool thread. Only the copy mesh is touched;
	# the rendered original is never accessed here.
	match _retopo_op:
		"retopologize":
			_retopo_ok = retopo.retopologize(_retopo_target_faces)
		"remesh":
			_retopo_ok = retopo.remesh_uniform(_retopo_edge_length, _retopo_iterations)
		"simplify":
			_retopo_ok = retopo.simplify_quad_dominant(_retopo_target_faces)
		"decimate":
			_retopo_ok = retopo.decimate_preserve_topology(_retopo_target_faces)
		"relax":
			retopo.relax_vertices(_retopo_iterations)
			_retopo_ok = true
	var result := retopo.get_mesh()
	if result and _retopo_ok:
		result.recalculate_normals()
		result.recalculate_bounds()

func _on_retopo_finished():
	var new_mesh: VFXMesh = retopo.get_mesh() if retopo else null
	retopo = null
	_set_retopo_ui_running(false)

	if new_mesh == null or new_mesh.get_live_vertex_count() == 0 or not _retopo_ok:
		if is_instance_valid(retopo_status):
			retopo_status.visible = true
			retopo_status.text = "Retopo failed"
		return

	# Adopt the result
	var old_mesh := editor_node.get_vfx_mesh()
	var active_node := editor_node.get_active_scene_node()
	if active_node:
		active_node.set_mesh(new_mesh)
	editor_node.set_vfx_mesh(new_mesh)      # C++ syncs painter + skin mesh refs
	uv_viewport.set_mesh(new_mesh)

	# Topology changed -> old skin weights are meaningless
	if skin and old_mesh and new_mesh.get_vertex_count() != old_mesh.get_vertex_count():
		skin.clear_weights()
		if skeleton:
			_capture_bind_pose_if_needed()
			skin.auto_weight_from_bones(4)

	_frame_mesh()
	editor_node.refresh_mesh()
	_update_retopo_info()


func _sync_painter_to_hit(ray_origin: Vector3, ray_dir: Vector3) -> bool:
	var node: VFXSceneNode = editor_node.raycast_scene_node(ray_origin, ray_dir)
	if node == null or node.get_mesh() == null:
		return false
	if editor_node.get_vfx_mesh() != node.get_mesh():
		editor_node.set_vfx_mesh(node.get_mesh())   # C++ auto-syncs painter mesh
	texture_painter.set_mesh_transform(node.get_global_transform())
	return true

func _prepare_bone_context(select_first: bool = true) -> void:
	BoneList.clear()
	if skeleton == null or skeleton.get_bone_count() == 0:
		return

	for i in skeleton.get_bone_count():
		BoneList.add_item(
			skeleton.get_bone_name(i),
			preload("res://icons/bone.png"),
			true
		)

	if select_first \
	or current_bone_idx < 0 \
	or current_bone_idx >= skeleton.get_bone_count():
		current_bone_idx = 0

	BoneList.select(current_bone_idx)
	_on_bone_selected(current_bone_idx)
