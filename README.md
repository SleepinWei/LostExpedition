# Lost Expedition

[English](README.md) | [简体中文](README.zh-CN.md)

A third-person adventure prototype built with native C++ in **Unreal Engine 5.8.3**. Explore a tropical island from its sandy shoreline, through the jungle, to a ruined tower on a central highland. Climb the tower using stone handholds, collect relics, and return to the beach.

![Island overview: sandy shoreline, tropical forest, central highland, and ruined tower](Docs/Images/island-overview.jpg)

## Screenshots

Actual Unreal Engine captures from the current island level.

**Beach and jungle approach**

![Coconut palms and jungle vegetation framing the approach from the beach](Docs/Images/island-beach.jpg)

**Ruined tower on the highland**

![Ruined stone tower with broken walls, window openings, and an exterior climbing route](Docs/Images/island-tower.jpg)

**Playable wall climbing**

![In-game character gripping a stone handhold on the tower wall](Docs/Images/tower-gameplay.jpg)

## Character animations

The [action system upgrade plan](Docs/ACTION_SYSTEM_PLAN.md) records the implementation stages and remaining animation dependencies.

Climbing includes a grab transition, alternating hand and foot reaches, weight shifts during vertical and sideways movement, a crouched rooftop pull-up, and a reverse transition for descending from the roof. Holding a direction chains adjacent holds without a per-hold pause. Brief commands and reversals are buffered until the next contact; releasing finishes the current reach. Wall traces select reachable foot contacts, and body correction keeps supporting limbs within reach.

![Animated climbing: upward reach, sideways traverse, rooftop mantle, and descent](Docs/Images/climbing-animation.gif)

Pistol and rifle fire use the official UE mannequin animation sequences, blended into the upper body over the existing locomotion pose. The weapon follows the animated hand; successive rifle shots retrigger recoil, and recovery blends back into movement. Aim elevation follows the camera. Official reload and equip clips share the upper-body layer; eight-direction walk/jog clips follow the actual movement direction during armed movement. The gait phase follows distance travelled. Consecutive shots crossfade recoil instead of resetting the arm pose.

Full-body action transitions retain the outgoing pose and velocity with critically damped offsets evaluated before contact IK. A single visible mesh and permanent weapon socket carry movement, traversal, and combat. Ground acceleration, braking, speed changes, and camera-facing rotation are smoothed.

![Moving fire, reload, and official weapon actions in the game](Docs/Images/firing-animation.gif)

## Getting started

This repository contains source code, configuration, generation scripts, test reports, and compressed README screenshots and animation previews. Game models, textures, maps, full-resolution captures, build output, and saves stay local. **A fresh clone requires asset restoration and map generation before it can be played.** Follow the [asset sources and restoration guide](Docs/ASSETS.md) for download links and exact import paths.

The macOS scripts default to `/Users/Shared/Epic Games/UE_5.8`. Update that path for your installation. Other platforms require the corresponding Unreal C++ toolchain and build commands.

| Script | Purpose |
| --- | --- |
| [`Scripts/OpenEditor.command`](Scripts/OpenEditor.command) | Open the editor at the island overview |
| [`Scripts/Play.command`](Scripts/Play.command) | Start the game from the beach, or resume a saved checkpoint |
| [`Scripts/PlayTower.command`](Scripts/PlayTower.command) | Start beside the tower to try wall climbing; keeps existing saves |
| [`Scripts/Build.command`](Scripts/Build.command) | Build the editor module |
| [`Scripts/SmokeTest.command`](Scripts/SmokeTest.command) | Run checks in the actual game world |
| [`Scripts/AnimationReview.command`](Scripts/AnimationReview.command) | Capture repeatable climbing and firing animation frames |

The map's internal asset path remains `/Game/Maps/CliffSanctuary`.

## Island and route

1. Start on the southwest beach, pass through coconut palms, and follow a continuous forest trail onto the central highland.
2. The highland rises approximately **22 m above sea level**. The tower's rooftop is another **28 m above the highland**, with window openings, broken upper walls, fallen masonry, roof beams, and wall plants.
3. Follow **23 stone handholds** on the west wall. The route includes two horizontal transfers and a final mantle onto the rooftop.
4. Checkpoints are located on the beach, in the forest, at the tower base, and on the rooftop. Approach a blue beacon and press **E** to save and restore health.
5. Collect one relic each from the beach, forest, and rooftop. Find the highland key, unlock the tower's east doorway, and return to the beach exit with all three relics.

