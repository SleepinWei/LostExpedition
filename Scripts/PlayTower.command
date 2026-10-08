#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
open -n "$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor.app" --args "$PROJECT_DIR/LostExpedition.uproject" /Game/Maps/CliffSanctuary -game -nosound -WatchtowerStart -windowed -ResX=1600 -ResY=1000
