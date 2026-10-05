#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT_DIR/LostExpedition.uproject" /Game/Maps/CliffSanctuary -game -AdventureSmokeTest -nullrhi -unattended -nosound -nosplash -stdout -FullStdOutLogOutput > "$PROJECT_DIR/Docs/runtime.log" 2>&1
cat "$PROJECT_DIR/Docs/runtime-test.txt"
