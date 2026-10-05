"""Download selected CC0 assets from Poly Haven, checking published MD5 hashes."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib, json, urllib.request

ROOT = Path(__file__).resolve().parents[1] / 'ArtSource' / 'CoastalRemake'
ROOT.mkdir(parents=True, exist_ok=True)
MODELS = ['coastal_cliff_02', 'rock_07', 'fern_02', 'wooden_crate_02', 'wooden_barrels_01']
TEXTURES = ['old_stone_wall_02', 'rock_face', 'aerial_grass_rock', 'worn_mossy_plasterwall']

def fetch(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'LostExpedition-AssetImport/1.0'})
    with urllib.request.urlopen(request, timeout=90) as response:
        return response.read()

def download(job):
    relative, info = job
    file = ROOT / relative
    file.parent.mkdir(parents=True, exist_ok=True)
    if not file.exists() or hashlib.md5(file.read_bytes()).hexdigest() != info['md5']:
        data = fetch(info['url'])
        assert hashlib.md5(data).hexdigest() == info['md5'], relative
        file.write_bytes(data)
    print('Ready:', relative, flush=True)

manifest = {'license': 'CC0', 'models': [], 'textures': []}
jobs = []
for asset_id in MODELS + TEXTURES:
    data = json.loads(fetch('https://api.polyhaven.com/files/' + asset_id))
    (ROOT / (asset_id + '-files.json')).write_text(json.dumps(data, indent=2))
    entry = {'id': asset_id, 'source': 'https://polyhaven.com/a/' + asset_id, 'files': {}}
    if asset_id in MODELS:
        info = data['fbx']['2k']['fbx']
        entry['file'] = asset_id + '/' + asset_id + '.fbx'
        jobs.append((entry['file'], info))
    for channel in ['Diffuse', 'nor_dx', 'Rough', 'Alpha']:
        if channel not in data:
            continue
        variants = data[channel]['2k']
        info = variants.get('png' if channel == 'Alpha' else 'jpg') or variants.get('png')
        if info:
            relative = asset_id + '/' + info['url'].rsplit('/', 1)[-1]
            entry['files'][channel] = relative
            jobs.append((relative, info))
    manifest['models' if asset_id in MODELS else 'textures'].append(entry)
with ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(download, jobs))
(ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2))
print('COASTAL_DOWNLOAD_COMPLETE', flush=True)
