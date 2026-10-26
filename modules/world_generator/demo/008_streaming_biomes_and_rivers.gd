extends "res://007_varied_regions_and_scale.gd"

@export var streaming_enabled := true
@onready var stream: TerrainStreamer = $TerrainStreamer
var river_instances: Array[MeshInstance3D] = []
var river_patches: Dictionary = {}
var river_definitions: Array[WorldRiverDecorator] = []
var stream_ui_elapsed := 0.0
var marker_placed := false
var last_map_focus := Vector3.INF

func _demo_title() -> String:
	return "008 · Streaming biomes and rivers"

func _capture_output() -> String:
	return "/tmp/worldgen-streaming/terrain.png"

func _terrain_description() -> String:
	return "Nearby terrain first; movement favours the forward side.\n14 terrain biomes · river channels · larger alpine ridges."

func _ready() -> void:
	stream.set_enabled(streaming_enabled)
	stream.stream_updated.connect(_stream_updated)
	call_deferred("_start_initial_stream")
	super._ready()
	preview_height = 256
	distance = 650
	camera_horizontal_offset = -85
	_update_camera()

func _start_initial_stream() -> void:
	if streaming_enabled:
		_refresh_preview()
		_build_river_water()

func _biome_names() -> Array:
	var names := []
	for child in world.get_children():
		if child is RegionContainer:
			names.append(str(child.name))
	return names

func _build_ui() -> void:
	super._build_ui()
	controls.view_radius.min_value = 32
	controls.view_radius.max_value = 2048
	controls.view_radius.value = 256
	controls.view_radius.tooltip_text = "Streaming residency radius. New terrain arrives progressively; resident triangle budget is still enforced."
	controls.region_size.max_value = 512
	controls.region_size.value = world.region_size
	controls.transition_width.max_value = 128
	controls.fit_budget.button_pressed = false
	controls.fit_budget.disabled = true

func _build_extra_controls(box: VBoxContainer) -> void:
	super._build_extra_controls(box)
	var row := HBoxContainer.new()
	box.add_child(row)
	_toggle(row, "streaming", "Stream terrain", streaming_enabled)
	_toggle(row, "rivers", "Rivers", true)
	var hint := Label.new()
	hint.text = "Streaming uses fixed-resolution 64 m patches of 16 m chunks.\nGenerate rebuilds settings; movement reuses loaded terrain.\nForest/taiga/rainforest are terrain presets; trees come later."
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	hint.custom_minimum_size.x = 360
	box.add_child(hint)

func _apply_extra_controls() -> void:
	super._apply_extra_controls()
	streaming_enabled = controls.streaming.button_pressed
	if controls.fit_budget.disabled and not streaming_enabled:
		controls.fit_budget.button_pressed = true
	controls.fit_budget.disabled = streaming_enabled
	stream.set_enabled(streaming_enabled)
	for decorator in world.decorators:
		if decorator is WorldRiverDecorator:
			decorator.enabled = controls.rivers.button_pressed
	world.area = AABB(Vector3(-256, -64, -256), Vector3(512, 256, 512))
	_build_river_water()

func _generate() -> bool:
	marker_placed = false
	stream.rebuild()
	return super._generate()

func _refresh_preview() -> bool:
	if not streaming_enabled:
		return super._refresh_preview()
	refresh_timer.stop()
	var radius: float = controls.view_radius.value
	terrain.preview_area = AABB(Vector3(-radius, -64, -radius), Vector3(radius * 2, 256, radius * 2))
	terrain.sample_spacing = controls.spacing.value
	stream.view_distance = radius
	stream.set_focus(center)
	requested_area = _terrain_statistics().bounds
	preview_center = center
	map_token += 1
	_build_map(map_token)
	var size := radius * 2
	(water.mesh as PlaneMesh).size = Vector2(size, size)
	water.position = Vector3(center.x, 0, center.z)
	camera.far = maxf(500, radius * 4)
	return true

