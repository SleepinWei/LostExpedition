# Character action system upgrade plan

[English](ACTION_SYSTEM_PLAN.md) | [简体中文](ACTION_SYSTEM_PLAN.zh-CN.md)

Upgrade the island prototype so that moving, grabbing the tower, climbing several handholds, mantling, and aiming form a continuous sequence. Responsive input, stable hand and foot contacts, and consistent collision take priority over adding more actions.

## Baseline before the first upgrade and target

At the original baseline, the prototype plays official firearm clips but generates climbing with timed position interpolation and two-bone IK. A handhold transfer lasts 0.76 seconds, followed by a cooldown. Commands received during a transfer are not buffered. Traversal and locomotion use separate visible meshes, without a full-body exit blend. The 73 recorded checks establish functional behavior; visual quality and input continuity need separate validation.

The target combines an animation movement model, gameplay-owned traversal targets, authored animation where available, contact correction, and transitions that preserve the outgoing pose and velocity. Ground movement remains capsule driven. Traversal movement has one authority at a time so root motion and collision sweeps do not fight each other.

## Implementation sequence

| Phase | Work | Acceptance |
| --- | --- | --- |
| 1 Movement and commands | Tune acceleration, braking, rotation, and aiming transitions. Centralize traversal input, buffer brief commands, select the next hold before the current transfer finishes, and support continuous movement and deliberate reversal. | Holding a direction chains valid holds without the previous per-hold pause. Releasing input stops further chaining. Reversal, blocked targets, drop, and journal interruption behave predictably. |
| 2 Animation and contacts | Preserve poses across action transitions. Blend complete climbing poses into locomotion. Use support contacts, body reach limits, wall foot traces, and movement curves tied to the contact phases. | Support limbs remain stable during their planted phase. Changes of direction and the mantle exit do not snap the whole body. The capsule follows a collision-safe path. |
| 3 Animation assets and movement selection | Inspect Epic's Game Animation Sample and local template assets. Integrate compatible locomotion and traversal assets with an animation instance, Pose Search or Motion Matching where supported, and Motion Warping for authored root-motion traversal. Add weapon reload and action layers. | The active asset paths, selection method, required plugins, and restoration steps are documented. Features unavailable because of a concrete asset or access dependency remain explicitly identified. |
| 4 Validation and delivery | Exercise a complete approach, grab, continuous climb, traverse, mantle, descent, and moving combat sequence. Record actual frames, check motion at different frame rates, run gameplay regressions, and publish code and compact previews. | Build succeeds; traversal and combat checks pass; contact error, transition motion, and chaining are measured. Documentation states the actual implemented system and remaining limitations. |

## Validation cases

- Hold W over three vertical transfers; release while transferring; tap the next direction before the contact window; reverse to S; traverse both horizontal gaps.
- Keep the planted hand in world space until release. Measure solved hand positions as well as requested IK targets.
- Grab from the approach, mantle from the last hold, walk away from the roof, reverse onto the wall, and drop to restore normal movement.
- Move and turn while aiming, fire the pistol, hold automatic rifle fire, release aim during recovery, reload, and switch weapons.
- Compare transfer completion and contacts at 30, 60, and 120 simulated updates per second. Keep existing damage, inventory, checkpoint, and collision tests.
- Review real engine captures at normal speed and slowed playback. A passing numerical check does not replace visual review.

## Asset and repository policy

Use compatible Epic template or free sample content and preserve the existing local island assets. Large animation packages, generated Unreal assets, raw captures, caches, and saves stay local. Commit C++, configuration, reproducible setup scripts, source links, and small documentation previews. Sample content needs to be restored separately in a fresh clone.

## Technical references

