import unreal
context = unreal.load_asset('/Game/VehicleTemplate/Input/IMC_Default')
key = unreal.Key()
key.import_text('Gamepad_LeftY')
for name, negative in [('IA_Throttle', False), ('IA_Brake', True)]:
    action = unreal.load_asset('/Game/VehicleTemplate/Input/Actions/' + name)
    if name == 'IA_Brake':
        # Analog braking must update while held, not just on press/release edges.
        action.set_editor_property('triggers', [])
        unreal.EditorAssetLibrary.save_loaded_asset(action)
    context.unmap_key(action, key)
    modifier = unreal.new_object(unreal.StickHalfAxisModifier, outer=context)
    modifier.set_editor_property('negative_direction', negative)
    modifier.set_editor_property('dead_zone', 0.15)
    modifier.set_editor_property('exponent', 1.0)
    mapping = context.map_key(action, key)
    mapping.set_editor_property('modifiers', [modifier])
    # Python exposes mappings as struct copies; explicitly write the array back.
    mappings = list(context.get_editor_property('mappings'))
    mappings[-1] = mapping
    context.set_editor_property('mappings', mappings)
unreal.EditorAssetLibrary.save_loaded_asset(context)
for name in ['IA_Throttle', 'IA_Brake']:
    action = unreal.load_asset('/Game/VehicleTemplate/Input/Actions/' + name)
    matches = [m for m in context.get_editor_property('mappings')
               if m.action.get_path_name() == action.get_path_name()
               and m.key.export_text() == key.export_text()]
    assert len(matches) == 1 and len(matches[0].modifiers) == 1, name
unreal.log('Stick movement mappings: PASS')
