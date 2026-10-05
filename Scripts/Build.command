#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
"$ENGINE_DIR/Engine/Build/BatchFiles/Mac/Build.sh" LostExpeditionEditor Mac Development "$PROJECT_DIR/LostExpedition.uproject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=2
