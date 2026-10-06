"""Regenerate Pose Search databases and compiled AnimGraph from installed UE templates."""
import os
import unreal
result = unreal.ExpeditionMotionMatchingLibrary.build_motion_matching_content()
unreal.log(result)
path = os.path.join(unreal.Paths.project_dir(), "Docs", "motion-matching-setup.txt")
with open(path, "w", encoding="utf-8") as report:
    report.write(result + "\n")
if "MM_SETUP_COMPLETE" not in result:
    raise RuntimeError(result)
