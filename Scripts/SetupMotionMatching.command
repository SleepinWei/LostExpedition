#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_DIR/LostExpedition.uproject" -run=pythonscript -script="$PROJECT_DIR/Scripts/setup_motion_matching.py" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/mm-setup.log" 2>&1
rg -q 'MM_SETUP_COMPLETE' "$PROJECT_DIR/Docs/mm-setup.log"
rg -q 'MM_SETUP_COMPLETE' "$PROJECT_DIR/Docs/motion-matching-setup.txt"
cat "$PROJECT_DIR/Docs/motion-matching-setup.txt"
