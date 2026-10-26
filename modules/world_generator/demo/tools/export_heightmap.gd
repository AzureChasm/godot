extends SceneTree

# Batch orchestration only. Surface extraction runs in the native coordinator.
func _initialize() -> void:
	call_deferred("run")

func run() -> void:
	var options := {"scene": ProjectSettings.get_setting("application/run/main_scene"), "output": "/tmp/worldgen-heightmap/heightmap.csv", "stride": "1"}
	var args := OS.get_cmdline_user_args()
	for i in range(0, args.size(), 2):
		if i + 1 >= args.size() or not args[i].begins_with("--"):
			push_error("Expected --scene PATH --output PATH [--stride N] [--seed N] [--workers N] [--spacing M]")
			quit(1)
			return
		var key := args[i].trim_prefix("--")
		if key not in ["scene", "output", "stride", "seed", "workers", "spacing"]:
			push_error("Unknown export option: " + key)
			quit(1)
			return
		options[key] = args[i + 1]
	for key in ["stride", "seed", "workers"]:
		if options.has(key) and not str(options[key]).is_valid_int():
			push_error("Expected integer for " + key)
			quit(1)
			return
	if int(options.stride) < 1 or int(options.stride) > 32 or int(options.get("workers", "1")) < 1:
		push_error("Stride must be 1..32; workers must be positive.")
		quit(1)
		return
	if options.has("spacing") and (not str(options.spacing).is_valid_float() or not is_finite(float(options.spacing)) or float(options.spacing) < 0.025 or float(options.spacing) > 2):
		push_error("Spacing must be finite and between 0.025 and 2 metres.")
		quit(1)
		return
	var packed = load(options.scene)
	if not packed is PackedScene:
		quit(1)
		return
	var scene = packed.instantiate()
	if scene.get("streaming_enabled") != null:
		scene.streaming_enabled = false
	var view: TerrainView = scene.get_node("TerrainView")
	view.auto_generate = false
	if options.has("spacing"):
		view.sample_spacing = float(options.spacing)
	view.heightmap_csv_path = options.output
	view.heightmap_stride = int(options.stride)
	if options.has("workers"):
		view.worker_count = int(options.workers)
	if options.has("seed"):
		scene.get_node("World").seed = int(options.seed)
	root.add_child(scene)
	await process_frame
	if scene.has_method("prepare_generation"):
		await scene.prepare_generation()
	if not view.generate_preview():
		push_error(view.status)
		quit(1)
		return
	var deadline := Time.get_ticks_msec() + 180000
	while view.is_generating() and Time.get_ticks_msec() < deadline:
		await process_frame
	var stats := view.get_statistics()
	var success: bool = not view.is_generating() and not stats.is_empty() and stats.get("heightmap_error", "") == "" and stats.get("heightmap", {}).has("path")
	if success:
		success = stats.heightmap.get("request_id", -1) == stats.request_id
	if success:
		var manifest := FileAccess.open(options.output + ".json", FileAccess.WRITE)
		if manifest == null:
			push_error("Cannot write export manifest.")
			quit(1)
			return
		manifest.store_string(JSON.stringify({"scene": options.scene, "scene_sha256": FileAccess.get_sha256(options.scene), "layout_seed": scene.get_node("World").seed, "statistics": stats}, "\t"))
		print("HEIGHTMAP_EXPORTED: ", JSON.stringify(stats.heightmap))
	else:
		push_error("Heightmap export failed: " + view.status + " " + str(stats.get("heightmap_error", "")))
	scene.queue_free()
	await process_frame
	quit(0 if success else 1)