## Controls

| Action | Input |
| --- | --- |
| Move / look | WASD / mouse |
| Sprint / jump | Left Shift / Space |
| Grab a handhold / interact | E |
| Climb up / down | W / S |
| Traverse left / right | A / D |
| Mantle from the final handhold | Space |
| Descend from the rooftop | Approach the west opening, face outward, and press E |
| Let go | Left Ctrl |
| Aim / fire | Right / left mouse button |
| Pistol / rifle / reload | 1 / 2 / R |
| Medkit / grenade | Q / G |
| Adventure journal / reset this island's save | Tab / F5 |

Climbing includes facing checks, adjacent handhold selection, collision sweeps, horizontal transfers, descent, rooftop mantling, and grabbing the edge from above. Hands and feet move in separate phases, with a supporting hand retained during the first reach. Intermediate holds cannot be mantled; horizontal gaps require **A/D**.

## Combat, items, and saves

The prototype includes a pistol, rifle, reloading, over-the-shoulder aiming, hit damage, grenades with blast occlusion, medkits, keys, relics, and checkpoint saves.

The island uses the separate `LostExpedition_Island_Checkpoint` save slot to avoid restoring positions from the previous map. Older saves are preserved.

## Generation and validation

After restoring the required assets, run this in your system terminal to generate the original coconut palm mesh and download the sand textures:

```sh
python3 Scripts/prepare_island_assets.py
```

Then execute `Scripts/setup_scene.py` through Unreal's **Tools → Execute Python Script** menu. This regenerates the map and overwrites manual changes to that generated level; save your own edits elsewhere first.

| Source | Responsibility |
| --- | --- |
| [`IslandTerrain.h`](Source/LostExpedition/IslandTerrain.h) | Island outline, highland, and trail height functions |
| [`ExpeditionWorld.cpp`](Source/LostExpedition/ExpeditionWorld.cpp) | Continuous terrain, ocean, vegetation instances, and ruined tower |
| [`ExpeditionTower.h`](Source/LostExpedition/ExpeditionTower.h) | Handhold coordinates and tower dimensions |
| [`ExplorerCharacter.cpp`](Source/LostExpedition/ExplorerCharacter.cpp) | Traversal, combat, and saves |
| [`ExplorerAnimation.cpp`](Source/LostExpedition/ExplorerAnimation.cpp) | Official firearm animation blending, staged climbing limbs, and pull-up poses |
| [`ExplorerPoseComponent.cpp`](Source/LostExpedition/ExplorerPoseComponent.cpp) | Update the visible action pose after locomotion bones are evaluated |
| [`create_island_materials.py`](Scripts/create_island_materials.py) | Blended sand and rock, shallow water, shoreline foam, and vegetation materials |

The recorded runtime results are in the [test report](Docs/runtime-test.txt). Checks cover the continuous beach-to-tower route, every handhold transfer, supporting-hand and wall-foot contact error, 30/60/120 Hz chaining, buffered taps and reversals, release continuity, blocked reaches, animated rooftop ascent and descent, firearm animation triggers and recoil continuity, moving aim, reload/equip actions, weapon attachment, damage, items, and saves. These results apply to the configured local project; run the checks again after restoring assets in a fresh clone.

For repeatable screenshots, run `Scripts/editor_view.py` at editor startup with `-AdventureCapture -AdventureCaptureExit`. It writes the full-resolution island views to `Docs/`. Launching the game with `-WatchtowerVisualReview` captures a real wall-gripping state to `Docs/tower-gameplay.png` and exits. Only the compressed copies in `Docs/Images/` are included in Git.

`Scripts/AnimationReview.command` captures fixed-timestep animation frames into `Docs/AnimationFrames/`. Run `python3 Scripts/assemble_animation_previews.py` with Pillow installed to assemble the small GIF previews. The source frames remain local.

## Current scope

This is a single-player adventure prototype using the UE mannequin and procedural climbing animation. Climbing still uses generated limb poses rather than authored motion capture. Motion Matching/Pose Search and root-motion Motion Warping are not active: Epic Game Animation Sample has not been downloaded, and its Fab acquisition requires account login. The current ground system uses the template animation blueprint plus directional weapon clips. Rope swinging, audio, and cinematic sequences are not implemented. External asset sources and restoration instructions are listed in [Docs/ASSETS.md](Docs/ASSETS.md).
