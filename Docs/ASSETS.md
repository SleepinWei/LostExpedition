# 素材下载与本地恢复

公开仓库发布源码、配置、说明、测试报告、少量自制 OBJ 几何源文件，以及 `Docs/Images/` 下的 README 压缩预览图和动图。整个 `Content/`、下载的 FBX/贴图、原始全分辨率截图、编译缓存和存档均不上传，公开提交历史也不包含这些大文件。已有本机工程里的文件继续保留。

## 海岸扫描素材

新海岛额外使用 [coast_sand_01](https://polyhaven.com/a/coast_sand_01) 的 2K Diffuse、Normal DX 与 Roughness。运行 `python3 Scripts/prepare_island_assets.py` 下载并校验，目标为 `/Game/Island/T_coast_sand_01_*_2k`。同一脚本生成原创椰子树 OBJ，导入目标为 `/Game/Island/SM_CoconutPalm`。生成文件位于被 Git 忽略的 `ArtSource/Island/` 和 `Content/Island/`。

以下资源的来源页提供下载。海岸组由 `Scripts/download_coastal_assets.py` 下载 2K 文件并校验官方 API 提供的 MD5，再由 `Scripts/import_coastal_assets.py` 在 Unreal 内导入。

| 素材链接 | 用途 | Unreal 目标 |
| --- | --- | --- |
| [coastal_cliff_02](https://polyhaven.com/a/coastal_cliff_02) | 海岸岩壁 | `/Game/Coastal/Meshes/SM_coastal_cliff_02` |
| [rock_07](https://polyhaven.com/a/rock_07) | 岩块与地基 | `/Game/Coastal/Meshes/SM_rock_07` |
| [fern_02](https://polyhaven.com/a/fern_02) | 蕨类 | `/Game/Coastal/Meshes/SM_fern_02` |
| [wooden_crate_02](https://polyhaven.com/a/wooden_crate_02) | 环境木箱 | `/Game/Coastal/Meshes/SM_wooden_crate_02` |
| [old_stone_wall_02](https://polyhaven.com/a/old_stone_wall_02) | 塔身与旧石墙 | `/Game/Coastal/Textures/T_old_stone_wall_02_*_2k` |
| [rock_face](https://polyhaven.com/a/rock_face) | 岩体与浅色石檐 | `/Game/Coastal/Textures/T_rock_face_*_2k` |
| [aerial_grass_rock](https://polyhaven.com/a/aerial_grass_rock) | 地表 | `/Game/Coastal/Textures/T_aerial_grass_rock_*_2k` |
| [worn_mossy_plasterwall](https://polyhaven.com/a/worn_mossy_plasterwall) | 风化抹灰 | `/Game/Coastal/Textures/T_worn_mossy_plasterwall_*_2k` |

Poly Haven 资源采用 [CC0 许可](https://polyhaven.com/license)。下载脚本还保留了 [wooden_barrels_01](https://polyhaven.com/a/wooden_barrels_01) 的下载记录；当前场景没有使用该模型。

## 植被和路面素材

这些资源来自原有本地素材库，**海岸下载/导入脚本不包含本组**。新克隆需要手动下载并导入，或从已经配置好的本地 Unreal 工程迁移对应资源及其依赖。

| 素材链接 | 下载建议 | 必需目标路径 |
| --- | --- | --- |
| [island_tree_01](https://polyhaven.com/a/island_tree_01) | FBX 与 1K 贴图 | `/Game/Nature/SM_TownTree` |
| [grass_medium_01](https://polyhaven.com/a/grass_medium_01) | FBX 与 1K 贴图 | `/Game/Nature/SM_TownGrass` |
| [cobblestone_floor_01](https://polyhaven.com/a/cobblestone_floor_01) | 1K Diffuse、Normal DX、Rough | `/Game/Textures/PolyHaven/T_cobblestone_floor_01_{diff,nor_dx,rough}_1k` |
| [wood_planks](https://polyhaven.com/a/wood_planks) | 1K Diffuse、Normal DX、Rough | `/Game/Textures/PolyHaven/T_wood_planks_{diff,nor_dx,rough}_1k` |

FBX 作为静态网格导入，启用 Combine Meshes，并重命名为表中的名称。树的树干、枝条、叶片分别指定对应材质与贴图；叶片使用 Masked、Two Sided，并将 Alpha 接到 Opacity Mask。草丛材质同样需要双面显示，并使用下载的透明度信息。法线贴图使用 Normalmap 压缩，法线与粗糙度关闭 sRGB；Diffuse 保持 sRGB。原始文件与多材质贴图名称可查 `Docs/AssetSources/PolyHaven-manifest.json`。

旧版材质记录还引用 [rocky_terrain](https://polyhaven.com/a/rocky_terrain)、[grass_ground](https://polyhaven.com/a/grass_ground)、[forest_ground_04](https://polyhaven.com/a/forest_ground_04)、[clay_roof_tiles](https://polyhaven.com/a/clay_roof_tiles)、[plastered_wall_02](https://polyhaven.com/a/plastered_wall_02)、[pine_bark](https://polyhaven.com/a/pine_bark) 和 [pine_sapling_small](https://polyhaven.com/a/pine_sapling_small)。这些不是当前高塔/海岸 V2 材质生成的必需项。

## Unreal 官方资源

新海岛树木材质还会读取 `/Game/Textures/PolyHaven/` 下的 `T_island_tree_01_{diff,nor_dx,rough}_1k`、`T_island_tree_01_branches_{diff,nor_dx,rough}_1k`、`T_island_tree_01_leaves_{diff,nor_dx,rough,alpha}_1k`。手动导入时请保留准确名称；树网格的材质槽顺序应为树干、叶片、枝条。

安装 [Unreal Engine](https://www.unrealengine.com/download) 5.8，并勾选引擎的模板资源。使用自己的引擎安装目录运行：

```sh
python3 Scripts/restore_engine_assets.py --engine "/Users/Shared/Epic Games/UE_5.8"
python3 Scripts/restore_engine_assets.py --engine "/Users/Shared/Epic Games/UE_5.8" --check
```

| 引擎内相对路径 | 本项目目标 |
| --- | --- |
| `Templates/TemplateResources/High/Characters/Content` | `Content/Characters` |
| `Templates/TemplateResources/Standard/Weapons/Content` | `Content/Weapons` |
| `Templates/TemplateResources/Standard/ArchVis/Content/SampleScene/Tree` | `Content/ArchVis/SampleScene/Tree` |

脚本会检查源目录是否存在。UE 资源按其自身条款使用，不包含在公开仓库中。

角色开火动画复用上述 `High/Characters/Content` 中的官方序列：

| Unreal 路径 | 用途 |
| --- | --- |
| `/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS` | 手枪瞄准基础姿态 |
| `/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire` | 手枪后坐力，Mesh Space Additive |
| `/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS` | 步枪瞄准基础姿态 |
| `/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire` | 步枪后坐力，Mesh Space Additive |

`restore_engine_assets.py` 已包含这些序列及其骨骼依赖。攀爬动画由 `ExplorerAnimation.cpp` 在运行时生成，不需要额外下载攀爬动作文件。压缩 GIF 是本工程实机动作捕获，源帧不上传。

## 恢复步骤

1. 安装 UE 5.8 与对应 C++ 工具链，恢复上面的 Unreal 官方模板资源。
2. 编译 `LostExpeditionEditor`；Mac 可运行 `Scripts/Build.command`。首次打开可能提示默认地图尚不存在，完成下列生成步骤后即可使用。
3. 下载并导入“植被和路面素材”中的四项必需资源，确保目标路径与名称完全一致。
4. 在系统终端运行 `python3 Scripts/download_coastal_assets.py`，下载海岸模型和贴图。
5. 在 Unreal 的 **Tools → Execute Python Script** 中执行 `Scripts/import_coastal_assets.py`。等待导入、材质和 LOD 处理完成。
6. 在系统终端运行 `python3 Scripts/prepare_island_assets.py`，生成椰子树并下载沙滩材质。
7. 在 Unreal 的同一菜单执行 `Scripts/setup_scene.py`。此脚本会生成海岛材质和 `Content/Maps/CliffSanctuary.umap`，也会覆盖该生成地图上的手动修改。运行前请保存自己的修改。
8. 打开地图并 Play；Mac 可用 `Scripts/PlayTower.command` 直达塔下。运行 `Scripts/SmokeTest.command` 检查素材引用、连续攀爬及其他玩法。

`ArtSource/CoastalRemake/weathered_block.obj`、`cliff_core.obj` 与 `.mtl` 是小体积的自制几何源文件，保留在 Git 中。当前岛体直接由 C++ 程序网格生成。最新运行结果见 `Docs/runtime-test.txt`；新克隆完成素材恢复后，应重新运行检查确认导入结果。
