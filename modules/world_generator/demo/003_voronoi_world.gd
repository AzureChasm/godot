extends Node3D

# Demo controls only. Allocation, noise, transitions, field and meshes are C++.
const BIOMES := ["Plains", "Hills", "Mountains", "Sea", "DeepSea"]
@onready var world: WorldRegionLayout = $World
@onready var terrain: TerrainView = $TerrainView
@onready var camera: Camera3D = $Camera3D
# Camera focus and terrain residency are intentionally separate world positions.
var center := Vector3.ZERO
var preview_center := Vector3.ZERO
var refresh_timer: Timer
var map_view: TextureRect
var map_panel: VBoxContainer
var progress_bar: ProgressBar
var progress_label: Label
var requested_area := AABB()
var map_token := 0
var map_ready_revision := 0
var yaw := 0.3
var pitch := 0.85
var distance := 77.0
var metrics: Label
var message: Label
var camera_status: Label
var controls: Dictionary = {}
var guides: Node3D
var water: MeshInstance3D
var guide_token := 0
var grid_visible := true
var preview_height := 32.0
var camera_horizontal_offset := -13.0
var capture_path := "/tmp/worldgen-unbounded.png"

func _ready() -> void:
	_build_ui()
	_build_water()
	refresh_timer = Timer.new()
	refresh_timer.one_shot = true
	refresh_timer.wait_time = 0.3
	refresh_timer.timeout.connect(_refresh_if_needed)
	add_child(refresh_timer)
	_update_camera()
	terrain.preview_generated.connect(_generated)
	terrain.generation_started.connect(_generation_started)
	terrain.generation_failed.connect(_generation_failed)
	terrain.generation_cancelled.connect(_generation_cancelled)
	get_viewport().size_changed.connect(_resize_ui)
	_resize_ui()
	if "--capture" in OS.get_cmdline_user_args():
		var deadline := Time.get_ticks_msec() + 120000
		while _terrain_statistics().is_empty() or terrain.is_generating() or map_ready_revision != _terrain_statistics().get("revision", 0):
			if Time.get_ticks_msec() > deadline:
				push_error("Capture timed out: " + terrain.status)
				get_tree().quit(1)
				return
			await get_tree().process_frame
		await RenderingServer.frame_post_draw
		DirAccess.make_dir_recursive_absolute(capture_path.get_base_dir())
		var capture_error := get_viewport().get_texture().get_image().save_png(capture_path)
		get_tree().quit(0 if capture_error == OK else 1)

