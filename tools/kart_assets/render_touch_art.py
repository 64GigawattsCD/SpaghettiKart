"""Blender batch job: orthographic, transparent N64-style touch hardware sprites."""
import math
import sys
from pathlib import Path

import bpy

OUT = Path(sys.argv[sys.argv.index('--') + 1])
scene = bpy.context.scene
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene.render.engine = 'CYCLES'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x = scene.render.resolution_y = 128
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.view_settings.view_transform = 'Standard'
scene.world.color = (.35, .35, .35)
bpy.ops.object.camera_add(location=(0, 0, 8))
scene.camera = bpy.context.object
scene.camera.data.type = 'ORTHO'
scene.camera.data.ortho_scale = 2.14
bpy.ops.object.light_add(type='AREA', location=(-3, 4, 6))
light = bpy.context.object
light.data.energy = 650
light.data.shape = 'DISK'
light.data.size = 4
light.rotation_euler = (-light.location).to_track_quat('-Z', 'Y').to_euler()


def material(name, color, metallic=0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1)
    result.use_nodes = True
    shader = result.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Metallic'].default_value = metallic
    shader.inputs['Roughness'].default_value = .3
    return result


black = material('Ink outline', (.008, .01, .016))
silver = material('Pearl highlight', (.75, .82, .95), .25)
palette = {
    'green': (.12, .75, .025), 'red': (.95, .045, .025),
    'blue': (.025, .45, 1), 'gold': (1, .63, .015),
    'violet': (.56, .23, .92), 'silver': (.68, .76, .87),
}
colors = {name: material(name, color, .12) for name, color in palette.items()}
hardware = []


def lathe(name, profile, materials, bands):
    count = 64
    vertices = [(radius * math.cos(i * math.tau / count),
                 radius * math.sin(i * math.tau / count), z)
                for radius, z in profile for i in range(count)]
    faces = [(j*count+i, j*count+(i+1)%count,
              (j+1)*count+(i+1)%count, (j+1)*count+i)
             for j in range(len(profile)-1) for i in range(count)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    for mat in materials:
        mesh.materials.append(mat)
    for face in mesh.polygons:
        face.material_index = bands[face.index // count]
    hardware.append(obj)
    return obj


def render(name, objects):
    for obj in hardware:
        obj.hide_render = obj not in objects
    scene.render.filepath = str(OUT / (name + '.png'))
    bpy.ops.render.render(write_still=True)


# The center stays completely transparent; Android supplies the translucent fill.
for name, color in colors.items():
    rim = lathe('Rim ' + name,
                [(1, 0), (.98, .04), (.92, .15), (.84, .15), (.78, .03), (.76, 0)],
                [black, color], [0, 1, 1, 1, 0])
    render('touch_rim_' + name, [rim])

knob = lathe('Shift knob', [(0, 0), (.9, 0), (1, .08), (.94, .21),
                          (.74, .42), (.4, .55), (0, .59)],
             [black, colors['gold'], silver], [0, 0, 1, 1, 1, 2])
render('touch_shift_knob', [knob])

wheel = lathe('Steering rim', [(1, 0), (.98, .14), (.89, .23),
                             (.76, .23), (.68, .10), (.68, 0)],
              [black, silver], [0, 0, 0, 1, 0])
hub = lathe('Wheel hub', [(0, .19), (.24, .19), (.24, .32), (.18, .38), (0, .38)],
            [black, silver], [0, 0, 1, 1])
spokes = []
for angle in (0, 120, 240):
    a = math.radians(angle-90)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(math.cos(a)*.45, math.sin(a)*.45, .16))
    spoke = bpy.context.object
    spoke.name = 'Wheel spoke'
    spoke.scale = (.65, .14, .10)
    spoke.rotation_euler.z = a
    spoke.data.materials.append(silver)
    hardware.append(spoke)
    spokes.append(spoke)
render('touch_steering', [wheel, hub, *spokes])


def pedal_part(name, location, size, mat, bevel):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    edge = obj.modifiers.new('Rounded pedal edge', 'BEVEL')
    edge.width = bevel
    edge.segments = 2
    hardware.append(obj)
    return obj


for name, width, height in [('accelerator', .78, 1.66), ('brake', 1.66, .98)]:
    parts = [pedal_part(name+' outline', (0, 0, .05), (width+.12, height+.12, .15), black, .10),
             pedal_part(name+' face', (0, 0, .16), (width, height, .16), silver, .08),
             pedal_part(name+' arm', (0, -.65, -.08), (.23, .62, .10), black, .03)]
    for offset in (-1, 0, 1):
        if name == 'accelerator':
            position, size = (offset*.18, 0, .27), (.07, height-.3, .07)
        else:
            position, size = (0, offset*.21, .27), (width-.3, .08, .07)
        parts.append(pedal_part(name+' tread', position, size, black, .025))
    render('touch_'+name, parts)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'touch-hardware.blend'))
