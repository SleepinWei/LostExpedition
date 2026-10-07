# Three-stage wall climbing

The clothed **Diesel** character now presents the complete action pose. Manny remains a hidden animation source for ground Motion Matching, weapon clips and authored wall sequences. Unreal's runtime IK Retargeter converts that finished pose to Diesel's Mixamo rig; final contact correction aligns hands/feet and the weapon with the different body proportions.

| Stage | Input and behavior | Animation |
| --- | --- | --- |
| Exploratory reach | Approach the first grip or press E. On the wall, WASD selects an adjacent grip. The capsule stays at its current support. | Ground reach or mirrored left/right probe; torso/neck turn, leading arm extension and partially open fingers |
| Jump to grab | Space commits the selected grip. Early presses retain at least 0.18 s of anticipation. | Mirrored full-body leap, push-off, tucked legs, reaching arms and closing fingers; capsule follows a swept arc |
| Secure catch | Contact automatically enters a 0.32 s recovery before another leap. | Two-hand catch, torso compression, knee bracing and recovery to hanging idle |

Holding direction selects another grip after a catch and waits for Space. A single Space press during flight can queue one next leap. At the final hold, Space tops out. Ctrl drops from the wall or cancels a standing probe; opening the journal cancels an uncommitted probe. Newly obstructed flight paths restore falling rather than passing through geometry. Roof entry continues to route the capsule over the parapet before lowering it onto the highest hold.

Seven project-authored `AnimSequence` assets under `/Game/Animation/WallClimb/` provide the full-body base: `AS_Probe_Ground`, `AS_Probe_Left`, `AS_Probe_Right`, `AS_Jump_Left`, `AS_Jump_Right`, `AS_Catch`, `AS_Hang`. The reproducible keyframe authoring function in `ExplorerAnimation.cpp` generates every source-skeleton track at 30 Hz. They are keyframed prototype actions, not imported motion capture. World-space grip/foot IK adapts these actions to the tower's geometry. Climbing uses explicit sequence phases; ground Motion Matching remains active. Root-motion Motion Warping is not enabled.

## Character source and restoration

[Diesel by THEUNSEENVULGA](https://theunseenvulga.itch.io/3d-charater-riggeddiesel) is a hand-painted rigged male character published under CC0. Download **Diesel.glb** through “Download Now” → “No thanks, just take me to the downloads”. No paid pack or Fab login is required. The original download and imported meshes/textures remain local.

The tested source is 10,515,216 bytes, SHA-256 `9fbb438e8221f96e1f25da90c731f474bf031970a75ba132c87e1aeaf784745e`. `prepare_explorer_character.py` merges body, clothes, boots, hair and facial meshes while preserving embedded textures and skin weights; it selects the middle eyebrow variant. `import_explorer_character.py` imports one skeletal mesh, characterizes both rigs, maps the chains and aligns the retarget pose. Root-motion remapping is disabled because the Mixamo rig uses its pelvis as the skeleton root; remapping that bone to the source ground root would collapse pelvis height and stretch the skin. The pelvis operation preserves animation offsets while gameplay controls displacement.

1. Restore official template assets and build the C++ editor module as described in [ASSETS.md](ASSETS.md).
2. Close Unreal and run `Scripts/SetupExplorer.command /absolute/path/to/Diesel.glb`. Without an argument it uses `ArtSource/Characters/Diesel.glb` from a previous download.
3. Restart Unreal. The visible mesh is `/Game/Explorer/Character/Explorer/SkeletalMeshes/SK_Diesel`; the runtime retargeter is `/Game/Explorer/RTG_Explorer_Diesel`.
4. Run `Scripts/SmokeTest.command` and `Scripts/WallClimbReview.command` to check gameplay and capture a real-engine review.

Generated assets, original downloads and raw capture frames are excluded from Git. The public repository keeps code, scripts, source links, small previews and reports.

## Validation

UE 5.8.3 Mac Development build succeeds; **112 runtime checks pass with zero failures**. The full tower route, horizontal transfers, descent, mantle and gameplay regressions pass. Fixed tests measure maximum secure-contact hand error at **0.000 cm**, foot error at **4.122 cm** (within the 8 cm acceptance limit), and 30/120 Hz capsule-path difference at **0.000 cm**; this applies to the tested poses, not every possible configuration or rendered frame rate. The setup script has been exercised from the downloaded GLB through both rig creation and all seven saved sequences. See [runtime-test.txt](runtime-test.txt), [explorer-character-setup.txt](explorer-character-setup.txt) and [wall-climb-setup.txt](wall-climb-setup.txt).
