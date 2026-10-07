# Motion Matching integration

Ground locomotion now evaluates Epic's `FAnimNode_MotionMatching` inside the compiled `/Game/Animation/MotionMatching/ABP_ExplorerMotionMatching` animation blueprint. The native animation instance supplies the query; this is actual Pose Search selection of animation frames.

## Content and graph

| Database | Content | Indexed poses at 30 Hz |
| --- | --- | --- |
| `PSD_Unarmed` | Idle, eight walk directions, eight jog directions | 1,055 |
| `PSD_Pistol` | ADS idle, eight walk directions, eight jog directions | 928 |
| `PSD_Rifle` | ADS idle, eight walk directions, eight jog directions | 927 |

Each database uses exact full-feature search at this small size, avoiding loss of directional candidates from the default four-dimensional PCA projection.

All 51 sequences come from the installed UE 5.8 High mannequin template. Generated copies under `MotionMatching/Sequences` enable looping and root extraction; the original template files are preserved. Pose Search indexes the animation's recorded root trajectory. `IgnoreRootMotion` keeps capsule movement under CharacterMovement or collision-safe traversal control.

`PSS_Explorer` matches horizontal velocity and facing at -0.15, 0.1, 0.25 and 0.5 seconds, plus pelvis/foot positions and velocities from the character pose. Trajectory weight is 12 and pose weight is 0.5: movement intent takes priority over remaining in an idle pose; foot features still guide transition phase. Continuing-pose cost bias is -0.003. The history collector stores 12 poses at 30 Hz. The native query contains actual movement history and predicts acceleration, braking, maximum speed and bounded facing changes through 0.85 seconds in world mesh space.

The graph blends Motion Matching with an explicit-time jump/fall/land sequence evaluator, then collects pose history before its final output. Motion Matching uses its internal blend stack with a 0.18-second transition and a 0.65–1.8 playback range. Weapon database changes interrupt a continuing result outside the allowed database; an old unarmed pose cannot persist after entering pistol/rifle aim.

The hidden skeletal mesh evaluates this graph. Its resulting pose feeds upper-body fire/reload/equip layers, full-body transition correction and climbing contact IK. The completed pose is then retargeted to the clothed Diesel character. The old directional weapon selector runs only when the generated Motion Matching blueprint is unavailable.

## Restore or regenerate

1. Restore official template content with `python3 Scripts/restore_engine_assets.py --engine "/Users/Shared/Epic Games/UE_5.8"`.
2. Close Unreal and compile with `Scripts/Build.command`.
3. Run `Scripts/SetupMotionMatching.command`, or execute `Scripts/setup_motion_matching.py` in the editor's Python menu. This creates the schema, three indexed databases and the compiled AnimBlueprint. It rewrites only this generated animation folder; keep manual variants elsewhere.
4. Restart the editor/game so the character loads the generated class and database references.
5. Run `Scripts/SmokeTest.command`; inspect [runtime-test.txt](runtime-test.txt) and [motion-matching-setup.txt](motion-matching-setup.txt).

Content assets and full capture frames remain local. C++, setup scripts, configuration and reports are in Git. There is no Fab login or Game Animation Sample requirement for this integration. Adjust the engine path in the macOS command scripts for another installation; other platforms need their Unreal build and commandlet equivalents.

## Verification and coverage

The UE 5.8.3 Mac Development editor build succeeds; the runtime suite passes **121 checks with zero failures**. Recorded rifle strafe/backward foot excursions are 62.78/53.13 cm across the tested cycles.

The runtime suite evaluates the actual AnimGraph while advancing CharacterMovement. It checks an unarmed forward jog, braking to idle, pistol strafe, rifle backward movement, database switching, past/future query samples, jump/fall/land blending and absence of animation-driven capsule displacement. It retains the complete tower route, contact, weapon, item and save regressions. Selected animation names, database names, times and costs are recorded in the report.

`Scripts/MotionMatchingReview.command` requests 288 real game frames at a fixed animation/movement timestep of 1/24 second. The sequence includes start, sprint, 90-degree turn, stop, pistol strafing and fire, rifle backward fire and a jump. Frames stay in `Docs/MotionMatchingFrames/`; the compact preview is in the README. Assemble it with `python3 Scripts/assemble_animation_previews.py --motion-matching-only` using Pillow. The assembly script uses only screenshots completed in the current run and substitutes the nearest completed frame if rendering coalesces a request, preventing reuse of an old PNG. This capture uses actual animation evaluation and movement, with a following review camera.

The databases contain locomotion loops. Dedicated start/stop, pivot, stumble and varied landing clips are still absent. Wall climbing samples full-body leap/probe/catch sequences and phase-weighted contact IK described in [WALL_CLIMB.md](WALL_CLIMB.md). Eight generated action sequences are sampled at runtime. Matching plus blending improves selection and transitions but cannot replace missing animation coverage. Climbing uses explicit probe/leap/catch phases with procedural contact correction; root-motion Motion Warping is not active.

[Epic's Motion Matching documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-matching-in-unreal-engine) describes schema, database, trajectory and pose-history setup. [Game Animation Sample](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) remains an optional source for expanding authored coverage; its content has not been imported.
