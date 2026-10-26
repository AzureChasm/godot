extends Node3D

# UI/camera glue. All terrain profiles, sampling, meshes and collision use C++.
const REGION_NAMES := ["Plains", "Hills", "Mountains", "Sea", "DeepSea"]
const DISPLAY_NAMES := ["Plains", "Hills", "Mountains", "Sea", "Deep sea"]
@onready var terrain: TerrainView = $TerrainView
@onready var camera: Camera3D = $Camera3D
var selected := -1
var target := Vector3.ZERO
var yaw := 0.16
var pitch := 0.85
var distance := 30.0
var metrics: Label
var selected_label: Label
var controls: Dictionary = {}
var water_guides: Node3D

func _ready() -> void:
	_update_camera()
	_build_labels_and_water()
	_build_controls()
	terrain.preview_generated.connect(_show_statistics)
	_show_statistics(terrain.get_statistics())
	_select(-1)
	if "--capture" in OS.get_cmdline_user_args():
		var deadline := Time.get_ticks_msec() + 60000
		while terrain.get_statistics().is_empty() or terrain.is_generating():
			if Time.get_ticks_msec() > deadline:
				get_tree().quit(1)
				return
			await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/tmp/world-generator-terrain-gallery.png")
		get_tree().quit()

func _build_labels_and_water() -> void:
	# Keep labels and guides outside World: its child tree contains containers only.
	water_guides = Node3D.new()
	add_child(water_guides)
	for i in range(REGION_NAMES.size()):
		var region: RegionContainer = $World.get_node(REGION_NAMES[i])
		var label := Label3D.new()
		label.text = DISPLAY_NAMES[i]
		label.position = region.position + Vector3(0, 4.4, 0)
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.no_depth_test = true
		label.font_size = 64
		label.pixel_size = 0.018
		label.outline_size = 12
		add_child(label)
		if i >= 3:
			var guide := MeshInstance3D.new()
			var plane := PlaneMesh.new()
			plane.size = Vector2(8, 8)
			guide.mesh = plane
			guide.position = Vector3(region.position.x, 0, region.position.z)
			var water := StandardMaterial3D.new()
			water.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			water.albedo_color = Color(0.07, 0.4, 0.65, 0.28)
			water.roughness = 0.25
			water.cull_mode = BaseMaterial3D.CULL_DISABLED
			guide.material_override = water
			water_guides.add_child(guide)

func _build_controls() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var panel := PanelContainer.new()
	panel.position = Vector2(16, 16)
	canvas.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "top", "right", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 12)
	panel.add_child(margin)
	var box := VBoxContainer.new()
	margin.add_child(box)
	var title := Label.new()
	title.text = "WORLD GENERATOR / Terrain gallery"
	title.add_theme_font_size_override("font_size", 21)
	box.add_child(title)
	var row := HBoxContainer.new()
	box.add_child(row)
	var all_button := Button.new()
	all_button.text = "0: All"
	all_button.pressed.connect(_select.bind(-1))
	row.add_child(all_button)
	for i in range(DISPLAY_NAMES.size()):
		var button := Button.new()
		button.text = "%d: %s" % [i + 1, DISPLAY_NAMES[i]]
		button.pressed.connect(_select.bind(i))
		row.add_child(button)
	selected_label = Label.new()
	box.add_child(selected_label)
	var fields := HBoxContainer.new()
	box.add_child(fields)
	_add_field(fields, "elevation", "Elevation", -5, 5, 0.05)
	_add_field(fields, "amplitude", "Relief", 0, 5, 0.05)
	_add_field(fields, "wavelength", "Wavelength", 0.25, 30, 0.25)
	_add_field(fields, "seed", "Seed", -2147483648, 2147483647, 1)
	var actions := HBoxContainer.new()
	box.add_child(actions)
	var generate := Button.new()
	generate.text = "Generate (R)"
	generate.pressed.connect(_regenerate)
	actions.add_child(generate)
	var water := CheckButton.new()
	water.text = "Water-level guide (W)"
	water.button_pressed = true
	water.toggled.connect(func(enabled: bool) -> void: water_guides.visible = enabled)
	actions.add_child(water)
	var cave := CheckButton.new()
	cave.text = "Carve mountain cave (C)"
	cave.toggled.connect(func(enabled: bool) -> void:
		$World.decorators[0].enabled = enabled
		_regenerate())
	actions.add_child(cave)
	controls["water_toggle"] = water
	controls["cave_toggle"] = cave
	metrics = Label.new()
	box.add_child(metrics)
	var help := Label.new()
	help.text = "Right-drag: orbit · Wheel: zoom · Select a profile, adjust values, then Generate. Sea is seabed + visual guide; no fluid simulation."
	help.add_theme_font_size_override("font_size", 13)
	box.add_child(help)

