extends "res://003_voronoi_world.gd"

# This scene adds orchestration only. Allocation, instance variation, terrain,
# carving, materials and LOD remain native C++ generation operations.
var ravines: Array[WorldRavineDecorator] = []
var layouts: Array[WorldRegionLayout] = []
var profiles: Array[WorldTerrainDecorator] = []
var profile_names: Array[String] = []
var bases: Array[WorldBaseElevationDecorator] = []
var authored_sizes: Dictionary = {}
var authored_elevations: Dictionary = {}
var map_level := 1
var profile_index := 0

func _ready() -> void:
	preview_height = 128
	distance = 300
	camera_horizontal_offset = -52
	grid_visible = false
	capture_path = _capture_output()
	_collect_settings($World)
	super._ready()

func _collect_settings(container: RegionContainer) -> void:
	if container is WorldRegionLayout and container != $World:
		layouts.append(container)
		authored_sizes[container] = container.region_size
	for decorator in container.decorators:
		if decorator is WorldRavineDecorator and not ravines.has(decorator):
			ravines.append(decorator)
		elif decorator is WorldTerrainDecorator and not profiles.has(decorator):
			profiles.append(decorator)
			var kind: String = decorator.get_class().trim_prefix("World").trim_suffix("Decorator")
			var role := "layer" if decorator.blend_mode == WorldTerrainDecorator.ADD_RELIEF else "base"
			profile_names.append("%s · %s (%s)" % [container.name, kind, role])
		elif decorator is WorldBaseElevationDecorator and not bases.has(decorator):
			bases.append(decorator)
			authored_elevations[decorator] = decorator.elevation
	for child in container.get_children():
		if child is RegionContainer:
			_collect_settings(child)

func _build_extra_controls(box: VBoxContainer) -> void:
	var explanation := Label.new()
	explanation.text = _terrain_description()
	explanation.custom_minimum_size.x = 360
	explanation.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(explanation)
	var row := HBoxContainer.new()
	box.add_child(row)
	for level in range(1, 5):
		_toggle(row, "ravine_%d" % level, "L%d cuts" % level, true)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "cut_chance", "Cut chance ×", 0, 25, 0.1, 1)
	_add_field(row, "variation", "Cut variation", 0, 1, 0.05, 0.8)
	_add_field(row, "nested_scale", "Child cell scale", 0.5, 2, 0.25, 1)
	row = HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "base_scale", "Elevation ×", 0, 1.5, 0.1, 1)
	_add_field(row, "map_level", "Map level", 1, 4, 1, 1)
	_toggle(row, "bounded", "Bound world", false)
	row = HBoxContainer.new()
	box.add_child(row)
	_toggle(row, "strata", "Strata", true)
	_toggle(row, "caves", "Cave", true)
	_toggle(row, "additions", "Rock outcrops", true)
	var hint := Label.new()
	hint.text = "Generate applies settings. G: labels · V: map · W: sea\nMap level selects a nested allocation; L4 may end earlier."
	hint.custom_minimum_size.x = 360
	hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(hint)
	_build_profile_controls(box)

func _toggle(row: HBoxContainer, key: String, title: String, value: bool) -> void:
	var toggle := CheckButton.new()
	toggle.text = title
	toggle.button_pressed = value
	toggle.focus_mode = Control.FOCUS_NONE
	row.add_child(toggle)
	controls[key] = toggle

func _apply_extra_controls() -> void:
	# Resources are explicitly authored settings here, never mutated by workers.
	for index in range(mini(4, ravines.size())):
		var cut := ravines[index]
		cut.enabled = controls["ravine_%d" % (index + 1)].button_pressed
		cut.probability = clampf((0.04 if index == 0 else 0.08) * controls.cut_chance.value, 0, 1)
		cut.variation = controls.variation.value
	for layout in layouts:
		layout.region_size = maxf(4, authored_sizes[layout] * controls.nested_scale.value)
		layout.layout_mode = world.layout_mode
		layout.cell_jitter = world.cell_jitter
	for base in bases:
		base.elevation = authored_elevations[base] * controls.base_scale.value
	for profile in profiles:
		# Keep relief visible at elevation multiplier zero.
		profile.enabled = true
	_set_strata(world, controls.strata.button_pressed)
	world.decorators[0].enabled = controls.caves.button_pressed
	for profile in profiles:
		if profile.blend_mode == WorldTerrainDecorator.ADD_SOLID:
			profile.enabled = controls.additions.button_pressed
	world.bounded = controls.bounded.button_pressed
	world.area = AABB(Vector3(-128, -64, -128), Vector3(256, 128, 256))
	map_level = int(controls.map_level.value)

func _set_strata(container: RegionContainer, enabled: bool) -> void:
	for decorator in container.decorators:
		if decorator is WorldStrataDecorator:
			decorator.enabled = enabled
	for child in container.get_children():
		if child is RegionContainer:
			_set_strata(child, enabled)