func _move(offset: Vector3) -> void:
	super._move(offset)
	if streaming_enabled and controls.auto_follow.button_pressed:
		stream.set_focus(center, offset)
		preview_center = center
		water.position = Vector3(center.x, 0, center.z)

func _process(delta: float) -> void:
	super._process(delta)
	if streaming_enabled:
		stream_ui_elapsed += delta
		if stream_ui_elapsed >= 0.2:
			stream_ui_elapsed = 0
			_stream_updated()
			if controls.auto_follow.button_pressed and (last_map_focus - center).length_squared() > 32 * 32:
				last_map_focus = center
				map_token += 1
				_build_map(map_token)

func _update_progress() -> void:
	if not streaming_enabled:
		super._update_progress()

func _terrain_statistics() -> Dictionary:
	if streaming_enabled and is_instance_valid(stream):
		return stream.get_statistics()
	return super._terrain_statistics()

func _sample_field(point: Vector3) -> float:
	if streaming_enabled:
		return stream.sample_field(point)
	return super._sample_field(point)

func _stream_updated() -> void:
	if not streaming_enabled or not is_instance_valid(metrics):
		return
	var stats := stream.get_statistics()
	metrics.text = "%d / %d resident patches · %d queued\n%d triangles · %.2f m samples\n64 m patches · 16 m world chunks\nMesh publication: 1 ms/frame soft budget" % [stats.resident_patches, stats.wanted_patches, stats.queued_patches, stats.triangles, stats.effective_spacing]
	progress_label.text = "Streaming limit · see status" if not stats.error.is_empty() else "Streaming nearby terrain…" if stats.active else "Ready" if stats.queued_patches == 0 else "Queued"
	progress_bar.value = 100.0 * stats.resident_patches / maxf(1, stats.wanted_patches)
	message.text = stats.error if not stats.error.is_empty() else "Focus (%.0f, %.0f) · radius %.0f m\nLoaded patches persist while moving" % [stats.focus.x, stats.focus.z, stream.view_distance]
	_update_river_patches(stats)
	if not marker_placed and stream.has_terrain_at(Vector3.ZERO):
		_update_scale_reference(stats)
		marker_placed = true
	if is_instance_valid(scale_reference):
		scale_reference.visible = marker_placed and stream.has_terrain_at(Vector3.ZERO)


func _river_water(bounds: AABB) -> Array:
	var instances := []
	for decorator in river_definitions:
		if decorator.enabled:
			var mesh: ArrayMesh = decorator.create_water_mesh(bounds)
			if mesh == null:
				continue
			var instance := MeshInstance3D.new()
			instance.mesh = mesh
			instance.set_meta("water_kind", "river")
			var material := StandardMaterial3D.new()
			material.albedo_color = Color(0.04, 0.32, 0.46)
			material.roughness = 0.25
			material.cull_mode = BaseMaterial3D.CULL_DISABLED
			instance.material_override = material
			add_child(instance)
			instances.append(instance)
	return instances

func _build_river_water() -> void:
	for instance in river_instances:
		instance.queue_free()
	river_instances.clear()
	for instances in river_patches.values():
		for instance in instances:
			instance.queue_free()
	river_patches.clear()
	river_definitions.clear()
	for decorator in world.decorators:
		if decorator is WorldRiverDecorator:
			river_definitions.append(decorator.duplicate(true))
	if not streaming_enabled:
		river_instances.assign(_river_water(terrain.preview_area))

func _update_river_patches(stats: Dictionary) -> void:
	var present := {}
	for cell in stats.resident_cells:
		present[cell] = true
		if not river_patches.has(cell):
			var bounds := AABB(Vector3(cell.x * 64, -64, cell.y * 64), Vector3(64, 256, 64))
			river_patches[cell] = _river_water(bounds)
	for cell in river_patches.keys():
		if not present.has(cell):
			for instance in river_patches[cell]:
				instance.queue_free()
			river_patches.erase(cell)
