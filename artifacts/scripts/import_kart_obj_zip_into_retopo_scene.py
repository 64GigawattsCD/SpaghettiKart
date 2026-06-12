import os
import zipfile

import bpy

scene_path = r"E:\SpaghettiKart\artifacts\first-person-kart-screens\meshy_kart_retopo_source.blend"
zip_path = r"E:\MarioArt\Meshes\Kart_OBJ.zip"
extract_root = r"E:\SpaghettiKart\artifacts\first-person-kart-screens\Kart_OBJ_import"
collection_name = "Kart_OBJ_zip_import"


def clear_existing_collection(name):
    collection = bpy.data.collections.get(name)
    if collection is None:
        return

    for obj in list(collection.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.collections.remove(collection)


def import_obj(filepath):
    before = set(bpy.context.scene.objects)

    if hasattr(bpy.ops.wm, "obj_import"):
        bpy.ops.wm.obj_import(filepath=filepath)
    else:
        bpy.ops.import_scene.obj(filepath=filepath)

    return [obj for obj in bpy.context.scene.objects if obj not in before]


if bpy.data.filepath != scene_path:
    bpy.ops.wm.open_mainfile(filepath=scene_path)

os.makedirs(extract_root, exist_ok=True)
with zipfile.ZipFile(zip_path, "r") as archive:
    archive.extractall(extract_root)

clear_existing_collection(collection_name)
import_collection = bpy.data.collections.new(collection_name)
bpy.context.scene.collection.children.link(import_collection)

obj_files = []
for root, _dirs, files in os.walk(extract_root):
    for filename in files:
        if filename.lower().endswith(".obj"):
            obj_files.append(os.path.join(root, filename))
obj_files.sort()

imported_meshes = []
for filepath in obj_files:
    part_name = os.path.splitext(os.path.basename(filepath))[0]
    imported = import_obj(filepath)

    for obj in imported:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
        import_collection.objects.link(obj)

        obj.name = f"zip_{part_name}"
        if obj.type == "MESH":
            obj.data.name = f"{obj.name}_mesh"
            imported_meshes.append(obj)

        obj.select_set(False)

for obj in imported_meshes:
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
    obj.select_set(False)
    print(
        f"{obj.name}: verts={len(obj.data.vertices)} polys={len(obj.data.polygons)} "
        f"dims=({obj.dimensions.x:.3f}, {obj.dimensions.y:.3f}, {obj.dimensions.z:.3f})"
    )

bpy.ops.wm.save_as_mainfile(filepath=scene_path)
print(f"Imported {len(imported_meshes)} OBJ mesh objects from {zip_path}")
print(f"Saved {scene_path}")
