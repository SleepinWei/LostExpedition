#!/bin/zsh
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
SAMPLE_PROJECT="${1:-$(dirname "$PROJECT_DIR")/GameAnimationSample/GameAnimationSample.uproject}"
[[ -f "$SAMPLE_PROJECT" ]] || { print -u2 'Download Game Animation Sample 5.8 through Fab / Epic Launcher first.'; exit 1; }
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$SAMPLE_PROJECT" -run=pythonscript "-script=$PROJECT_DIR/Scripts/migrate_game_animation_sample.py" -nullrhi -nosound -unattended -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/mocap-migration.log" 2>&1
rg -q 'MOCAP_MIGRATION_COMPLETE' "$PROJECT_DIR/Docs/mocap-migration.log"
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_DIR/LostExpedition.uproject" -run=pythonscript "-script=$PROJECT_DIR/Scripts/setup_mocap_character.py" -nullrhi -nosound -unattended -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/mocap-setup.log" 2>&1
rg -q 'MOCAP_SETUP_COMPLETE' "$PROJECT_DIR/Docs/mocap-setup.log"
print 'Mocap and TwinBlast ready. Restart the game to load the new assets.'
