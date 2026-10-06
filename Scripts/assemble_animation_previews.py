"""Assemble real Unreal captures into small README animation previews (requires Pillow)."""
import argparse
import re
from pathlib import Path
from PIL import Image

project = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--motion-matching-only', action='store_true')
args = parser.parse_args()
output = project / 'Docs/Images'
output.mkdir(exist_ok=True)
clips = [('motion-matching', 'MotionMatchingFrames', 0, 288)] if args.motion_matching_only else [('climbing-animation', 'AnimationFrames', 0, 160), ('firing-animation', 'AnimationFrames', 160, 330)]
for name, directory, start, end in clips:
    source = project / 'Docs' / directory
    stride = 3 if name == 'motion-matching' else 2
    indices = list(range(start, end, stride))
    if name == 'motion-matching':
        # A slow render can coalesce screenshot requests. Read the current run's
        # completed captures so a stale PNG from an older run never enters the GIF.
        log = project / 'Docs/mm-visual-review.log'
        if log.is_file():
            captured = sorted({int(value) for value in re.findall(r'Tracing Screenshot "(\d+)"', log.read_text(errors='replace'))})
            if not captured:
                raise SystemExit('No completed Motion Matching screenshots in the current capture log')
            indices = [min(captured, key=lambda value: abs(value - index)) for index in indices]
    paths = [source / f'{index:04d}.png' for index in indices]
    missing = [path.name for path in paths if not path.is_file()]
    if missing:
        raise SystemExit(f'Missing animation captures: {missing}')
    destination = output / f'{name}.gif'
    # Include every view in the palette so later rooftop ocean colors survive.
    swatches = Image.new('RGB', (96, 60 * len(paths)))
    for index, path in enumerate(paths):
        with Image.open(path) as capture:
            sample = capture.convert('RGB')
            sample.thumbnail((96, 60), Image.Resampling.LANCZOS)
            swatches.paste(sample, (0, index * 60))
    palette = swatches.quantize(colors=64)
    widths = [320, 300, 288] if name == 'motion-matching' else [480, 400, 360, 320]
    for width in widths:
        frames = []
        for path in paths:
            with Image.open(path) as capture:
                frame = capture.convert('RGB')
                frame.thumbnail((width, width * 5 // 8), Image.Resampling.LANCZOS)
                frames.append(frame.quantize(palette=palette, dither=Image.Dither.NONE))
        frames[0].save(destination, save_all=True, append_images=frames[1:], duration=round(stride * 1000 / 24), loop=0, optimize=True)
        if destination.stat().st_size < 2_500_000:
            break
    else:
        raise SystemExit(f'{destination.name} exceeds the 2.5 MB documentation preview budget')
    print(f'{destination.name}: {len(frames)} frames, {destination.stat().st_size} bytes, {frames[0].size}')
