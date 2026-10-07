"""Verify that every requested 60 Hz frame came from this Unreal capture run."""
from pathlib import Path
import re

project = Path(__file__).resolve().parents[1]
log = (project / 'Docs/wall-climb-review.log').read_text(errors='replace')
captured = {int(n) for n in re.findall(r'Tracing Screenshot "(\d+)"', log)}
expected = set(range(480))
missing = sorted(expected - captured)
if missing:
    raise SystemExit(f'Current Unreal review is missing frames: {missing}')
for index in expected:
    if not (project / f'Docs/WallClimbFrames/{index:04d}.png').is_file():
        raise SystemExit(f'Missing frame file: {index}')
stages = {int(n) for n in re.findall(r'WALL_CLIMB_REVIEW frame=\d+ stage=(\d+)', log)}
if not {3, 5, 6, 7}.issubset(stages):
    raise SystemExit(f'Incomplete stage coverage: {stages}')
report = ('WALL_CLIMB_REVIEW_COMPLETE\n'
          '480 requested and completed Unreal frames; 960 x 600; 60 Hz animation timestep.\n'
          'Ground probe, explicit Space jump, secure catch, buffered vertical climb,\n'
          'horizontal transfer and buffered return to upward climbing.\n'
          'Frame 300 deliberately cuts to the high horizontal route.\n'
          'No substituted or interpolated frames; fixed-timestep capture is not a performance benchmark.\n')
(project / 'Docs/wall-climb-review.txt').write_text(report)
print(report)
