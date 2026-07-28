extends Node

# M0 smoke test (task 0.6): load the extension, ping it, then open the .usda
# fixture and print its prim tree. Exits nonzero on failure so CI (task 0.7)
# can assert on the process exit code, not just scrape output.
func _ready() -> void:
	var bridge := UsdBridge.new()
	print(bridge.ping())

	# USD does its own file I/O and cannot read res:// paths, so resolve to an
	# absolute OS path on the GDScript side; open_stage() takes a real
	# filesystem path by design. Note globalize_path() only resolves res://
	# like this in the editor / unexported context — fine for the smoke test,
	# revisit when fixtures must work from an exported .pck.
	var fixture_path := ProjectSettings.globalize_path("res://fixtures/smoke.usda")

	if not bridge.open_stage(fixture_path):
		push_error("Smoke test FAILED: could not open stage at " + fixture_path)
		get_tree().quit(1)
		return

	var root: Node3D = bridge.import_stage(fixture_path)
	if root == null:
		push_error("Smoke test FAILED: import_stage returned null for " + fixture_path)
		get_tree().quit(1)
		return
	add_child(root)
	print("Imported root: ", root.name, " (", root.get_child_count(), " children)")

	# Headless (CI) must exit so the run can be asserted on; a windowed run
	# (F5) stays open so the imported tree can be inspected in the Remote
	# scene dock — Node3Ds have no visual, so that dock is the only view of it.
	# Detect via DisplayServer, not OS.get_cmdline_args(): Godot strips
	# engine-consumed flags like --headless before scripts ever see them.
	if DisplayServer.get_name() == "headless":
		get_tree().quit(0)
