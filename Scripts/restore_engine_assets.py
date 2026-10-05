"""Copy the required template assets from your own UE 5.8 installation."""
import argparse
import shutil
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--engine', type=Path, default=Path('/Users/Shared/Epic Games/UE_5.8'))
parser.add_argument('--check', action='store_true', help='Validate source and installed files without copying')
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
resources = args.engine / 'Templates/TemplateResources'
packages = [('High/Characters/Content', 'Characters'),
            ('Standard/Weapons/Content', 'Weapons'),
            ('Standard/ArchVis/Content/SampleScene/Tree', 'ArchVis/SampleScene/Tree')]
for source, target in packages:
    source = resources / source
    destination = project / 'Content' / target
    if not source.is_dir():
        raise SystemExit(f'Missing template resources: {source}. Install UE 5.8 template content first.')
    files = list(source.rglob('*.uasset'))
    if args.check:
        missing = [p.relative_to(source) for p in files if not (destination / p.relative_to(source)).is_file()]
        if missing:
            raise SystemExit(f'{target}: {len(missing)} files missing; rerun without --check.')
    else:
        shutil.copytree(source, destination, dirs_exist_ok=True)
    print(f'{target}: {len(files)} assets ready')
