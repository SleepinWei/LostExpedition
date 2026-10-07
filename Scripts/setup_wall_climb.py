"""Generate full-body wall-climbing AnimSequences from the project keyframe authoring code."""
from pathlib import Path
import unreal
report=unreal.ExpeditionMotionMatchingLibrary.build_wall_climb_content()
(Path(unreal.Paths.project_dir())/'Docs/wall-climb-setup.txt').write_text(report)
unreal.log(report)
assert 'WALL_CLIMB_SETUP_COMPLETE' in report, report
