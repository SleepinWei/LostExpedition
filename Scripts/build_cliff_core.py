"""Closed eroded plateau core beneath the scanned cliff dressing (OBJ source)."""
from pathlib import Path
import math,random
r=random.Random(87);vertices=[];faces=[]
outline=[(-1,-.72),(-.83,-.98),(-.35,-1),(.35,-.98),(.83,-.94),(1,-.72),(1,-.25),(.96,.3),(.99,.75),(.73,.99),(.25,1),(-.35,.96),(-.8,.97),(-.99,.72),(-.98,.2),(-1,-.3)]
n=len(outline)
for j,(z,scale) in enumerate([(0,1),(-.15,1.04),(-.5,.91),(-1,.74)]):
 for x,y in outline:
  jitter=1 if j==0 else r.uniform(.88,1.1)
  vertices.append((x*100*scale*jitter,y*100*scale*jitter,z*200))
for j in range(3):
 for i in range(n):
  a=j*n+i;b=j*n+(i+1)%n;c=(j+1)*n+(i+1)%n;d=(j+1)*n+i
  faces.extend([('Stone',(a,c,b)),('Stone',(a,d,c))])
vertices.append((0,0,0));top=len(vertices)-1
for i in range(n):faces.append(('Earth',(top,i,(i+1)%n)))
vertices.append((0,0,-200));bottom=len(vertices)-1
for i in range(n):faces.append(('Stone',(bottom,3*n+(i+1)%n,3*n+i)))
lines=['mtllib cliff_core.mtl','o ClosedCliffCore']
for x,y,z in vertices:lines.append(f'v {x} {y} {z}') # Unreal OBJ import uses Z up
mat=''
for m,idx in faces:
 if m!=mat:lines.append('usemtl '+m);mat=m
 lines.append('f '+' '.join(str(i+1) for i in idx))
root=Path(__file__).resolve().parents[1]/'ArtSource/CoastalRemake'
(root/'cliff_core.obj').write_text('\n'.join(lines)+'\n')
(root/'cliff_core.mtl').write_text('newmtl Stone\nKd 0.4 0.4 0.4\nnewmtl Earth\nKd 0.2 0.3 0.1\n')
