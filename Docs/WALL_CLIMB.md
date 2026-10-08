# Three-stage wall climbing

The visible **TwinBlast ActionHero** character uses ground Motion Matching and weapon animation through Unreal's runtime IK Retargeter. Climbing now samples complete action sequences before retargeting, with **one final wall-contact solve on the visible rig**. The hidden source skeleton no longer solves the same contacts first.

| Stage | Input | Performance |
| --- | --- | --- |
| Reach / probe | Approach or E; WASD selects an adjacent hold | Feet stay planted; the supporting arm holds the current ledge while the free hand explores |
| Jump / grab | Space, with 0.16 s minimum probe time | Load the legs, push off, release the wall, swing the arms and tuck a leg, then reach for the destination |
| Secure catch | Automatic, 0.22 s | Hands engage in sequence, boots brace, torso absorbs and returns to hanging |

A jump lasts 0.88–0.96 s depending on distance. The collision capsule stays at its takeoff position for the first 20% while the skeleton anticipates, then follows a swept arc. Holding direction selects the next grip but waits for Space; one Space during flight queues one additional leap. Ctrl drops/cancels. Journal cancels an uncommitted probe. Obstacles interrupt the flight and restore gravity. The final hold supports mantling and reverse entry over the parapet.

## What was wrong and what changed

The previous revision sampled `AS_Hang` at time zero throughout climbing. Moving the capsule and interpolating hand targets could pass route checks while still showing no full-body jump. Source and target IK both constrained the limbs, including during flight. The baked source poses also used reversed anatomical sides and the wrong pitch axis in mannequin mesh space; final contact IK concealed those errors until constraints were released.

- Eight sequences now run at their actual phase: ground probe, left/right probe, left/right leap, catch, hang and a separate ground leap. Leap keys include preload, extension, asymmetric arm swing/leg tuck, reach and absorption. C1 key curves are baked at 60 Hz. Mesh-space +Y is forward and +X is anatomical left.
- `ExplorerClimbMotion.h` shares timing between collision travel and contact envelopes. Four contact weights become zero during flight. At zero weight the retargeted action animation is preserved; IK returns gradually near the destination.
- End-effector position and bend plane blend geometrically towards the contact solution. Blending complete solved joint quaternions had rotated knees through a large arc despite nearby foot positions. Persistent bend planes constrain elbow/pole flips.
- Wrist contact correction follows the release/arrival curve. Free-flight wrists retain the retargeted animation instead of being overwritten with a fixed palm pose. The correction keeps a continuous quaternion branch during partial blends to prevent a sudden flip when the relative rotation crosses 180 degrees. The runtime arm solve uses a fixed extension reserve. Re-applying a nonlinear soft-reach correction to an already-corrected, captured wrist point caused an elbow step on direction changes and is removed.
- During an interrupted probe, the displayed hand positions are captured and root support remains on the original ledge. Planted boots are world-space contacts. Pose springs act before final contact correction rather than repeatedly smoothing an already-constrained pose.

The eight support sequences remain original program-authored keyframes. **Epic Game Animation Sample 5.8 motion capture is now installed** alongside them:

| Action | Playback |
| --- | --- |
| Normal jump | `MC_M_Neutral_Jump_F_Start_Stand_Lfoot`, starting at 0.40 s (push-off); existing fall and landing transitions remain |
| Ground-to-wall | `MC_M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot`, 0.00–0.43 s while probing, a short 0.43–0.40 s compression, then 0.40–0.72 s push-off/reach; the short catch continues to 0.80 s while blending to hang |
| Wall-to-wall | Left/right 2.5 m climb clips, 0.40–0.74 s; up to 60% upper-body blend during flight; authored legs retain wall push-off and tuck |
| Rooftop pull-up | Left-foot 2.5 m climb, 0.80–2.20 s as a full-body action |
| One-metre mantle | Retargeted and available locally; not selected by this tower controller |

The captured root track is extracted. The swept gameplay path still owns capsule movement. Ground contacts release at the captured push-off; wall contacts use their separate support timeline. The animated elbow/knee plane is transported from its original limb axis to the target axis; direct projection could reverse it. Signed bend angles and wrist quaternions retain a continuous branch during partial IK blends. Stage transitions inherit final visible limb positions, while wrist contact rotation is excluded from the seed to avoid applying it twice. Hand axes and finger curl axes are calibrated from the selected model's reference knuckles rather than assuming Diesel's axes.

The half-second ground probe preserves the captured arm pose with only 35% reach correction; it does not plant the hands. Pelvis height respects both planted legs' reach after retargeting, and the exploratory hand arc bows toward the wall. On Space, the final visible pose seeds the transition and the arms are free for push-off; contact IK returns near the destination. An early Space input also retains the wrist orientation weight actually reached by the probe before releasing it into the jump.

