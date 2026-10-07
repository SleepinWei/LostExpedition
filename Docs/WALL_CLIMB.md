# Three-stage wall climbing

The visible **Diesel** character uses ground Motion Matching and weapon animation through Unreal's runtime IK Retargeter. Climbing now samples complete action sequences before retargeting, with **one final wall-contact solve on the visible rig**. The hidden source skeleton no longer solves the same contacts first.

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
- Wrist orientation follows the release/arrival curve. A free palm faces the wall and closes onto the stone at catch. The runtime arm solve uses a fixed extension reserve. Re-applying a nonlinear soft-reach correction to an already-corrected, captured wrist point caused an elbow step on direction changes and is removed.
- During an interrupted probe, the displayed hand positions are captured and root support remains on the original ledge. Planted boots are world-space contacts. Pose springs act before final contact correction rather than repeatedly smoothing an already-constrained pose.

These are original program-authored keyframe animations, **not imported climbing motion capture**. Ground Motion Matching remains active. Root-motion Motion Warping, a general surface reach-ring selector and physics secondary motion are not implemented. The current stylized Diesel rig has short arms; a realistic character and a broader authored/mocap library remain relevant to matching the target art quality. Passing the regression suite does not certify animation quality.

## Research used

[Naughty Dog's official climbing breakdown](https://www.naughtydog.com/blog/uncharted_4_climbing_legacy_of_thieves_collection_pc) describes extensive authored/mocap coverage for reach combinations, meaningful support from the feet, and coordinated full-body IK. The implementation lesson here is to supply a readable performance first and reserve contact corrections for supported phases.

The published GDC session descriptions cover [partial/additive layering and player control (2010)](https://www.gdcvault.com/play/1012451/Animation-and-Player-Control-in), [animation workflow/prototyping (2017)](https://www.gdcvault.com/play/1024309/Animation-Bootcamp-Uncharted-4-Naughty), and [physics layered over gameplay animation (2017)](https://gdcvault.com/play/1024087/Physics-Animation-in-Uncharted-4). The official article and session descriptions were reviewed; these links do not imply a full viewing of the talks or implementation of Naughty Dog's proprietary system.

## Character source and restoration

[Diesel by THEUNSEENVULGA](https://theunseenvulga.itch.io/3d-charater-riggeddiesel) is a hand-painted rigged male character published under CC0. Download **Diesel.glb** through “Download Now” → “No thanks, just take me to the downloads”. No paid pack or Fab login is required. The original download and imported meshes/textures remain local.

The tested source is 10,515,216 bytes, SHA-256 `9fbb438e8221f96e1f25da90c731f474bf031970a75ba132c87e1aeaf784745e`. `prepare_explorer_character.py` merges body, clothes, boots, hair and facial meshes while preserving embedded textures and skin weights; it selects the middle eyebrow variant. `import_explorer_character.py` imports one skeletal mesh, characterizes both rigs, maps the chains and aligns the retarget pose. Root-motion remapping is disabled because the Mixamo rig uses its pelvis as the skeleton root; remapping that bone to the source ground root would collapse pelvis height and stretch the skin. The pelvis operation preserves animation offsets while gameplay controls displacement.

1. Restore official template assets and build the C++ editor module as described in [ASSETS.md](ASSETS.md).
2. Close Unreal and run `Scripts/SetupExplorer.command /absolute/path/to/Diesel.glb`. Without an argument it uses `ArtSource/Characters/Diesel.glb` from a previous download.
3. Restart Unreal. The visible mesh is `/Game/Explorer/Character/Explorer/SkeletalMeshes/SK_Diesel`; the runtime retargeter is `/Game/Explorer/RTG_Explorer_Diesel`.
4. Run `Scripts/SmokeTest.command` and `Scripts/WallClimbReview.command` to check gameplay and capture a real-engine review.

Generated assets, original downloads and raw capture frames are excluded from Git. The public repository keeps code, scripts, source links, small previews and reports.

## Validation

UE 5.8.3 Mac Development builds successfully; **121 checks pass, zero failures**. The tested jump lowers the pelvis 4.957 cm before takeoff and has 14 unconstrained frames at 60 Hz. During that interval the hand and boot move 24.447/26.504 cm relative to the capsule. Maximum sampled joint rotation is 14.680 degrees per frame; interrupted-probe displacement is 0.3325 cm. These figures apply to the scripted cases only.

See [runtime-test.txt](runtime-test.txt) and [climb-continuity-summary.json](climb-continuity-summary.json) for the latest build and runtime measurements. The suite measures 13 visible joints through vertical, horizontal and interrupted probes, plus preload, true unconstrained flight, arm/leg travel relative to the capsule and anatomical hand sides. It also checks all 23 holds, drop, blockers, descent, mantle, ground locomotion, weapons and items.

The [60 fps review](Images/wall-climb-60fps.mp4) contains 480 actual Unreal frames, with a deliberate cut to the high horizontal route at frame 300. Simulation advances only after each screenshot completes. Fixed-timestep capture does not measure hardware performance; raw frames remain local.
