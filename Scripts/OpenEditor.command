#!/bin/zsh
set -e
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE_DIR='/Users/Shared/Epic Games/UE_5.8'
open -n "$ENGINE_DIR/Engine/Binaries/Mac/UnrealEditor.app" --args "$PROJECT_DIR/LostExpedition.uproject" "-ExecCmds=py exec(open('$PROJECT_DIR/Scripts/editor_view.py').read())"