- [Animation and Player Control in Uncharted 1 and 2, GDC 2010](https://www.gdcvault.com/play/1012451/Animation-and-Player-Control-in): player animation, partial and additive animation, and layering.
- [Creating a Character in Uncharted, GDC 2008 PDF](https://media.gdcvault.com/gdc08/slides/S6169i1.pdf): action states, layers, controllers, and animation relative to environmental objects; primarily NPC architecture.
- [Uncharted 4 climbing breakdown, Naughty Dog](https://www.naughtydog.com/blog/uncharted_4_climbing_legacy_of_thieves_collection_pc?sf171901127=1): analog reaching, climbing animation coverage, and coordinated full-body IK.
- [Motion Matching in The Last of Us Part II, GDC 2021](https://www.gdcvault.com/play/1027118/Motion-Matching-in-The-Last): Naughty Dog's later motion matching work.
- [Epic Game Animation Sample](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016) and [sample documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-animation-sample-project-in-unreal-engine): free movement animation content and a working Motion Matching example. Its gameplay traversal is an example rather than a complete adventure controller.
- [Motion Warping](https://dev.epicgames.com/documentation/en-us/unreal-engine/motion-warping-in-unreal-engine) and [inertial animation blending](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine): align authored root motion to targets and preserve transition continuity.

## First upgrade implemented

Starting point: `e3fa5e8`. The first upgrade implements phases 1–2, the local weapon-animation portion of phase 3, and runtime/capture validation from phase 4.

- Transfers use distance-based durations of 0.48–0.76 seconds. Lookahead selects and collision-checks the next hold. Adjacent transfers carry velocity and consume any remaining timestep at the contact boundary.
- Axis input is evaluated once per frame. Held input chains moves; a new brief direction is retained until the transfer ends plus a 0.18-second buffer. Release/reversal replans the remaining Hermite curve from its current position and analytic velocity. Space pressed during the final reach buffers the rooftop mantle. Journal and drop clear commands.
- A single visible pose mesh keeps a permanent weapon socket. The hidden template animation blueprint remains the locomotion source. Full-body transitions use critically damped position/rotation offsets, retain outgoing velocity and world-root placement, and run before contact IK.
- Wall traces determine foot positions. The tower wall has a contact-surface tag, including already generated maps. Reach correction shifts the body before solving the four limbs. Hand/foot release phases permit the body to advance while maintaining the current supports.
- Ground acceleration, braking, camera-facing rotation, and target speed are tuned. Armed movement blends adjacent directions among 32 official pistol/rifle walk and jog clips, with a distance-driven common cycle. This is directional animation selection, not Motion Matching.
- Official non-additive reload/equip clips are blended above the pelvis; official mesh-space additive recoil crossfades across shots. Gameplay ammunition timers remain independent of pose evaluation.

### Verification

UE 5.8.3 Mac Development build succeeded. The real-world runtime suite passes **94 checks**, including all 23 holds, both horizontal transfers, roof ascent/descent, commands, blocked reaches, recoil continuity, moving aim, reload/equip, damage, inventory, and saves. At 30/60/120 fixed updates per second, held input chains four transfers without an idle frame. Tested planted support hand and foot errors, and the 30/120 Hz body-path difference, round to **0.000 cm**. These are deterministic test measurements, not claims about rendering frame rate or every possible animation pose.

Real-engine captures include chained vertical/sideways traversal, rooftop ascent/descent, moving fire, and both reload actions. The raw 960×600 frames remain local; compact GIFs are included in the README. Visual review must still assess the authored quality of a final character/animation set.

## Motion Matching upgrade implemented

The next upgrade replaces active ground locomotion with Epic's compiled Motion Matching node. Three Pose Search databases use the existing official template skeleton and 51 locomotion sequences, totaling 2,910 indexed poses. Actual pose history, past movement and predicted velocity/facing supply the query. Jump, fall and landing have explicit-time blends; weapon changes interrupt the continuing result when its database is no longer allowed. Capsule movement and the current climbing/contact pipeline retain their existing authority.

Generation, runtime verification, capture workflow and precise coverage are recorded in [MOTION_MATCHING.md](MOTION_MATCHING.md). The first upgrade's 94-check result above is historical; the current results are in [runtime-test.txt](runtime-test.txt).

## Three-stage wall climbing and clothed hero implemented

The first character upgrade replaced the visible mannequin with the CC0 Diesel character, retargets the completed action pose through Unreal IK Retargeter, and adds seven full-body keyframe sequences. Climbing now explicitly probes a selected grip, waits for Space, leaps along a swept arc and recovers in a secure catch. This input model supersedes the first upgrade's automatic held-direction chaining. Setup, provenance and coverage are recorded in [WALL_CLIMB.md](WALL_CLIMB.md); the current regression results are in [runtime-test.txt](runtime-test.txt).

## Remaining coverage and next work

Game Animation Sample is optional expanded content, not a prerequisite for the installed Motion Matching system. It has not been downloaded or migrated. Dedicated start/stop and pivot coverage, imported climbing motion capture and varied landing clips remain future work.

1. Expand compatible authored movement/traversal coverage from the free sample or other licensed clips, preserving skeleton and dependency restoration.
2. Tune selection and transitions against those clips; use Motion Warping only for traversal sequences with compatible authored root motion.
3. Re-run the gameplay/contact suite and compare starts, stops, sharp turns, landing and the full tower route in real playback.

Rope traversal, physical secondary animation, sound and cinematic polish remain later work. This is a functioning prototype with Motion Matching, not a claim of Uncharted's production animation quality.


The earlier continuity rebuild replaced runtime stage-clip swapping with a shared base pose, persistent pre-contact pose springs on both rigs, fixed support feet and continuous probe/transfer/catch curves. Interrupted probes now start from the displayed hands. See [WALL_CLIMB.md](WALL_CLIMB.md) and [climb-continuity-summary.json](climb-continuity-summary.json) for measured results and the full-rate review.

## Full-body leap correction

The shared static hanging pose did not provide a readable jump. Runtime now samples eight full-body actions with preload, extension, free flight and catch. Contact IK runs once after retargeting and releases during flight. Baking fixes the mannequin mesh-space left/right and pitch axes. Final validation also measures actual limb motion relative to the capsule, not only route success and positional continuity. See [WALL_CLIMB.md](WALL_CLIMB.md) for current implementation, limits and research references.