func _build_ui() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var panel := PanelContainer.new()
	canvas.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_LEFT_WIDE)
	panel.offset_left = 12
	panel.offset_top = 12
	panel.offset_right = 398
	panel.offset_bottom = -12
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	panel.add_child(scroll)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 10)
	scroll.add_child(margin)
	var box := VBoxContainer.new()
	margin.add_child(box)
	var title := Label.new()
	title.text = _demo_title()
	title.add_theme_font_size_override("font_size", 20)
	box.add_child(title)
	_build_extra_controls(box)
	var weights := HBoxContainer.new()
	box.add_child(weights)
	for index in range(_biome_names().size()):
		if index > 0 and index % 3 == 0:
			weights = HBoxContainer.new()
			box.add_child(weights)
		var biome: String = _biome_names()[index]
		_add_field(weights, biome, biome, 0, 100, 1, world.get_node(biome).occurrence_weight)
	var row := HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "seed", "Layout seed", -1000000, 1000000, 1, world.seed)
	_add_field(row, "region_size", "Cell size (m)", 4, 64, 1, world.region_size)
	_add_field(row, "transition_width", "Transition (m)", 0, 32, 0.5, world.transition_width)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "cell_jitter", "Site jitter", 0, 1, 0.05, world.cell_jitter)
	_add_field(row, "cells_per_region", "Avg cells/region", 1, 16, 1, world.cells_per_region)
	var column := VBoxContainer.new()
	row.add_child(column)
	var label := Label.new()
	label.text = "Distribution"
	column.add_child(label)
	var mode := OptionButton.new()
	mode.add_item("Grid", WorldRegionLayout.GRID)
	mode.add_item("Voronoi", WorldRegionLayout.VORONOI)
	mode.select(world.layout_mode)
	mode.focus_mode = Control.FOCUS_NONE
	column.add_child(mode)
	controls["layout_mode"] = mode
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "stretch_x", "X stretch", 0.25, 4, 0.25, world.distribution_scale.x)
	_add_field(row, "stretch_z", "Z stretch", 0.25, 4, 0.25, world.distribution_scale.y)
	_add_field(row, "camera_speed", "Camera m/s", 2, 60, 1, 18)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "view_radius", "View radius (m)", 16, 256, 16, terrain.preview_area.size.x * 0.5)
	_add_field(row, "spacing", "Samples (m)", 0.125, 2, 0.125, terrain.sample_spacing)
	_add_field(row, "workers", "Workers", 1, max(256, OS.get_processor_count()), 1, terrain.worker_count)
	row = HBoxContainer.new()
	box.add_child(row)
	var collision := CheckButton.new()
	collision.text = "Collision"
	collision.button_pressed = terrain.collision_enabled
	collision.focus_mode = Control.FOCUS_NONE
	row.add_child(collision)
	controls["collision"] = collision
	var lod := CheckButton.new()
	lod.text = "Distance LOD"
	lod.button_pressed = terrain.lod_enabled
	lod.focus_mode = Control.FOCUS_NONE
	row.add_child(lod)
	controls["lod"] = lod
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "lod_distance", "Full detail (m)", 4, 128, 4, terrain.lod_distance)
	_add_field(row, "lod_error", "LOD error (m)", 0.01, 2, 0.01, terrain.lod_error)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_button(row, "Cancel", terrain.cancel_generation)
	_add_button(row, "Frame view", _frame_view)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_button(row, "Generate (R)", _generate)
	_add_button(row, "←", _pan.bind(Vector2(-8, 0)))
	_add_button(row, "↑", _pan.bind(Vector2(0, -8)))
	_add_button(row, "↓", _pan.bind(Vector2(0, 8)))
	_add_button(row, "→", _pan.bind(Vector2(8, 0)))
	var follow := CheckButton.new()
	follow.text = "Refresh terrain after camera stops"
	follow.button_pressed = true
	follow.focus_mode = Control.FOCUS_NONE
	box.add_child(follow)
	controls["auto_follow"] = follow
	progress_label = Label.new()
	box.add_child(progress_label)
	progress_bar = ProgressBar.new()
	progress_bar.show_percentage = true
	progress_bar.custom_minimum_size.y = 16
	box.add_child(progress_bar)
	metrics = Label.new()
	metrics.custom_minimum_size.x = 360
	metrics.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(metrics)
	message = Label.new()
	message.custom_minimum_size.x = 360
	message.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(message)
	camera_status = Label.new()
	box.add_child(camera_status)
	var help := Label.new()
	help.custom_minimum_size.x = 360
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	help.text = "Arrows: pan camera · Speed: slider/control\nRight-drag: orbit · Wheel: zoom\nG: chunk grid/labels · V: Voronoi map\nW: water guide · Change settings → Generate"
	box.add_child(help)
	map_panel = VBoxContainer.new()
	map_panel.position = Vector2(1040, 16)
	canvas.add_child(map_panel)
	var caption := Label.new()
	caption.text = "Cell edges: dark · Region edges: white"
	caption.add_theme_font_size_override("font_size", 12)
	map_panel.add_child(caption)
	map_view = TextureRect.new()
	map_view.custom_minimum_size = Vector2(216, 216)
	map_view.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	map_panel.add_child(map_view)

func _add_button(row: HBoxContainer, title: String, action: Callable) -> void:
	var button := Button.new()
	button.text = title
	button.focus_mode = Control.FOCUS_NONE
	button.pressed.connect(action)
	row.add_child(button)

func _add_field(row: HBoxContainer, key: String, title: String, low: float, high: float, step: float, value: float) -> void:
	var column := VBoxContainer.new()
	row.add_child(column)
	var label := Label.new()
	label.text = title
	column.add_child(label)
	var field := SpinBox.new()
	field.min_value = low
	field.max_value = high
	field.step = step
	field.value = value
	field.custom_minimum_size.x = 105
	column.add_child(field)
	controls[key] = field

func _build_water() -> void:
	water = MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(64, 64)
	water.mesh = plane
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color(0.05, 0.36, 0.64, 0.25)
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	water.material_override = material
	add_child(water)

func _generate() -> bool:
	map_token += 1
	terrain.cancel_generation()
	for biome in _biome_names():
		world.get_node(biome).occurrence_weight = controls[biome].value
	terrain.worker_count = int(controls.workers.value)
	terrain.sample_spacing = controls.spacing.value
	terrain.collision_enabled = controls.collision.button_pressed
	terrain.lod_enabled = controls.lod.button_pressed
	terrain.lod_distance = controls.lod_distance.value
	terrain.lod_error = controls.lod_error.value
	world.layout_mode = controls.layout_mode.get_selected_id()
	world.cell_jitter = controls.cell_jitter.value
	world.cells_per_region = int(controls.cells_per_region.value)
	world.distribution_scale = Vector2(controls.stretch_x.value, controls.stretch_z.value)
	world.seed = int(controls.seed.value)
	world.region_size = controls.region_size.value
	world.transition_width = controls.transition_width.value
	_apply_extra_controls()
	return _refresh_preview()