func _diagnostic_region(point: Vector3) -> Dictionary:
	var layout: WorldRegionLayout = world
	var query := point
	var offset := Vector3.ZERO
	var instance_seed := 0
	var region_key := ""
	var cell_key := ""
	var top_biome := ""
	var names: Array[String] = []
	var result: Dictionary
	for level in range(map_level):
		result = layout.get_region_at(query, instance_seed)
		if result.is_empty():
			return result
		var child := layout.get_node(NodePath(result.biome)) as RegionContainer
		if level == 0:
			top_biome = result.biome
		names.append(result.biome)
		region_key += "/" + str(result.region)
		cell_key = region_key + "/" + str(result.cell)
		var instance_center: Vector3 = result.center
		result.center += offset
		instance_seed = result.instance_seed
		if not child is WorldRegionLayout:
			break
		# Convert the point into the chosen template's authoring coordinates,
		# exactly as RegionLayoutModifier does; never place scene instances.
		var translation := instance_center - child.global_position
		offset += translation
		query -= translation
		layout = child
	result.region = Vector3i(hash(region_key), 0, 0)
	result.cell = Vector3i(hash(cell_key), 0, 0)
	result.biome = top_biome
	result["label"] = "/".join(names)
	return result

func _demo_title() -> String:
	return "004 · Nested regions and ravines"

func _build_profile_controls(box: VBoxContainer) -> void:
	var toggle := CheckButton.new()
	toggle.text = "Edit terrain profiles"
	toggle.focus_mode = Control.FOCUS_NONE
	box.add_child(toggle)
	var fields := VBoxContainer.new()
	fields.visible = false
	box.add_child(fields)
	toggle.toggled.connect(func(enabled: bool): fields.visible = enabled)
	var selector := OptionButton.new()
	for name in profile_names:
		selector.add_item(name)
	selector.fit_to_longest_item = false
	selector.clip_text = true
	selector.focus_mode = Control.FOCUS_NONE
	selector.item_selected.connect(_select_profile)
	fields.add_child(selector)
	var row := HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_elevation", "Local height", -20, 20, 0.25, 0)
	_add_field(row, "profile_amplitude", "Relief", 0, 512, 0.25, 1)
	_add_field(row, "profile_wavelength", "Wavelength", 0.25, 1024, 0.25, 24)
	row = HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_seed", "Terrain seed", -1000000, 1000000, 1, 100)
	_add_field(row, "profile_octaves", "Octaves", 1, 6, 1, 3)
	_add_field(row, "profile_material_id", "Material ID", 1, 65535, 1, 2)
	row = HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_roughness", "Rock detail", -1, 0.95, 0.05, -1)
	_add_field(row, "profile_ridge_sharpness", "Peak sharpness", 1, 8, 0.1, 2)
	_add_field(row, "profile_interior_elevation", "Interior lift", -32, 64, 0.5, 0)
	row = HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_interior_scale", "Interior relief ×", 0, 8, 0.1, 1)
	_add_field(row, "profile_interior_width", "Edge ramp (m)", 0.1, 128, 0.1, 12)
	controls.profile_roughness.tooltip_text = "-1 uses the profile default; larger values retain more fine rocky detail."
	controls.profile_ridge_sharpness.tooltip_text = "Legacy noise Mountains only; structural ranges use the summit/ridge controls."
	var continuous := CheckButton.new()
	continuous.text = "Continuous world noise"
	continuous.focus_mode = Control.FOCUS_NONE
	continuous.toggled.connect(_edit_profile_noise)
	fields.add_child(continuous)
	controls["profile_world_space_noise"] = continuous
	for property in ["elevation", "amplitude", "wavelength", "seed", "octaves", "material_id", "roughness", "ridge_sharpness", "interior_elevation", "interior_scale", "interior_width"]:
		controls["profile_" + property].value_changed.connect(_edit_profile.bind(property))
	_select_profile(0)

func _select_profile(index: int) -> void:
	profile_index = index
	controls.profile_world_space_noise.set_pressed_no_signal(profiles[index].world_space_noise)
	for property in ["elevation", "amplitude", "wavelength", "seed", "octaves", "material_id", "roughness", "ridge_sharpness", "interior_elevation", "interior_scale", "interior_width"]:
		controls["profile_" + property].set_value_no_signal(profiles[index].get(property))

func _edit_profile(value: float, property: String) -> void:
	profiles[profile_index].set(property, int(value) if property in ["seed", "octaves", "material_id", "summit_count", "terrace_steps"] else value)

func _merge_biome_borders() -> bool:
	return map_level == 1 and world.biome_edges

func _edit_profile_noise(enabled: bool) -> void:
	profiles[profile_index].world_space_noise = enabled

func _capture_output() -> String:
	return "/tmp/worldgen-nested-regions.png"

func _terrain_description() -> String:
	return "Five biomes with varied terrain subregions.\nRocky mountains rise from their outer edges inward."
