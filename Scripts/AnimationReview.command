#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PROJECT_DIR/LostExpedition.uproject" /Game/Maps/CliffSanctuary -game -AnimationVisualReview -windowed -ResX=960 -ResY=600 -unattended -nosound -nosplash -stdout -FullStdOutLogOutput "$@"
