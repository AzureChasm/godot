extends "res://005_landforms_and_ridges.gd"

func _demo_title() -> String:
	return "006 · Heightmap-evaluated terrain"

func _capture_output() -> String:
	return "/tmp/worldgen-heightmap/terrain.png"

func _terrain_description() -> String:
	return "Broader summits with varied heights and irregular ridges.\nCompared against a saved top-surface height map."

func _build_profile_controls(box: VBoxContainer) -> void:
	super._build_profile_controls(box)
	var toggle := CheckButton.new()
	toggle.text = "Mountain shape controls"
	toggle.focus_mode = Control.FOCUS_NONE
	box.add_child(toggle)
	var fields := VBoxContainer.new()
	fields.visible = false
	box.add_child(fields)
	toggle.toggled.connect(func(enabled: bool): fields.visible = enabled)
	var row := HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_summit_variation", "Height variety", 0, 1, 0.01, 0.28)
	_add_field(row, "profile_summit_rounding", "Round summits", 0, 1, 0.01, 0)
	row = HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_summit_breadth", "Summit breadth", 0.2, 0.9, 0.01, 0.46)
	_add_field(row, "profile_range_irregularity", "Irregular ridges", 0, 1, 0.01, 0)
	_add_field(row, "profile_summit_height_bias", "Height contrast", 0.25, 4, 0.05, 1)
	var hierarchy := CheckButton.new()
	hierarchy.text = "Dominant and secondary summits"
	hierarchy.focus_mode = Control.FOCUS_NONE
	hierarchy.toggled.connect(_edit_summit_hierarchy)
	fields.add_child(hierarchy)
	controls["profile_hierarchical_summits"] = hierarchy
	for property in ["summit_height_bias", "summit_variation", "summit_rounding", "summit_breadth", "range_irregularity"]:
		controls["profile_" + property].value_changed.connect(_edit_profile.bind(property))
	_select_profile(0)

func _select_profile(index: int) -> void:
	super._select_profile(index)
	if not controls.has("profile_hierarchical_summits"):
		return
	controls.profile_hierarchical_summits.set_pressed_no_signal(profiles[index].hierarchical_summits)
	for property in ["summit_height_bias", "summit_variation", "summit_rounding", "summit_breadth", "range_irregularity"]:
		controls["profile_" + property].set_value_no_signal(profiles[index].get(property))

func _edit_summit_hierarchy(enabled: bool) -> void:
	profiles[profile_index].hierarchical_summits = enabled