func _add_field(row: HBoxContainer, property: String, title: String, minimum: float, maximum: float, step: float) -> void:
	var column := VBoxContainer.new()
	row.add_child(column)
	var label := Label.new()
	label.text = title
	column.add_child(label)
	var spin := SpinBox.new()
	spin.min_value = minimum
	spin.max_value = maximum
	spin.step = step
	spin.custom_minimum_size.x = 125 if property == "seed" else 115
	spin.value_changed.connect(_change_value.bind(property))
	column.add_child(spin)
	controls[property] = spin

func _select(index: int) -> void:
	selected = index
	var profile: WorldTerrainDecorator
	if index >= 0:
		var region: RegionContainer = $World.get_node(REGION_NAMES[index])
		profile = region.decorators[0]
		target = region.position + Vector3(0, profile.elevation, 0)
		distance = 15.0
		selected_label.text = DISPLAY_NAMES[index] + " — " + profile.get_class() + " (adjustments are runtime-only)"
	else:
		target = Vector3.ZERO
		distance = 30.0
		selected_label.text = "Select a terrain type to adjust its resource. All five are editable under World in the Inspector."
	for property in ["elevation", "amplitude", "wavelength", "seed"]:
		var spin: SpinBox = controls[property]
		spin.editable = profile != null
		if profile:
			spin.set_value_no_signal(profile.get(property))
	_update_camera()

func _change_value(value: float, property: String) -> void:
	if selected < 0:
		return
	var region: RegionContainer = $World.get_node(REGION_NAMES[selected])
	region.decorators[0].set(property, int(value) if property == "seed" else value)
	selected_label.text = DISPLAY_NAMES[selected] + " — changed; press Generate (R)"

func _regenerate() -> void:
	if terrain.generate_preview():
		if selected >= 0:
			selected_label.text = DISPLAY_NAMES[selected] + " — queued on workers"
	else:
		metrics.text = terrain.status

func _show_statistics(stats: Dictionary) -> void:
	if not stats.is_empty() and selected >= 0:
		selected_label.text = DISPLAY_NAMES[selected] + " — generated"
	metrics.text = "Generating..." if stats.is_empty() else "%d world chunks (16 m) · %d sections · %d triangles · %.0f ms" % [stats.chunk_count, stats.section_count, stats.triangles, stats.generation_ms]

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		yaw -= event.relative.x * 0.006
		pitch = clampf(pitch + event.relative.y * 0.006, 0.1, 1.45)
		_update_camera()
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(3, distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(70, distance / 0.9)
		_update_camera()
	elif event is InputEventKey and event.pressed and not event.echo:
		if event.keycode >= KEY_0 and event.keycode <= KEY_5:
			_select(event.keycode - KEY_1)
		elif event.keycode == KEY_R:
			_regenerate()
		elif event.keycode == KEY_W:
			controls["water_toggle"].button_pressed = not controls["water_toggle"].button_pressed
		elif event.keycode == KEY_C:
			controls["cave_toggle"].button_pressed = not controls["cave_toggle"].button_pressed

func _update_camera() -> void:
	camera.position = target + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(target)
