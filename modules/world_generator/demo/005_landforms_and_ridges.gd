extends "res://004_nested_regions_and_ravines.gd"

# All landform shape/composition work remains in the native module.
func _demo_title() -> String:
	return "005 · Landforms and ridgelines"

func _capture_output() -> String:
	return "/tmp/worldgen-landforms.png"

func _terrain_description() -> String:
	return "Major summits joined by broad ridgelines.\nSmall terrain layers preserve the mountain shape."

func _build_profile_controls(box: VBoxContainer) -> void:
	super._build_profile_controls(box)
	var toggle := CheckButton.new()
	toggle.text = "Advanced landform controls"
	toggle.focus_mode = Control.FOCUS_NONE
	box.add_child(toggle)
	var fields := VBoxContainer.new()
	fields.visible = false
	box.add_child(fields)
	toggle.toggled.connect(func(enabled: bool): fields.visible = enabled)
	var row := HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_detail_strength", "Small detail", 0, 0.5, 0.01, 0.03)
	_add_field(row, "profile_ridge_width", "Ridge width ×", 0.04, 0.5, 0.01, 0.18)
	_add_field(row, "profile_summit_count", "Summits / tile", 1, 6, 1, 3)
	row = HBoxContainer.new()
	fields.add_child(row)
	_add_field(row, "profile_terrace_steps", "Terrace levels", 2, 32, 1, 6)
	_add_field(row, "profile_edge_strength", "Edge relief ×", 0, 4, 0.05, 1)
	for property in ["detail_strength", "ridge_width", "summit_count", "terrace_steps", "edge_strength"]:
		controls["profile_" + property].value_changed.connect(_edit_profile.bind(property))
	_select_profile(0)

func _select_profile(index: int) -> void:
	super._select_profile(index)
	if not controls.has("profile_detail_strength"):
		return
	for property in ["detail_strength", "ridge_width", "summit_count", "terrace_steps", "edge_strength"]:
		controls["profile_" + property].set_value_no_signal(profiles[index].get(property))