A dedicated hang-to-hang mocap library is still missing. Root-motion Motion Warping, a general reach-ring selector and coat/secondary physics are not implemented. TwinBlast's long coat and mechanical arms are a visual compromise, not a Drake likeness. Passing regression tests does not certify artistic quality.

## Research used

[Naughty Dog's official climbing breakdown](https://www.naughtydog.com/blog/uncharted_4_climbing_legacy_of_thieves_collection_pc) describes extensive authored/mocap coverage for reach combinations, meaningful support from the feet, and coordinated full-body IK. The implementation lesson here is to supply a readable performance first and reserve contact corrections for supported phases.

The published GDC session descriptions cover [partial/additive layering and player control (2010)](https://www.gdcvault.com/play/1012451/Animation-and-Player-Control-in), [animation workflow/prototyping (2017)](https://www.gdcvault.com/play/1024309/Animation-Bootcamp-Uncharted-4-Naughty), and [physics layered over gameplay animation (2017)](https://gdcvault.com/play/1024087/Physics-Animation-in-Uncharted-4). The official article and session descriptions were reviewed; these links do not imply a full viewing of the talks or implementation of Naughty Dog's proprietary system.

## Character source and restoration

Download Epic's free [Game Animation Sample 5.8](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) through Fab / Epic Launcher. The installed sample includes `SKM_TwinBlast_ActionHero`; it has 204 bones. Epic's [sample documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine) identifies the animation library as motion captured and supports migrating its assets into other projects.

1. Restore the official template assets and compile the project, then run `Scripts/SetupMotionMatching.command`.
2. Keep the downloaded sample beside `LostExpedition`, close Unreal and run `Scripts/SetupMocap.command /absolute/path/to/GameAnimationSample.uproject`.
3. Migration preserves existing packages, includes dependencies, and selects four animations plus the source and visible character meshes. `setup_mocap_character.py` creates UEFN → Manny baked retargets and Manny → TwinBlast runtime retargeting, then generates the eight wall-support poses.
4. Restart Unreal and run `Scripts/SmokeTest.command` and `Scripts/WallClimbReview.command`.

TwinBlast's built-in guns, grenade and ultimate-weapon assembly are collapsed at their dedicated bones; the project's separate pistol/rifle component owns weapon rendering and attachment.

The visible mesh is `/Game/Characters/Paragon/Heroes/TwinBlast/Meshes/SKM_TwinBlast_ActionHero`; the runtime retargeter is `/Game/Explorer/RTG_Explorer_TwinBlast`. Selected clips are baked under `/Game/Animation/Mocap/`. [Migration manifest](mocap-migration.json) and [retarget report](mocap-setup.json) record the exact paths. Original samples, imported/generated Content and raw frames stay local. Public Git contains code, scripts, source links and small previews.

The optional fallback is [CC0 Diesel by THEUNSEENVULGA](https://theunseenvulga.itch.io/3d-charater-riggeddiesel). Restore it with `Scripts/SetupExplorer.command /absolute/path/to/Diesel.glb` and launch with `-LegacyExplorer`. Its Mixamo pelvis is the root, so its retargeter disables independent root remapping. TwinBlast has a separate root; both profiles resolve canonical gameplay landmarks.

## Validation

UE 5.8.3 Mac Development builds successfully; **127 checks pass, zero failures**. The TwinBlast trace covers full ground probing, immediate Space, vertical, horizontal and interrupted wall probes. The tested wall jump lowers the pelvis 3.871 cm before takeoff and has 13 unconstrained frames at 60 Hz. Hand/boot travel relative to the capsule is 16.395/32.352 cm. Maximum joint rotation is 18.900 degrees per frame; maximum state-boundary displacement is 2.185 cm. Across 26 fully released hand samples, contact correction changes wrist rotation by at most 0.001135 degrees. These figures apply only to the scripted cases.

See [runtime-test.txt](runtime-test.txt) and [climb-continuity-summary.json](climb-continuity-summary.json) for the latest build and runtime measurements. The suite measures 13 visible joints through both ground-probe input timings and vertical, horizontal and interrupted wall probes, plus preload, true unconstrained flight, arm/leg travel relative to the capsule and anatomical hand sides. It also checks all 23 holds, drop, blockers, descent, mantle, ground locomotion, weapons and items.

The [60 fps review](Images/wall-climb-60fps.mp4) contains 630 actual Unreal frames, including deliberate cuts to the horizontal route at frame 300 and rooftop pull-up at frame 480. Simulation advances only after each screenshot completes. Fixed-timestep capture does not measure hardware performance; raw frames remain local.
