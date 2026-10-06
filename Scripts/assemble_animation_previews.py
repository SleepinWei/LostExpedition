"""Assemble real Unreal captures into small README animation previews (requires Pillow)."""
from pathlib import Path
from PIL import Image

project = Path(__file__).resolve().parents[1]
source = project / 'Docs/AnimationFrames'
output = project / 'Docs/Images'
output.mkdir(exist_ok=True)
for name, start, end in [('climbing-animation', 0, 160), ('firing-animation', 160, 240)]:
    paths = [source / f'{index:04d}.png' for index in range(start, end, 2)]
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
    for width in [480, 400, 360, 320]:
        frames = []
        for path in paths:
            with Image.open(path) as capture:
                frame = capture.convert('RGB')
                frame.thumbnail((width, width * 5 // 8), Image.Resampling.LANCZOS)
                frames.append(frame.quantize(palette=palette, dither=Image.Dither.NONE))
        frames[0].save(destination, save_all=True, append_images=frames[1:], duration=83, loop=0, optimize=True)
        if destination.stat().st_size < 2_500_000:
            break
    else:
        raise SystemExit(f'{destination.name} exceeds the 2.5 MB documentation preview budget')
    print(f'{destination.name}: {len(frames)} frames, {destination.stat().st_size} bytes, {frames[0].size}')