func _refresh_preview() -> bool:
	refresh_timer.stop()
	# Only the requested residency window moves. Existing terrain coordinates and
	# camera transforms never change when generation publishes a replacement.
	var requested := Vector3(snappedf(center.x, 16), 0, snappedf(center.z, 16))
	var radius: float = controls.view_radius.value
	requested_area = AABB(requested + Vector3(-radius, -preview_height * 0.5, -radius), Vector3(radius * 2, preview_height, radius * 2))
	terrain.preview_area = requested_area
	terrain.lod_origin = center
	if not terrain.generate_preview():
		message.text = terrain.status
		return false
	return true

func _refresh_if_needed() -> void:
	if controls.auto_follow.button_pressed:
		var requested := Vector3(snappedf(center.x, 16), 0, snappedf(center.z, 16))
		if terrain.is_generating() and requested_area.get_center() == requested:
			return
		_refresh_preview()

func _move(offset: Vector3) -> void:
	center += offset
	_update_camera()
	if controls.auto_follow.button_pressed and Vector3(snappedf(center.x, 16), 0, snappedf(center.z, 16)) != preview_center:
		refresh_timer.start()
	else:
		refresh_timer.stop()

func _pan(offset: Vector2) -> void:
	var right := Vector3(cos(yaw), 0, -sin(yaw))
	var backward := Vector3(sin(yaw), 0, cos(yaw))
	_move(right * offset.x + backward * offset.y)

func _process(delta: float) -> void:
	_update_progress()
	var focused := get_viewport().gui_get_focus_owner()
	if focused is LineEdit:
		return
	var direction := Vector2(float(Input.is_physical_key_pressed(KEY_RIGHT)) - float(Input.is_physical_key_pressed(KEY_LEFT)), float(Input.is_physical_key_pressed(KEY_DOWN)) - float(Input.is_physical_key_pressed(KEY_UP)))
	if direction.length_squared() > 0:
		var speed: float = controls.camera_speed.value
		_pan(direction.normalized() * speed * minf(delta, 0.05))

func _input(event: InputEvent) -> void:
	# Prevent arrow navigation from stealing focus from camera controls. Text
	# fields keep normal cursor/value editing while explicitly focused.
	if event is InputEventKey and event.keycode in [KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN] and not get_viewport().gui_get_focus_owner() is LineEdit:
		get_viewport().set_input_as_handled()

func _generated(stats: Dictionary) -> void:
	progress_label.text = "Ready"
	progress_bar.value = 100
	metrics.text = "%d chunks (16 m) · %d sections\n%d triangles · %.0f ms total\n%d worker threads · %.0f ms worker wall\nScene/physics: %.0f ms spread over frames" % [stats.chunk_count, stats.section_count, stats.triangles, stats.generation_ms, stats.worker_threads_used, stats.worker_ms, stats.publication_ms]
	preview_center = stats.bounds.get_center()
	var distribution := "Voronoi: ~%d cells/region" % world.cells_per_region if world.layout_mode == WorldRegionLayout.VORONOI else "Grid: one cell/region"
	metrics.text += "\nLOD: %d → %d triangles · %d sections" % [stats.source_triangles, stats.triangles, stats.lod_sections]
	message.text = "Loaded center (%d, %d) · %.3f m samples\n%s · Runtime settings" % [preview_center.x, preview_center.z, stats.effective_spacing, distribution]
	(water.mesh as PlaneMesh).size = Vector2(stats.bounds.size.x, stats.bounds.size.z)
	water.position = Vector3(preview_center.x, 0, preview_center.z)
	_build_guides()
	map_token += 1
	_build_map(map_token)

func _generation_started(_request: int) -> void:
	map_token += 1
	progress_label.text = "Queued on workers…"

func _generation_failed(error: String) -> void:
	message.text = error
	progress_label.text = "Failed · previous terrain retained"

func _generation_cancelled() -> void:
	map_token += 1
	progress_label.text = "Cancelling · previous terrain retained"

