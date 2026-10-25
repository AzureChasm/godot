extends Node3D

# Demo glue only: generation, chunk sampling, meshing, and collision are C++.
@onready var terrain: TerrainView = $TerrainView
@onready var camera: Camera3D = $Camera3D
var yaw := 0.65
var pitch := 0.55
var distance := 10.0
var label: Label

func _ready() -> void:
	_update_camera()
	var canvas := CanvasLayer.new()
	add_child(canvas)
	var panel := PanelContainer.new()
	panel.position = Vector2(20, 20)
	panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	canvas.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "top", "right", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 16)
	panel.add_child(margin)
	label = Label.new()
	label.add_theme_font_size_override("font_size", 17)
	margin.add_child(label)
	terrain.preview_generated.connect(_show_statistics)
	_show_statistics(terrain.get_statistics())
	if "--capture" in OS.get_cmdline_user_args():
		for frame in range(20):
			await get_tree().process_frame
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/tmp/world-generator-prototype.png")
		get_tree().quit()

func _show_statistics(stats: Dictionary) -> void:
	var title := "WORLD GENERATOR / first terrain prototype"
	var controls := "Right-drag: orbit   Wheel: zoom   R: regenerate   Space: bounded / unbounded"
	var metrics := "Generating preview..."
	if not stats.is_empty():
		metrics = "%d chunks · %d compressed · %d triangles · %.0f ms" % [stats.chunk_count, stats.uniform_chunks, stats.triangles, stats.generation_ms]
		metrics += "\nRoot: %s · generated area: %s" % ["bounded" if $World.bounded else "unbounded (finite view request)", str(stats.bounds.size)]
	label.text = title + "\n" + controls + "\n" + metrics

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
		yaw -= event.relative.x * 0.006
		pitch = clampf(pitch + event.relative.y * 0.006, -0.1, 1.45)
		_update_camera()
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			distance = maxf(2, distance * 0.9)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			distance = minf(40, distance / 0.9)
		_update_camera()
	elif event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_R:
			terrain.generate_preview()
		elif event.keycode == KEY_SPACE:
			$World.bounded = not $World.bounded
			terrain.generate_preview()
			_show_statistics(terrain.get_statistics())

func _update_camera() -> void:
	var target := Vector3(0, 0.5, 0)
	camera.position = target + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
	camera.look_at(target)
