"""Run inside Epic's downloaded Game Animation Sample 5.8 to migrate selected assets.

Large licensed assets stay in local Content, which is excluded from Git.
The destination defaults to the sibling LostExpedition project's Content directory.
Existing destination packages are preserved by AssetTools' Skip conflict policy.
"""
from pathlib import Path
import json
import unreal

ANIM_ROOT = '/Game/Characters/UEFN_Mannequin/Animations/'
CLIPS = [
    'Jump/M_Neutral_Jump_F_Start_Stand_Lfoot',
    'Traversal/Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot',
    'Traversal/Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Rfoot',
    'Traversal/Mantle/M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot',
]
PACKAGES = [ANIM_ROOT + clip for clip in CLIPS] + [
    '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin',
    '/Game/Characters/Paragon/Heroes/TwinBlast/Meshes/SKM_TwinBlast_ActionHero',
]

def main():
    source = Path(unreal.Paths.project_dir()).resolve()
    target = source.parent / 'LostExpedition'
    assert (target / 'LostExpedition.uproject').is_file(), str(target)
    for package in PACKAGES:
        assert unreal.EditorAssetLibrary.does_asset_exist(package), package
    options = unreal.MigrationOptions()
    options.set_editor_property('prompt', False)
    options.set_editor_property('ignore_dependencies', False)
    options.set_editor_property('asset_conflict', unreal.AssetMigrationConflict.SKIP)
    unreal.AssetToolsHelpers.get_asset_tools().migrate_packages(PACKAGES, str(target / 'Content'), options)
    missing = [p for p in PACKAGES if not (target / 'Content' / (p.removeprefix('/Game/') + '.uasset')).is_file()]
    assert not missing, repr(missing)
    report = {'sample': 'Game Animation Sample 5.8',
              'url': 'https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016',
              'selected_packages': PACKAGES, 'dependencies': 'Migrated by Unreal AssetTools',
              'conflicts': 'Preserve existing packages (Skip)'}
    (target / 'Docs/mocap-migration.json').write_text(json.dumps(report, indent=2) + '\n')
    unreal.log('MOCAP_MIGRATION_COMPLETE')

if __name__ == '__main__':
    main()
