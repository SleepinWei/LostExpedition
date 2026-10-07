"""Merge the CC0 Diesel skin pieces into one GLB, preserving embedded textures.
The source download stays local. Run with ordinary Python before UE import.
"""
from pathlib import Path
import struct,json
root=Path(__file__).resolve().parents[1]/'ArtSource/Characters'
data=(root/'Diesel.glb').read_bytes();size=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+size]);binary=data[20+size:]
primitives=[]
for node in doc['nodes']:
    if 'mesh' in node:
        if node.get('name') not in ('big_eyebrow.002','small_eye_brow.002'):
            primitives.extend(doc['meshes'][node['mesh']]['primitives'])
        del node['mesh'];node.pop('skin',None)
index=len(doc['meshes']);doc['meshes'].append({'name':'SK_Diesel','primitives':primitives})
nodeindex=len(doc['nodes']);doc['nodes'].append({'name':'SK_Diesel','mesh':index,'skin':0})
# The mesh and bones share the original Armature space.
armature=next(n for n in doc['nodes'] if n.get('name')=='Armature');armature['children'].append(nodeindex)
encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*((-len(encoded))%4)
body=struct.pack('<I4s',len(encoded),b'JSON')+encoded+binary
(root/'Explorer.glb').write_bytes(struct.pack('<4sII',b'glTF',2,12+len(body))+body)
print('Explorer.glb: merged',len(primitives),'material primitives')
