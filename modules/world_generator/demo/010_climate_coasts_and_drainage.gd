extends "res://009_lakes_plateaus_and_terrain_variety.gd"

class DrainageOverlay extends Control:
	var paths: Array = []
	var area := AABB()
	func _draw() -> void:
		if area.size.x <= 0 or area.size.z <= 0:
			return
		for path in paths:
			var points := PackedVector2Array()
			for p in path:
				points.append(Vector2((p.x - area.position.x) / area.size.x * size.x, (p.z - area.position.z) / area.size.z * size.y))
			if points.size() > 1:
				draw_polyline(points, Color(0.1, 0.75, 1), 1.4, true)
				draw_circle(points[0], 2.5, Color.WHITE)

var router := WorldRiverRouter.new()
var plan_request := 0
var plan_finished := -1
var plan_started := -1
var preparing := false
var plan_valid := false
var batch_requested := false
var routing_stats: Dictionary = {}
var drainage_paths: Array = []
var lake_sites: Dictionary = {}
var weight_controls: Dictionary = {}
var overlay: DrainageOverlay
var feature_buttons: Dictionary = {}

func _demo_title() -> String:
	return "010 · Climate, coasts and drainage"

func _capture_output() -> String:
	return "/tmp/worldgen010/terrain.png"

func _terrain_description() -> String:
	return "Land/ocean → climate families → biomes → formations.\nHighland headwaters follow noisy drainage edges to the ocean."

func _ready() -> void:
	super._ready()
	stream.set_paused(true)
	preview_height = 512
	distance = 950
	camera_horizontal_offset = -110
	_update_camera()

# The older scene's fixed river/lake alignment is replaced by the routed graph.
func _synchronise_water_levels() -> void:
	pass

func _build_ui() -> void:
	super._build_ui()
	controls.view_radius.value = 384
	controls.transition_width.value = world.transition_width
	controls.map_level.max_value = 7
	controls.map_level.value = 3
	map_level = 3
	controls.rivers.text = "River channels"
	controls.lakes.text = "Lake basins"
	controls.river_grade.value = 0.0005
	controls.river_grade.get_parent().get_child(0).text = "Minimum river grade"
	controls.river_grade.tooltip_text = "Routing grade target; sea-level outlets stay at Y=0. Generated paths receive no additional deformation."
	overlay = DrainageOverlay.new()
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.clip_contents = true
	map_view.add_child(overlay)
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

func _build_extra_controls(box: VBoxContainer) -> void:
	super._build_extra_controls(box)
	var row := HBoxContainer.new()
	box.add_child(row)
	box.move_child(row, 2)
	for name in ["sea_water", "lake_water", "river_water"]:
		_toggle(row, name, {"sea_water": "Sea surface", "lake_water": "Lake surface", "river_water": "River surface"}[name], true)
		controls[name].toggled.connect(func(_value: bool): _apply_water_visibility())
	row = HBoxContainer.new()
	box.add_child(row)
	box.move_child(row, 3)
	for name in ["Headwater", "Mouth", "Coastal lake", "Highland lake"]:
		var button := Button.new()
		button.text = name
		button.disabled = true
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(_focus_feature.bind(name))
		row.add_child(button)
		feature_buttons[name] = button
	var toggle := CheckButton.new()
	toggle.text = "Climate / biome weights"
	toggle.focus_mode = Control.FOCUS_NONE
	box.add_child(toggle)
	var fields := VBoxContainer.new()
	fields.visible = false
	box.add_child(fields)
	toggle.toggled.connect(func(value: bool): fields.visible = value)
	var groups := [$World/Land]
	for child in $World/Land.get_children():
		groups.append(child)
	groups.append($World/Oceans)
	for group in groups:
		var label := Label.new()
		label.text = str(group.name)
		fields.add_child(label)
		var index := 0
		for child in group.get_children():
			if index % 2 == 0:
				row = HBoxContainer.new()
				fields.add_child(row)
			var key := "weight_" + str(child.get_path())
			_add_field(row, key, str(child.name), 0, 100, 1, child.occurrence_weight)
			weight_controls[child] = controls[key]
			index += 1

