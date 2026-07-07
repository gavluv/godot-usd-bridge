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

	get_tree().quit(0)
