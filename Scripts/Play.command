#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
"$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor" "$PROJECT_DIR/LostExpedition.uproject" /Game/Maps/CliffSanctuary -game -windowed -ResX=1440 -ResY=900 -nosplash
