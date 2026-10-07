#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
mkdir -p "$PROJECT_DIR/ArtSource/Characters"
if [[ -n "${1:-}" ]]; then
    cp "$1" "$PROJECT_DIR/ArtSource/Characters/Diesel.glb"
fi
if [[ ! -f "$PROJECT_DIR/ArtSource/Characters/Diesel.glb" ]]; then
    print 'Download the free Diesel.glb from https://theunseenvulga.itch.io/3d-charater-riggeddiesel'
    print 'Then run Scripts/SetupExplorer.command /absolute/path/to/Diesel.glb'
    exit 1
fi
python3 "$PROJECT_DIR/Scripts/prepare_explorer_character.py"
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_DIR/LostExpedition.uproject" -run=pythonscript -script="$PROJECT_DIR/Scripts/import_explorer_character.py" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/explorer-import.log" 2>&1
rg -q 'EXPLORER_SETUP_COMPLETE' "$PROJECT_DIR/Docs/explorer-character-setup.txt"
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_DIR/LostExpedition.uproject" -run=pythonscript -script="$PROJECT_DIR/Scripts/setup_wall_climb.py" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/wall-climb-setup.log" 2>&1
rg -q 'WALL_CLIMB_SETUP_COMPLETE' "$PROJECT_DIR/Docs/wall-climb-setup.txt"
cat "$PROJECT_DIR/Docs/explorer-character-setup.txt" "$PROJECT_DIR/Docs/wall-climb-setup.txt"
