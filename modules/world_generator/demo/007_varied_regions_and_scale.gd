extends "res://006_heightmap_evaluated_terrain.gd"

var scale_reference: MeshInstance3D

func _demo_title() -> String:
	return "007 · Varied regions and human scale"

func _capture_output() -> String:
	return "/tmp/worldgen-regions/terrain.png"

func _terrain_description() -> String:
	return "Rocky ranges, rolling lowlands and deeper seabeds.\nCurved region boundaries · orange capsule: 1.8 m tall."

func _build_extra_controls(box: VBoxContainer) -> void:
	super._build_extra_controls(box)
	var fit := CheckButton.new()
	fit.text = "Fit sampling to preview budget"
	fit.button_pressed = true
	fit.focus_mode = Control.FOCUS_NONE
	fit.tooltip_text = "Large windows use coarser sampling everywhere; mesh LOD is separate. Disable to test a fixed resolution."
	box.add_child(fit)
	controls["fit_budget"] = fit
	var focus := Button.new()
	focus.text = "View 1.8 m scale capsule"
	focus.focus_mode = Control.FOCUS_NONE
	focus.pressed.connect(func():
		if is_instance_valid(scale_reference) and scale_reference.visible:
			center = scale_reference.position
			distance = 5
			pitch = 1.2
			camera_horizontal_offset = 0
			_update_camera()
	)
	box.add_child(focus)
	var row := HBoxContainer.new()
	box.add_child(row)
	_add_field(row, "boundary_noise", "Boundary noise × cell", 0, 0.4, 0.01, world.boundary_noise)

func _apply_extra_controls() -> void:
	super._apply_extra_controls()
	world.boundary_noise = controls.boundary_noise.value

func _fits_preview(area: AABB, requested_spacing: float) -> bool:
	# Match native lattice snapping and section halos before allocating any data.
	var world_cells := int(ceil(16.0 / requested_spacing))
	var best := 4
	for cells in range(4, 65):
		if world_cells % cells == 0 and abs(cells - terrain.section_cells) <= abs(best - terrain.section_cells):
			best = cells
	var spacing := 16.0 / world_cells
	if world_cells % best != 0:
		var subdivisions := maxi(1, int(ceil(float(world_cells) / terrain.section_cells)))
		best = int(ceil(float(world_cells) / subdivisions))
		spacing = 16.0 / (best * subdivisions)
	var section_size := spacing * best
	var first := (area.position / section_size).floor()
	var last := (area.end / section_size).ceil() - Vector3.ONE
	var count := last - first + Vector3.ONE
	var sections := int(count.x * count.y * count.z)
	return sections <= 16384 and sections * (best + 3) ** 3 <= terrain.sample_budget

func _refresh_preview() -> bool:
	var requested := Vector3(snappedf(center.x, 16), 0, snappedf(center.z, 16))
	var radius: float = controls.view_radius.value
	var area := AABB(requested + Vector3(-radius, -preview_height * 0.5, -radius), Vector3(radius * 2, preview_height, radius * 2))
	terrain.sample_spacing = controls.spacing.value
	if controls.fit_budget.button_pressed:
		for spacing in [terrain.sample_spacing, 1.0, 2.0]:
			if spacing < controls.spacing.value:
				continue
			terrain.sample_spacing = spacing
			if _fits_preview(area, spacing):
				break
	return super._refresh_preview()

func _generated(stats: Dictionary) -> void:
	super._generated(stats)
	_update_scale_reference(stats)

func _update_scale_reference(stats: Dictionary) -> void:
	message.text += "\nRequested %.2f m · effective %.2f m samples" % [controls.spacing.value, stats.effective_spacing]
	if is_instance_valid(scale_reference):
		scale_reference.hide()
	if not stats.bounds.has_point(Vector3(0, stats.bounds.get_center().y, 0)):
		if is_instance_valid(scale_reference):
			scale_reference.hide()
		return
	# Highest signed-field crossing, independent of the collision toggle. Scan
	# only one column after publication, then bisect for stable foot placement.
	var top: float = stats.bounds.end.y
	var bottom: float = stats.bounds.position.y
	var upper := top
	var found := false
	if _sample_field(Vector3(0, top, 0)) <= 0:
		return
	var step: float = stats.effective_spacing
	var y := top - step
	while y >= bottom:
		if _sample_field(Vector3(0, y, 0)) <= 0:
			for iteration in range(16):
				var midpoint := (y + upper) * 0.5
				if _sample_field(Vector3(0, midpoint, 0)) <= 0:
					y = midpoint
				else:
					upper = midpoint
			found = true
			break
		upper = y
		y -= step
	if not found:
		if is_instance_valid(scale_reference):
			scale_reference.hide()
		return
	if not is_instance_valid(scale_reference):
		scale_reference = MeshInstance3D.new()
		scale_reference.name = "HumanScaleReference"
		var capsule := CapsuleMesh.new()
		capsule.height = 1.8
		capsule.radius = 0.3
		scale_reference.mesh = capsule
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(1, 0.3, 0.04)
		scale_reference.material_override = material
		add_child(scale_reference)
		var label := Label3D.new()
		label.text = "1.8 m"
		label.position.y = 1.25
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.no_depth_test = true
		label.fixed_size = true
		label.pixel_size = 0.0015
		scale_reference.add_child(label)
	scale_reference.position = Vector3(0, (y + upper) * 0.5 + 0.9, 0)
	scale_reference.show()