func _update_progress() -> void:
	if not is_instance_valid(progress_label):
		return
	if not terrain.is_generating():
		progress_bar.value = 100 if not _terrain_statistics().is_empty() else 0
		if terrain.status.begins_with("Generation failed"):
			progress_label.text = "Failed · previous terrain retained"
		elif terrain.status.begins_with("Cancelled"):
			progress_label.text = "Cancelled · previous terrain retained"
		return
	var progress := terrain.get_progress()
	if progress.stage == "Publishing":
		progress_bar.value = 60 + 40.0 * progress.published_pieces / maxf(1, progress.total_pieces)
		progress_label.text = "Registering meshes/collision: %d / %d" % [progress.published_pieces, progress.total_pieces]
	else:
		progress_bar.value = 60.0 * progress.completed_sections / maxf(1, progress.total_sections)
		progress_label.text = "%s: %d / %d sections" % [progress.stage, progress.completed_sections, progress.total_sections]

func _resize_ui() -> void:
	map_panel.position = Vector2(maxf(410, get_viewport().get_visible_rect().size.x - 240), 16)

func _frame_view() -> void:
	distance = controls.view_radius.value * 2.4
	camera.far = maxf(500, distance * 4)
	_update_camera()

func _build_guides() -> void:
	guide_token += 1
	var token := guide_token
	var started := Time.get_ticks_usec()
	if _terrain_statistics().is_empty():
		return
	if guides:
		remove_child(guides)
		guides.queue_free()
	guides = Node3D.new()
	guides.visible = grid_visible
	add_child(guides)
	var bounds: AABB = _terrain_statistics().bounds
	var mesh := ImmediateMesh.new()
	mesh.surface_begin(Mesh.PRIMITIVE_LINES)
	for axis in [0, 2]:
		var start := int(ceil(bounds.position[axis] / 16.0))
		var end := int(floor(bounds.end[axis] / 16.0))
		for cell in range(start, end + 1):
			var a := bounds.position
			var b := bounds.end
			a.y = 0.04
			b.y = 0.04
			a[axis] = cell * 16.0
			b[axis] = cell * 16.0
			mesh.surface_add_vertex(a)
			mesh.surface_add_vertex(b)
	mesh.surface_end()
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(0.45, 0.55, 0.6)
	instance.material_override = material
	guides.add_child(instance)
	var seen := {}
	# Sample the current footprint to label each grouped region only once.
	for z in range(9):
		for x in range(9):
			var query := bounds.position + Vector3((x + 0.5) * bounds.size.x / 9, -bounds.position.y, (z + 0.5) * bounds.size.z / 9)
			var info := _diagnostic_region(query)
			if Time.get_ticks_usec() - started > 2000:
				await get_tree().process_frame
				if token != guide_token or not is_inside_tree():
					return
				started = Time.get_ticks_usec()
			if not _guide_can_sample(query) or info.is_empty() or seen.has(info.region):
				continue
			seen[info.region] = true
			var point: Vector3 = info.center
			if not bounds.has_point(point) or _diagnostic_region(point).region != info.region:
				point = query
			var label := Label3D.new()
			label.text = str(info.label) if info.has("label") else "%s\nregion %s" % [info.biome, info.region]
			if not _guide_can_sample(point):
				point = query
			label.position = Vector3(point.x, _surface_height(point, bounds) + 1.5, point.z)
			label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			label.font_size = 32
			label.pixel_size = 0.0012
			label.fixed_size = true
			label.no_depth_test = true
			guides.add_child(label)

