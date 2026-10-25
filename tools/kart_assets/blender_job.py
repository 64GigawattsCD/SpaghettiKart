"""Isolated asset jobs. Own input meshes only; no game source writes."""
import importlib.util
import json
import math
from pathlib import Path
import re
import sys

import addon_utils
import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CACHE = ROOT / '.cache' / 'kart-tools'
COMMAND, OUTPUT, INPUT = sys.argv[sys.argv.index('--') + 1:]
OUT = Path(OUTPUT) if OUTPUT else None
# Right-handed conversion: author X/-Y/Z to game X/Z/Y (x, z, -y).
TO_GAME = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1)))


def write_json(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2), encoding='utf-8')


def configure():
    scene = bpy.context.scene
    scene.gameEditorMode = 'MK64'
    scene.f3d_type = 'F3DEX/LX'
    scene.blenderF3DScale = 1.0
    scene.exportInlineF3D = False
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.fast64.settings.dont_ask_color_management = True


def template():
    from fast64.fast64_internal.f3d.f3d_material import createF3DMat
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for name in ('KART_EXPORT', 'KART_REFERENCE', 'KART_MOUNTS'):
        collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(collection)
    for name in ('body', 'front_wheel', 'rear_wheel', 'steering_wheel', 'driver'):
        marker = bpy.data.objects.new(name + '_pivot', None)
        marker.empty_display_type = 'PLAIN_AXES'
        bpy.data.collections['KART_MOUNTS'].objects.link(marker)
    for role in ('body_recolor', 'body_silver', 'body_dark', 'body_accent', 'tire', 'hub'):
        mat = createF3DMat(None, preset='Shaded Solid')
        mat.name = role
        mat.use_fake_user = True
    bpy.context.scene['kart_author_axes'] = 'X lateral, -Y forward, Z up'
    bpy.context.scene['kart_export_axes'] = 'X lateral, Y up, Z forward'
    bpy.context.scene['kart_units'] = '1 Blender unit = 1 game unit; fit after sourcing model'
    bpy.context.scene['kart_template_version'] = 1
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'kart_authoring.blend'))


def audit():
    collection = bpy.data.collections.get('KART_EXPORT')
    objects = sorted(collection.all_objects, key=lambda o: o.name) if collection else []
    errors = []
    reports = []
    meshes = [o for o in objects if o.type == 'MESH']
    if not meshes:
        errors.append('KART_EXPORT needs at least one mesh')
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj in meshes:
        evaluated = obj.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        try:
            mesh.calc_loop_triangles()
            points = [v.co for v in mesh.vertices]
            bounds = [[min(p[i] for p in points), max(p[i] for p in points)] for i in range(3)] if points else []
            reports.append({'name': obj.name, 'vertices': len(points), 'triangles': len(mesh.loop_triangles),
                            'bounds_local': bounds, 'pivot_author': list(obj.location),
                            'pivot_game': list(TO_GAME.to_3x3() @ obj.location),
                            'materials': [m.name if m else None for m in mesh.materials],
                            'uv_layers': len(mesh.uv_layers),
                            'vertex_groups': len(obj.vertex_groups)})
            if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', obj.name):
                errors.append(f'{obj.name}: use an ASCII C identifier for mesh names')
            if obj.parent or any(abs(s - 1) > 1e-5 for s in obj.scale) or obj.matrix_basis.to_quaternion().angle > 1e-5:
                errors.append(f'{obj.name}: static export requires no parent and applied rotation/scale')
            if any(m.type == 'ARMATURE' for m in obj.modifiers):
                errors.append(f'{obj.name}: animated export is not wired; retain the rig in KART_REFERENCE')
            if not mesh.loop_triangles:
                errors.append(f'{obj.name}: no triangles')
            if any(t.area < 1e-10 for t in mesh.loop_triangles):
                errors.append(f'{obj.name}: degenerate triangles')
            if any(not math.isfinite(c) or abs(c) > 32760 for p in points for c in p):
                errors.append(f'{obj.name}: non-finite or out-of-range F3D coordinates')
            if any(not math.isfinite(c) for c in obj.location):
                errors.append(f'{obj.name}: non-finite pivot')
            if not mesh.uv_layers:
                errors.append(f'{obj.name}: add a UV map (also required for solid F3D materials)')
            if not mesh.materials or any(m is None or not m.is_f3d for m in mesh.materials):
                errors.append(f'{obj.name}: every material slot must use Fast64 materials')
            if any(p.material_index >= len(mesh.materials) for p in mesh.polygons):
                errors.append(f'{obj.name}: invalid face material index')
        finally:
            evaluated.to_mesh_clear()
    return {'schema': 1, 'source': INPUT or None, 'blender': bpy.app.version_string,
            'author_axes': 'X,-Y,Z', 'game_axes': 'X,Y,Z',
            'author_to_game': [list(row) for row in TO_GAME], 'units_scale': 1,
            'meshes': reports, 'errors': errors,
            'status': 'ready_for_static_export' if not errors else 'needs_attention'}


