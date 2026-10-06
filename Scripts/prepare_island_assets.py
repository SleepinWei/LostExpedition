"""Build an original coconut palm mesh and fetch a small CC0 sand texture set."""
from pathlib import Path
import hashlib, json, math, urllib.request
root=Path(__file__).resolve().parents[1]/'ArtSource/Island'
root.mkdir(parents=True,exist_ok=True)
vertices=[];uvs=[];faces=[]
def vertex(p,uv=(0,0)):
    vertices.append(p);uvs.append(uv);return len(vertices)
def face(indices,material):faces.append((indices,material))
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
# Curved tapering trunk, with slight rings in the silhouette.
for j in range(33):
    t=j/32;radius=(25-13*t)*(1+.035*math.sin(j*math.pi/2))
    for k in range(12):
        a=k*2*math.pi/12
        vertex((150*t*t+radius*math.cos(a),40*t*t+radius*math.sin(a),1100*t),(k/12,t*12))
for j in range(32):
    for k in range(12):
        a=j*12+k+1;b=j*12+(k+1)%12+1;c=b+12;d=a+12
        face((a,b,c),'Bark');face((a,c,d),'Bark')
for frond in range(14):
    angle=frond*2.399963
    along=(math.cos(angle),math.sin(angle),0);side=(-along[1],along[0],0)
    length=480+(frond%4)*65;lift=170+(frond%3)*55;drop=200+(frond%5)*35
    def curve(t):return add((150,40,1080),add(mul(along,length*t),(0,0,lift*math.sin(math.pi*t)-drop*t*t)))
    # Thin rib and dozens of individual tapered leaflets produce a feathery crown.
    for j in range(24):
        t=j/24;n=(j+1)/24;p=curve(t);q=curve(n)
        ids=[vertex(add(p,mul(side,-3*(1-t))),(0,t)),vertex(add(p,mul(side,3*(1-t))),(1,t)),vertex(add(q,mul(side,3*(1-n))),(1,n)),vertex(add(q,mul(side,-3*(1-n))),(0,n))]
        face((ids[0],ids[1],ids[2]),'Leaf');face((ids[0],ids[2],ids[3]),'Leaf')
    for j in range(2,25):
        t=j/26;p=curve(t);blade=(45+150*math.sin(math.pi*t))*(1-.22*t)
        for sign in [-1,1]:
            direction=add(mul(side,sign),mul(along,.35));mid=add(p,add(mul(direction,blade*.5),(0,0,-12)))
            tip=add(p,add(mul(direction,blade),(0,0,-65-30*t)))
            width=8+5*math.sin(math.pi*t)
            ids=[vertex(p,(.5,0)),vertex(add(mid,mul(along,-width)),(0,.5)),vertex(add(mid,(0,0,3)),(.5,.5)),vertex(add(mid,mul(along,width)),(1,.5)),vertex(tip,(.5,1))]
            for tri in [(0,1,2),(0,2,3),(1,4,2),(2,4,3)]:face(tuple(ids[i] for i in tri),'Leaf')
lines=['mtllib coconut_palm.mtl','o CoconutPalm']
lines += ['v %.5f %.5f %.5f'%v for v in vertices]
lines += ['vt %.5f %.5f'%uv for uv in uvs]
material=None
for indices,m in faces:
    if material!=m:lines.append('usemtl '+m);material=m
    lines.append('f '+' '.join(f'{i}/{i}' for i in indices))
(root/'coconut_palm.obj').write_text('\n'.join(lines)+'\n')
(root/'coconut_palm.mtl').write_text('newmtl Bark\nKd .35 .24 .13\nnewmtl Leaf\nKd .05 .22 .025\n')
def fetch(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'LostExpedition/1.0'}),timeout=90) as r:return r.read()
data=json.loads(fetch('https://api.polyhaven.com/files/coast_sand_01'))
manifest={'id':'coast_sand_01','source':'https://polyhaven.com/a/coast_sand_01','license':'CC0','textures':{}}
for channel in ['Diffuse','nor_dx','Rough']:
    info=data[channel]['2k'].get('jpg') or data[channel]['2k']['png'];p=root/info['url'].rsplit('/',1)[-1]
    if not p.exists() or hashlib.md5(p.read_bytes()).hexdigest()!=info['md5']:
        content=fetch(info['url']);assert hashlib.md5(content).hexdigest()==info['md5'];p.write_bytes(content)
    manifest['textures'][channel]=p.name
(root/'manifest.json').write_text(json.dumps(manifest,indent=2))
print(f'ISLAND_ASSETS_READY: original palm {len(vertices)} vertices; sand textures verified')
