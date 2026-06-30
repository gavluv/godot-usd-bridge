extends Node

# M0 smoke check: instantiate the GDExtension class and print its response.
# Quits immediately so `godot --headless --path demo` returns on its own.
func _ready() -> void:
	var bridge := UsdBridge.new()
	print(bridge.ping())
	get_tree().quit()