func _build_map(token: int) -> void:
	const SIDE := 96
	var merge_borders := _merge_biome_borders()
	map_panel.get_child(0).text = "Cells: dark · Biome borders: white" if merge_borders else "Cell edges: dark · Region edges: white"
	var bounds: AABB = _terrain_statistics().bounds
	var image := Image.create(SIDE, SIDE, false, Image.FORMAT_RGB8)
	var cells: Array[Vector3i] = []
	var regions: Array[Vector3i] = []
	var colors: Array[Color] = []
	var palette := {"Land": Color(0.4,0.6,0.25), "Oceans": Color(0.06,0.25,0.45), "Drylands": Color(0.8,0.6,0.22), "Humid": Color(0.14,0.5,0.25), "Temperate": Color(0.45,0.65,0.3), "Coldlands": Color(0.65,0.75,0.72), "Uplands": Color(0.6,0.6,0.55), "Trenches": Color(0.015,0.05,0.14), "Plains": Color(0.5, 0.7, 0.3), "Hills": Color(0.25, 0.48, 0.2), "Mountains": Color(0.75, 0.78, 0.82), "Sea": Color(0.15, 0.52, 0.7), "DeepSea": Color(0.08, 0.17, 0.35), "Desert": Color(0.85, 0.7, 0.3), "Canyons": Color(0.65, 0.29, 0.16), "Badlands": Color(0.77, 0.42, 0.2), "Savanna": Color(0.65, 0.65, 0.23), "Wetlands": Color(0.25, 0.4, 0.3), "Tundra": Color(0.65, 0.66, 0.58), "Forest": Color(0.18, 0.39, 0.16), "Taiga": Color(0.17, 0.33, 0.3), "Rainforest": Color(0.1, 0.48, 0.27), "Highlands": Color(0.53, 0.57, 0.39), "AlpineMeadows": Color(0.6, 0.7, 0.45), "Steppe": Color(0.68, 0.63, 0.34), "Mangroves": Color(0.18, 0.35, 0.3), "Volcanic": Color(0.28, 0.25, 0.27), "Karst": Color(0.75, 0.72, 0.6)}
	var frame_started := Time.get_ticks_usec()
	for z in range(SIDE):
		if Time.get_ticks_usec() - frame_started > 2000:
			await get_tree().process_frame
			if not is_inside_tree() or token != map_token:
				return
			frame_started = Time.get_ticks_usec()
		for x in range(SIDE):
			var point := Vector3(bounds.position.x + (x + 0.5) * bounds.size.x / SIDE, 0, bounds.position.z + (z + 0.5) * bounds.size.z / SIDE)
			var info := _diagnostic_region(point)
			if info.is_empty():
				return
			cells.append(info.cell)
			regions.append(info.region)
			colors.append(palette.get(info.biome, Color.MAGENTA))
	for z in range(SIDE):
		if Time.get_ticks_usec() - frame_started > 2000:
			await get_tree().process_frame
			if not is_inside_tree() or token != map_token:
				return
			frame_started = Time.get_ticks_usec()
		for x in range(SIDE):
			var index := x + z * SIDE
			var color := colors[index]
			for neighbor in [index - 1 if x > 0 else index, index - SIDE if z > 0 else index]:
				if regions[index] != regions[neighbor] and (not merge_borders or colors[index] != colors[neighbor]):
					color = Color.WHITE
					break
				if cells[index] != cells[neighbor]:
					color = color.darkened(0.45)
			image.set_pixel(x, z, color)
	if token != map_token:
		return
	map_view.texture = ImageTexture.create_from_image(image)
	map_ready_revision = _terrain_statistics().revision

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		yaw -= event.relative.x * 0.006
		pitch = clampf(pitch + event.relative.y * 0.006, 0.1, 1.45)
		_update_camera()
	elif event is InputEventMouseButton and event.pressed:
		get_viewport().gui_release_focus()
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(12, distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(10000, distance / 0.9)
		_update_camera()
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_R: _generate()
			KEY_G:
				grid_visible = not grid_visible
				if not is_instance_valid(guides) and grid_visible:
					_build_guides()
				if is_instance_valid(guides):
					guides.visible = grid_visible
			KEY_W: water.visible = not water.visible
			KEY_V: map_panel.visible = not map_panel.visible

func _update_camera() -> void:
	camera.position = center + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(center)
	camera.h_offset = camera_horizontal_offset
	camera.far = maxf(camera.far, distance * 3)
	if is_instance_valid(camera_status):
		camera_status.text = "Camera focus: (%.1f, %.1f)" % [center.x, center.z]

# Cumulative demos extend the shared camera, map, scheduling and performance UI.
func _build_extra_controls(_box: VBoxContainer) -> void:
	pass

func _apply_extra_controls() -> void:
	pass

func _diagnostic_region(point: Vector3) -> Dictionary:
	return world.get_region_at(point)

func _demo_title() -> String:
	return "003 · Voronoi world"

func _merge_biome_borders() -> bool:
	return false

func _biome_names() -> Array:
	return BIOMES

func _terrain_statistics() -> Dictionary:
	return terrain.get_statistics()

func _sample_field(point: Vector3) -> float:
	return terrain.sample_field(point)

func _guide_can_sample(_point: Vector3) -> bool:
	return true

func _surface_height(point: Vector3, bounds: AABB) -> float:
	var upper := bounds.end.y
	if _sample_field(Vector3(point.x, upper, point.z)) <= 0:
		return upper
	var step: float = maxf(1, _terrain_statistics().get("effective_spacing", 1))
	var lower := upper - step
	while lower >= bounds.position.y:
		if _sample_field(Vector3(point.x, lower, point.z)) <= 0:
			for iteration in range(10):
				var middle := (lower + upper) * 0.5
				if _sample_field(Vector3(point.x, middle, point.z)) <= 0:
					lower = middle
				else:
					upper = middle
			return (lower + upper) * 0.5
		upper = lower
		lower -= step
	return bounds.position.y
