"""Own pipe-frame cockpit, sized from the previous Blender draft. Run in Blender."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

OUT = Path(sys.argv[sys.argv.index('--') + 1])
ROOT = Path(__file__).resolve().parents[2]
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
roles = {
    'Paint': (0.72, 0.025, 0.035, 1), 'Metal': (0.5, 0.56, 0.62, 1),
    'Dark': (0.025, 0.032, 0.042, 1), 'Gold': (0.95, 0.59, 0.06, 1),
    'SteeringDark': (0.02, 0.025, 0.03, 1), 'SteeringMetal': (0.5, 0.56, 0.62, 1),
    'FrontTire': (0.02, 0.025, 0.03, 1), 'FrontHub': (0.95, 0.59, 0.06, 1),
    'RearTire': (0.02, 0.025, 0.03, 1), 'RearHub': (0.95, 0.59, 0.06, 1),
}
for role, color in roles.items():
    mat = bpy.data.materials.new(role)
    mat.diffuse_color = color
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = color
    shader.inputs['Metallic'].default_value = 0.65 if role in ('Metal', 'Gold') or 'Hub' in role else 0.12
    shader.inputs['Roughness'].default_value = 0.35


def coord(p):
    return Vector((p[0], -p[2], p[1]))


def finish(obj, name, role):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    obj.name = name
    obj.data.materials.append(bpy.data.materials[role])
    obj['export_role'] = role
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return obj


def tube(name, a, b, radius, role, sides=10):
    a, b = coord(a), coord(b)
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=(b-a).length, location=(a+b)/2)
    obj = bpy.context.object
    obj.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    return finish(obj, name, role)


def box(name, center, size, role, bevel=0.12):
    bpy.ops.mesh.primitive_cube_add(size=1, location=coord(center))
    obj = bpy.context.object
    obj.scale = (size[0], size[2], size[1])
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    mod = obj.modifiers.new('rounded_edges', 'BEVEL')
    mod.width = bevel
    mod.segments = 2
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return finish(obj, name, role)


def ring(name, radius, thickness, role, center=(0, 0, 0), axle=False, segments=32):
    bpy.ops.mesh.primitive_torus_add(major_segments=segments, minor_segments=8,
        major_radius=radius, minor_radius=thickness, location=coord(center))
    obj = bpy.context.object
    # Torus starts in Blender XY. Export steering is game XY; tires game YZ.
    obj.rotation_euler = (0, math.pi/2, 0) if axle else (math.pi/2, 0, 0)
    return finish(obj, name, role)


# Tubular perimeter, front bumper and side rails; all heights relative to wheel centers.
for side in (-1, 1):
    x = side * 2.7
    points = [(x, -0.25, -4.4), (x, -0.25, 3.5), (side*2.0, 0, 5.1)]
    for i, (a, b) in enumerate(zip(points, points[1:])):
        tube('frame_rail', a, b, 0.22, 'Paint')
    tube('side_bar', (side*3.2, 0.4, -2.8), (side*3.2, 0.4, 2.2), 0.2, 'Paint')
    for z in (-2.8, 2.2):
        tube('side_bar_mount', (x, -0.25, z), (side*3.2, 0.4, z), 0.16, 'Metal')
    box('side_pod', (side*2.9, 0.5, -0.4), (1.0, 0.8, 3.1), 'Paint', 0.25)
    tube('front_spindle', (side*2, 0, 3.75), (side*3.2, 0, 3.75), 0.15, 'Metal')
tube('front_bumper', (-2, 0, 5.1), (2, 0, 5.1), 0.25, 'Paint')
tube('rear_bumper', (-3, 0.15, -4.6), (3, 0.15, -4.6), 0.23, 'Paint')
tube('rear_axle', (-4.1, 0, -3.75), (4.1, 0, -3.75), 0.16, 'Metal')
box('floor_pan', (0, -0.18, 0.0), (4.2, 0.16, 6.7), 'Metal', 0.06)
box('nose_fairing', (0, 0.38, 3.9), (3.8, 0.85, 1.2), 'Paint', 0.3)
box('nose_inset', (0, 0.82, 3.9), (1.1, 0.035, 0.75), 'Dark', 0.015)
box('seat_cushion', (0, 0.45, -1.4), (2.7, 0.55, 2.8), 'Dark', 0.22)
box('seat_back', (0, 1.75, -2.65), (2.8, 2.7, 0.6), 'Dark', 0.25)
for x in (-1.35, 1.35):
    box('seat_bolster', (x, 0.9, -1.45), (0.4, 0.85, 2.7), 'Dark', 0.15)
box('engine_block', (1.8, 0.9, -3.65), (1.5, 1.8, 1.5), 'Dark')
for y in (0.4, 0.7, 1, 1.3, 1.6):
    box('cooling_fin', (1.8, y, -3.65), (1.7, 0.08, 1.6), 'Metal', 0.025)
tube('exhaust', (-1.8, 0.4, -3.5), (-1.8, 1.8, -4.3), 0.32, 'Metal', 16)
for x in (-0.85, 0.85):
    box('pedal', (x, 0.25, 2.5), (0.6, 0.25, 0.95), 'Dark', 0.07)
tube('steering_column', (0, 0, 2.7), (0, 2.15, 1.6), 0.16, 'Metal')

# Steering and wheel geometry stay local; the runtime owns their pivots/animation.
ring('steering_rim', 1.03, 0.14, 'SteeringDark')
for angle in (math.pi/6, 5*math.pi/6, 3*math.pi/2):
    tube('steering_spoke', (0, 0, 0), (math.cos(angle)*0.95, math.sin(angle)*0.95, 0), 0.08, 'SteeringMetal', 8)
tube('steering_hub', (0, 0, -0.12), (0, 0, 0.12), 0.23, 'SteeringMetal', 16)
for prefix, radius, width in [('Front', 1.16, 1.3), ('Rear', 1.5, 1.7)]:
    # Lathed tire profile: shoulders and central tread, without expensive topology.
    profile = [(-width/2, .6*radius), (-width/2, .85*radius), (-width*.35, radius),
               (width*.35, radius), (width/2, .85*radius), (width/2, .6*radius)]
    vertices, faces = [], []
    for x, r in profile:
        for j in range(32):
            a = 2*math.pi*j/32
            vertices.append(coord((x, math.cos(a)*r, math.sin(a)*r)))
    for i in range(len(profile)):
        for j in range(32):
            faces.append((i*32+j, i*32+(j+1)%32, ((i+1)%len(profile))*32+(j+1)%32, ((i+1)%len(profile))*32+j))
    mesh = bpy.data.meshes.new(prefix+'Tire')
    mesh.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(prefix+'Tire', mesh)
    bpy.context.collection.objects.link(obj)
    finish(obj, prefix+'Tire', prefix+'Tire')
    tube('wheel_hub', (-width/2-.03, 0, 0), (width/2+.03, 0, 0), radius*.59, prefix+'Hub', 24)
    for side in (-1, 1):
        for j in range(6):
            a = j*math.pi/3
            r = radius*.37
            tube('hub_bolt', (side*(width/2+.04), math.cos(a)*r, math.sin(a)*r),
                 (side*(width/2+.10), math.cos(a)*r, math.sin(a)*r), .075, prefix+'Tire', 6)


def export():
    groups = {role: [] for role in roles}
    light = Vector((-0.4, 0.85, 0.35)).normalized()
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or 'export_role' not in obj: continue
        mesh = obj.data
        mesh.calc_loop_triangles()
        for tri in mesh.loop_triangles:
            normal = tri.normal
            n = Vector((normal.x, normal.z, -normal.y))
            shade = round(255 * (0.42 + 0.58 * max(0, n.dot(light))))
            for index in tri.vertices:
                co = mesh.vertices[index].co
                p = (round(co.x*32), round(co.z*32), round(-co.y*32))
                assert max(abs(v) for v in p) < 32767
                groups[obj['export_role']].append((*p, shade))
    lines = ['/* Generated by tools/kart_assets/author_cockpit.py. Own geometry; game X/Y/Z. */',
             '#define FP_COCKPIT_SCALE (1.0f / 32.0f)']
    for role, vertices in groups.items():
        if not vertices:
            continue
        name = 'sCockpit'+role
        lines += [f'static Vtx {name}Vtx[] = {{']
        lines += ['    {{{%d,%d,%d},0,{0,0},{%d,%d,%d,255}}},' % (x,y,z,c,c,c) for x,y,z,c in vertices]
        lines += ['};', f'static Gfx {name}Dl[] = {{']
        for start in range(0, len(vertices), 30):
            count = min(30, len(vertices)-start)
            lines.append(f'    gsSPVertex({name}Vtx + {start}, {count}, 0),')
            for i in range(0, count, 3):
                lines.append(f'    gsSP1Triangle({i}, {i+1}, {i+2}, 0),')
        lines += ['    gsSPEndDisplayList(),', '};']
    (ROOT / 'src/first_person_cockpit.inc.c').write_text('\n'.join(lines)+'\n')
    (OUT / 'mesh-stats.json').write_text(json.dumps({k: len(v)//3 for k,v in groups.items()}, indent=2))


export()
# Keep an editable assembled preview. Export already happened in each part's local space.
for obj in list(bpy.data.objects):
    role = obj.get('export_role', '')
    if role.startswith('Steering'):
        obj.location = coord((0, 2.15, 1.6))
    if role.startswith(('Front', 'Rear')):
        front = role.startswith('Front')
        obj.location = coord((-3.15 if front else -4.15, 0, 3.75 if front else -3.75))
        twin = obj.copy(); twin.data = obj.data.copy()
        bpy.context.collection.objects.link(twin)
        twin.location.x *= -1
bpy.ops.object.camera_add(location=(12, 15, 10))
camera = bpy.context.object
camera.rotation_euler = (Vector((0, 0, 0.5))-camera.location).to_track_quat('-Z', 'Y').to_euler()
camera.data.type = 'ORTHO'; camera.data.ortho_scale = 18
bpy.context.scene.camera = camera
for location, energy, size in [((4, 3, 12), 1700, 8), ((-7, 0, 7), 1200, 7)]:
    bpy.ops.object.light_add(type='AREA', location=location)
    bpy.context.object.data.energy = energy
    bpy.context.object.data.shape = 'DISK'; bpy.context.object.data.size = size
scene = bpy.context.scene
scene.render.engine = 'CYCLES'; scene.cycles.samples = 24
scene.world.color = (0.22, 0.22, 0.22)
scene.render.resolution_x = 1000; scene.render.resolution_y = 850; scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.filepath = str(OUT / 'cockpit-preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'pipe-frame-cockpit.blend'))
bpy.ops.render.render(write_still=True)
