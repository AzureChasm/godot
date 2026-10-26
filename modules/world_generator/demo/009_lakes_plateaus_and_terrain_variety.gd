extends "res://008_streaming_biomes_and_rivers.gd"

var lake_definitions: Array[WorldLakeDecorator] = []
var guide_elapsed := 0.0
var guides_dirty := false
var guide_resident_count := -1
var guide_epoch := -1

func _demo_title() -> String:
	return "009 · Lakes, plateaus and terrain variety"

func _capture_output() -> String:
	return "/tmp/worldgen009/terrain.png"

func _terrain_description() -> String:
	return "20 terrain biomes · layered formations and plateaus.\nSloping rivers · lakes above sea level · leaner meshes."

func _ready() -> void:
	_synchronise_water_levels()
	super._ready()

func _build_ui() -> void:
	super._build_ui()
	# A visible, immediate speed slider replaces the old numeric field.
	var old_column: Control = controls.camera_speed.get_parent()
	var box: VBoxContainer = old_column.get_parent().get_parent()
	old_column.queue_free()
	var row := HBoxContainer.new()
	box.add_child(row)
	box.move_child(row, 2)
	var label := Label.new()
	label.text = "Camera 72 m/s"
	row.add_child(label)
	var speed := HSlider.new()
	speed.min_value = 2
	speed.max_value = 600
	speed.step = 1
	speed.value = 72
	speed.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	speed.custom_minimum_size.x = 170
	speed.focus_mode = Control.FOCUS_NONE
	speed.value_changed.connect(func(value: float): label.text = "Camera %.0f m/s" % value)
	row.add_child(speed)
	controls.camera_speed = speed

func _build_extra_controls(box: VBoxContainer) -> void:
	super._build_extra_controls(box)
	var row := HBoxContainer.new()
	box.add_child(row)
	_toggle(row, "lakes", "Lakes", true)
	_add_field(row, "river_grade", "Extra river grade (m/m)", 0, 0.02, 0.001, 0.002)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "near_error", "Near mesh error (m)", 0, 0.5, 0.01, terrain.lod_near_error)
	_add_field(row, "lod_levels", "LOD levels", 1, 4, 1, terrain.lod_max_level)
	controls.river_grade.tooltip_text = "Additional downhill drop per horizontal metre, on top of the authored point heights. Generate applies changes."
	controls.near_error.tooltip_text = "Conservative near-field mesh simplification. Zero keeps the source mesh near the player; sampling stays unchanged."

func _apply_extra_controls() -> void:
	for decorator in world.decorators:
		if decorator is WorldRiverDecorator:
			decorator.slope = controls.river_grade.value
		elif decorator is WorldLakeDecorator:
			decorator.enabled = controls.lakes.button_pressed
	terrain.lod_near_error = controls.near_error.value
	terrain.lod_max_level = int(controls.lod_levels.value)
	_synchronise_water_levels()
	super._apply_extra_controls()
	guides_dirty = true

func _synchronise_water_levels() -> void:
	var rivers: Array[WorldRiverDecorator] = []
	var lakes: Array[WorldLakeDecorator] = []
	for decorator in world.decorators:
		if decorator is WorldRiverDecorator:
			rivers.append(decorator)
		elif decorator is WorldLakeDecorator:
			lakes.append(decorator)
	if rivers.size() < 2:
		return
	var junction_height := 0.0
	var lake_height := 0.0
	for point in rivers[0].get_generated_centerline():
		if Vector2(point.x + 100, point.z + 256).length() < 0.01:
			junction_height = point.y
		if Vector2(point.x - 30, point.z - 128).length() < 0.01:
			lake_height = point.y
	var tributary_path := rivers[1].centerline
	var generated := rivers[1].get_generated_centerline()
	var adjustment: float = junction_height - generated[generated.size() - 1].y
	for i in range(tributary_path.size()):
		tributary_path[i].y += adjustment
	rivers[1].centerline = tributary_path
	if not lakes.is_empty():
		var location := lakes[0].center
		location.y = lake_height
		lakes[0].center = location

func _build_river_water() -> void:
	lake_definitions.clear()
	for decorator in world.decorators:
		if decorator is WorldLakeDecorator:
			lake_definitions.append(decorator.duplicate(true))
	super._build_river_water()

func _river_water(bounds: AABB) -> Array:
	var instances := super._river_water(bounds)
	for instance in instances:
		for lake in lake_definitions:
			if lake.enabled:
				instance.mesh = lake.conform_water_mesh(instance.mesh)
	for lake in lake_definitions:
		if not lake.enabled:
			continue
		var mesh: ArrayMesh = lake.create_water_mesh(bounds)
		if mesh == null:
			continue
		var instance := MeshInstance3D.new()
		instance.mesh = mesh
		instance.set_meta("water_kind", "lake")
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(0.035, 0.28, 0.4)
		material.roughness = 0.22
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		instance.material_override = material
		add_child(instance)
		instances.append(instance)
	return instances

func _guide_can_sample(point: Vector3) -> bool:
	return stream.has_terrain_at(point) if streaming_enabled else true

func _stream_updated() -> void:
	super._stream_updated()
	var stats := stream.get_statistics()
	if stats.resident_patches != guide_resident_count or stats.revision != guide_epoch:
		guide_resident_count = stats.resident_patches
		guide_epoch = stats.revision
		guides_dirty = true

func _move(offset: Vector3) -> void:
	super._move(offset)
	guides_dirty = true

func _process(delta: float) -> void:
	super._process(delta)
	guide_elapsed += delta
	if grid_visible and guides_dirty and guide_elapsed >= 1:
		guides_dirty = false
		guide_elapsed = 0
		_build_guides()

func _update_camera() -> void:
	super._update_camera()
	# Keep useful depth precision in the overview and a close near plane when
	# inspecting the capsule; tiny fixed near planes cause water z-fighting.
	camera.near = maxf(0.05, distance * 0.005)
