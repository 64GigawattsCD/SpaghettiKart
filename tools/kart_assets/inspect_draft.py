import bpy
import json
import sys
from pathlib import Path

out = Path(sys.argv[sys.argv.index('--') + 1])
items = []
for obj in bpy.data.objects:
    if obj.type == 'MESH':
        items.append({'name': obj.name, 'vertices': len(obj.data.vertices),
                      'location': list(obj.location), 'dimensions': list(obj.dimensions),
                      'materials': [m.name if m else None for m in obj.data.materials]})
(out / 'draft.json').write_text(json.dumps(items, indent=2))