func _apply_extra_controls() -> void:
	for cut in ravines:
		if cut.resource_name.begins_with("NestedCut"):
			var index := int(cut.resource_name.trim_prefix("NestedCut")) - 1
			cut.enabled = controls["ravine_%d" % (index + 1)].button_pressed
			cut.probability = clampf((0.04 if index == 0 else 0.08) * controls.cut_chance.value, 0, 1)
			cut.variation = controls.variation.value
	for layout in layouts:
		layout.region_size = maxf(4, authored_sizes[layout] * controls.nested_scale.value)
		layout.layout_mode = world.layout_mode
		layout.cell_jitter = world.cell_jitter
	for node in weight_controls:
		node.occurrence_weight = weight_controls[node].value
	for base in bases:
		base.elevation = authored_elevations[base] * controls.base_scale.value
	for profile in profiles:
		profile.enabled = controls.additions.button_pressed if profile.blend_mode == WorldTerrainDecorator.ADD_SOLID else true
	_set_strata(world, controls.strata.button_pressed)
	world.decorators[0].enabled = controls.caves.button_pressed
	world.bounded = controls.bounded.button_pressed
	world.area = AABB(Vector3(-512, -288, -512), Vector3(1024, 512, 1024))
	map_level = int(controls.map_level.value)
	terrain.lod_near_error = controls.near_error.value
	terrain.lod_max_level = int(controls.lod_levels.value)
	streaming_enabled = controls.streaming.button_pressed
	controls.fit_budget.disabled = streaming_enabled

func _generate() -> bool:
	plan_request += 1
	batch_requested = true
	router.cancel()
	stream.set_paused(true)
	terrain.cancel_generation()
	map_token += 1
	for name in _biome_names():
		world.get_node(name).occurrence_weight = controls[name].value
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
	call_deferred("_start_initial_stream")
	return true

func prepare_generation() -> void:
	if preparing:
		while preparing and is_inside_tree():
			await get_tree().process_frame
		return
	if plan_finished == plan_request:
		return
	preparing = true
	while plan_finished != plan_request:
		var request := plan_request
		router.cancel()
		while router.is_running():
			await get_tree().process_frame
		if request != plan_request:
			continue
		var decorators: Array[WorldDecorator] = []
		for decorator in world.decorators:
			if decorator is WorldRiverDecorator:
				continue
			if decorator is WorldLakeDecorator:
				decorator.enabled = false
			decorators.append(decorator)
		world.decorators = decorators
		router.minimum_grade = controls.river_grade.value
		router.minimum_source_height = 64
		router.max_incision = 6
		router.source_separation = 192
		router.edge_noise = 0.8
		plan_valid = router.begin(world, AABB(Vector3(-1280, -288, -1280), Vector3(2560, 512, 2560)), world.seed, 48, 6)
		while router.is_running():
			await get_tree().process_frame
		if request != plan_request:
			continue
		routing_stats = router.get_statistics()
		if not plan_valid:
			plan_finished = request
			break
		drainage_paths = router.get_paths()
		# Review buttons target the closest accepted course, independent of camera motion.
		drainage_paths.sort_custom(func(a, b): return Vector2(a[0].x, a[0].z).length_squared() < Vector2(b[0].x, b[0].z).length_squared())
		lake_sites = router.get_lakes()
		for path in drainage_paths:
			var river := WorldRiverDecorator.new()
			river.centerline = path
			river.meander = 0
			river.slope = 0
			river.width = 5.0
			river.bank_width = 8.0
			river.depth = 2.5
			river.max_cut_depth = 8.5
			river.enabled = controls.rivers.button_pressed
			decorators.append(river)
		var index := 0
		for decorator in decorators:
			if decorator is WorldLakeDecorator:
				var kind := "coastal" if index == 0 else "highland"
				if lake_sites.has(kind):
					decorator.center = lake_sites[kind]
					decorator.enabled = controls.lakes.button_pressed
				index += 1
		world.decorators = decorators
		plan_finished = request
		marker_placed = false
	preparing = false

func _start_initial_stream() -> void:
	stream.set_paused(true)
	await prepare_generation()
	if plan_started == plan_finished:
		return
	plan_started = plan_finished
	if not plan_valid:
		stream.set_paused(false)
		return
	_build_river_water()
	if streaming_enabled:
		stream.set_enabled(true)
		stream.rebuild()
		stream.set_paused(false)
		_refresh_preview()
	else:
		stream.set_enabled(false)
		if batch_requested:
			_refresh_preview()
		batch_requested = false
	for name in feature_buttons:
		feature_buttons[name].disabled = drainage_paths.is_empty() if name in ["Headwater", "Mouth"] else not lake_sites.has("coastal" if name == "Coastal lake" else "highland")
	if is_instance_valid(overlay):
		overlay.paths = drainage_paths
		overlay.queue_redraw()

func _refresh_preview() -> bool:
	if preparing:
		return true
	refresh_timer.stop()
	var radius: float = controls.view_radius.value
	terrain.preview_area = AABB(Vector3(-radius, -288, -radius), Vector3(radius * 2, 512, radius * 2))
	terrain.sample_spacing = controls.spacing.value
	if not streaming_enabled:
		terrain.lod_origin = center
		return terrain.generate_preview()
	stream.view_distance = radius
	stream.set_focus(center)
	requested_area = _terrain_statistics().bounds
	preview_center = center
	map_token += 1
	_build_map(map_token)
	(water.mesh as PlaneMesh).size = Vector2(radius * 2, radius * 2)
	water.position = Vector3(center.x, 0, center.z)
	camera.far = maxf(distance * 3, radius * 4)
	return true