def export_assets():
    from fast64.fast64_internal.f3d.f3d_writer import exportF3DtoC, DLFormat, GfxMatWriteMethod
    report = audit()
    write_json('inspection.json', report)
    if report['errors']:
        raise RuntimeError('; '.join(report['errors']))
    manifest = {'schema': 1, 'runtime_installed': False, 'mesh_format': 'F3DEX C display lists',
                'author_to_game': report['author_to_game'], 'units_scale': 1, 'parts': []}
    for info in report['meshes']:
        original = bpy.data.objects[info['name']]
        depsgraph = bpy.context.evaluated_depsgraph_get()
        mesh = bpy.data.meshes.new_from_object(original.evaluated_get(depsgraph), depsgraph=depsgraph)
        obj = bpy.data.objects.new('staged_' + original.name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        try:
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            exportF3DtoC(str(OUT), obj, DLFormat.Static, TO_GAME, '', False, False,
                        original.name, GfxMatWriteMethod.WriteAll)
            manifest['parts'].append({'name': original.name, 'pivot_game': info['pivot_game'],
                                       'model': original.name + '/model.inc.c',
                                       'header': original.name + '/header.h'})
        finally:
            bpy.data.objects.remove(obj, do_unlink=True)
            if mesh.users == 0:
                bpy.data.meshes.remove(mesh)
    write_json('manifest.json', manifest)


def selftest(pop):
    from fast64.fast64_internal.f3d.f3d_material import createF3DMat
    from types import SimpleNamespace
    template()
    assert audit()['errors'], 'Empty template must not pass export validation'
    mesh = bpy.data.meshes.new('calibration')
    mesh.from_pydata([(0, 0, 0), (2, 3, 5), (7, 0, 0)], [], [(0, 1, 2)])
    mesh.uv_layers.new(name='UVMap')
    obj = bpy.data.objects.new('calibration', mesh)
    bpy.data.collections['KART_EXPORT'].objects.link(obj)
    obj.location = (11, 13, 17)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    createF3DMat(obj, preset='Shaded Solid')
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'calibration.blend'))
    assert not audit()['errors'], audit()
    obj.scale.x = -1
    assert audit()['errors'], 'Mirrored transform must fail'
    obj.scale.x = 1
    obj.rotation_euler.z = 0.5
    assert audit()['errors'], 'Unapplied rotation must fail'
    obj.rotation_euler.z = 0
    mesh.materials.clear()
    assert audit()['errors'], 'Missing materials must fail'
    mesh.materials.append(bpy.data.materials['body_recolor'])
    assert abs(TO_GAME.determinant() - 1) < 1e-6
    assert tuple(TO_GAME.to_3x3() @ Vector((2, 3, 5))) == (2, 5, -3)
    export_assets()
    text = (OUT / 'calibration/model.inc.c').read_text()
    assert re.search(r'\{\s*2\s*,\s*5\s*,\s*-3\s*\}', text), text
    assert 'gsSPVertex' in text and 'gsSPEndDisplayList' in text
    assert json.loads((OUT / 'manifest.json').read_text())['parts'][0]['pivot_game'] == [11, 17, -13]
    # Exercise the unmodified population exporter with our own tiny route.
    root = bpy.data.objects.new('Course Population', None)
    bpy.context.scene.collection.objects.link(root)
    curve = bpy.data.curves.new('test_route', 'CURVE')
    spline = curve.splines.new('POLY')
    spline.points.add(1)
    spline.points[0].co = (0, 0, 0, 1)
    spline.points[1].co = (0, 10, 0, 1)
    route = bpy.data.objects.new('test_route', curve)
    bpy.context.scene.collection.objects.link(route)
    route.parent = root
    pop.export_pop_data(bpy.context, SimpleNamespace(export_names=[SimpleNamespace(name='path', export=True)]),
                        str(OUT / 'test.OK64.POP3'), root, route, [route], [])
    assert '[0.00,10.00,0.00]' in (OUT / 'test_path.OK64.POP3').read_text()
    write_json('selftest.json', {'passed': True, 'blender': bpy.app.version_string,
               'checks': ['Fast64 registration and materials', 'empty template rejected',
                          'mesh validation', 'mirrored scale rejected', 'rotation rejected',
                          'missing materials rejected', 'handedness', 'exported vertex axes',
                          'pivot mapping', 'F3DEX C output', 'population path export']})


addon_utils.enable('fast64', default_set=True, persistent=True)
if not addon_utils.check('fast64')[1]:
    raise RuntimeError('Fast64 did not activate')
pop_path = CACHE / 'references/blender_pop_export/export_pop_v3_blender2,93.py'
spec = importlib.util.spec_from_file_location('kart_population_export', pop_path)
pop = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = pop
spec.loader.exec_module(pop)
pop.register()
if bpy.app.background:
    # The legacy success popup needs an editor window and crashes headless Blender.
    pop.show_message_box = lambda *args, **kwargs: print('POP export:', args, kwargs)
if INPUT:
    bpy.ops.wm.open_mainfile(filepath=INPUT)
configure()
if COMMAND == 'doctor':
    write_json('doctor.json', {'blender': bpy.app.version_string, 'fast64': addon_utils.check('fast64')[1],
                              'population_export': bool(bpy.ops.export_test.some_data.get_rna_type()),
                              'config': bpy.utils.user_resource('CONFIG'),
                              'scripts': bpy.utils.user_resource('SCRIPTS')})
elif COMMAND == 'template':
    template()
elif COMMAND == 'validate':
    report = audit()
    write_json('inspection.json', report)
    if report['errors']:
        raise RuntimeError('; '.join(report['errors']))
elif COMMAND == 'export':
    export_assets()
elif COMMAND == 'selftest':
    selftest(pop)