func _update_progress() -> void:
	if preparing:
		var stats := router.get_statistics()
		progress_label.text = {0: "Preparing drainage…", 1: "Building drainage graph…", 2: "Sampling drainage terrain…", 3: "Routing headwaters to ocean…", 4: "Publishing drainage…"}.get(stats.phase, "Planning drainage…")
		progress_bar.value = 100.0 * stats.samples_done / maxf(1, stats.samples_total)
		message.text = "Drainage uses a fixed world area; moving the camera keeps the same rivers."
	else:
		super._update_progress()

func _stream_updated() -> void:
	if preparing:
		return
	super._stream_updated()
	if not routing_stats.is_empty():
		metrics.text += "\n%d routed rivers · %.0f ms drainage" % [drainage_paths.size(), routing_stats.get("planning_ms", 0)]
		if not str(routing_stats.get("error", "")).is_empty():
			message.text += "\n" + str(routing_stats.error)

func _river_water(bounds: AABB) -> Array:
	var instances := super._river_water(bounds)
	for instance in instances:
		instance.visible = controls.lake_water.button_pressed if instance.get_meta("water_kind", "river") == "lake" else controls.river_water.button_pressed
	return instances

func _apply_water_visibility() -> void:
	if not is_instance_valid(water):
		return
	water.visible = controls.sea_water.button_pressed
	for instance in river_instances:
		instance.visible = controls.lake_water.button_pressed if instance.get_meta("water_kind", "river") == "lake" else controls.river_water.button_pressed
	for instances in river_patches.values():
		for instance in instances:
			instance.visible = controls.lake_water.button_pressed if instance.get_meta("water_kind", "river") == "lake" else controls.river_water.button_pressed

func _unhandled_input(event: InputEvent) -> void:
	super._unhandled_input(event)
	if event is InputEventKey and event.pressed and event.keycode == KEY_W:
		controls.sea_water.set_pressed_no_signal(water.visible)

func _focus_feature(name: String) -> void:
	var point := center
	if name == "Headwater" and not drainage_paths.is_empty():
		point = drainage_paths[0][0]
	elif name == "Mouth" and not drainage_paths.is_empty():
		point = drainage_paths[0][drainage_paths[0].size() - 1]
	elif name == "Coastal lake" and lake_sites.has("coastal"):
		point = lake_sites.coastal
	elif name == "Highland lake" and lake_sites.has("highland"):
		point = lake_sites.highland
	distance = 240
	camera_horizontal_offset = -40
	_move(point - center)
	_refresh_preview()

func _merge_biome_borders() -> bool:
	return map_level <= 3

func _diagnostic_region(point: Vector3) -> Dictionary:
	var layout: WorldRegionLayout = world
	var query := point
	var offset := Vector3.ZERO
	var instance_seed := 0
	var key := ""
	var names: Array[String] = []
	var display := "Land"
	var known := ["Land", "Oceans", "Drylands", "Humid", "Temperate", "Coldlands", "Uplands", "Plains", "Hills", "Mountains", "Sea", "DeepSea", "Trenches", "Desert", "Canyons", "Badlands", "Savanna", "Wetlands", "Tundra", "Forest", "Taiga", "Rainforest", "Highlands", "AlpineMeadows", "Steppe", "Mangroves", "Volcanic", "Karst"]
	var result: Dictionary
	for level in range(map_level):
		result = layout.get_region_at(query, instance_seed)
		if result.is_empty():
			return result
		var child := layout.get_node(NodePath(result.biome)) as RegionContainer
		names.append(result.biome)
		if result.biome in known:
			display = result.biome
		key += "/" + str(result.region)
		var instance_center: Vector3 = result.center
		result.center += offset
		instance_seed = result.instance_seed
		if not child is WorldRegionLayout:
			break
		var translation := instance_center - child.global_position
		offset += translation
		query -= translation
		layout = child
	result.region = Vector3i(hash(key), 0, 0)
	result.cell = Vector3i(hash(key + str(result.cell)), 0, 0)
	result.biome = display
	result.label = "/".join(names)
	return result

func _build_map(token: int) -> void:
	await super._build_map(token)
	if is_instance_valid(overlay) and token == map_token:
		overlay.area = _terrain_statistics().bounds
		overlay.paths = drainage_paths
		overlay.queue_redraw()
