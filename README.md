# Lost Expedition / 失落远征

UE 5.8.3 原生 C++ 第三人称冒险原型。参考《神秘海域4》海岛悬崖遗迹的空间和玩法氛围，采用原创关卡布局、自制程序化关卡、Unreal 官方免费模板素材和CC0 扫描环境资源。

## 从公开仓库克隆

这是**源码仓库**，包含 C++、项目配置、场景生成/素材导入脚本、测试报告和少量自制 OBJ 几何源文件。模型、贴图、Unreal 二进制资源、地图和截图不上传 Git；素材下载链接、目标路径和恢复步骤见 [素材与恢复说明](Docs/ASSETS.md)。

新克隆需要先补齐素材并生成地图，才能运行游戏。已有本地项目中的素材继续保留。UE 官方模板资源从自己的 UE 5.8 安装目录补齐：

```sh
python3 Scripts/restore_engine_assets.py --engine "/Users/Shared/Epic Games/UE_5.8"
```

随后运行 `Scripts/Build.command` 编译，再按 [恢复步骤](Docs/ASSETS.md#恢复步骤) 下载、导入环境素材并生成地图。其他系统可在 `--engine` 指定 UE 5.8 安装目录，并通过 Unreal 的项目文件生成器及对应 C++ 工具链构建。Mac 启动脚本默认使用上述引擎路径。

仓库也不包含编译产物、缓存和个人存档。机器可读来源清单见 `Docs/AssetSources/used-assets.json`；UE 模板资源的使用遵循其自身条款。

## 启动

- 双击 `Scripts/OpenEditor.command`，或用 UE 5.8 打开 `LostExpedition.uproject`；打开地图 `Content/Maps/CliffSanctuary.umap`，点击 **Play**。
- 双击 `Scripts/Play.command` 可以直接进入窗口游戏。
- 首次打开可能需要等待着色器编译。窗口内点击一次以捕获鼠标；编辑器中 Shift+F1 释放鼠标，Esc 停止 Play。

## 海岬高塔

本机预览图：`Docs/tower-overview.png`。截图不随源码上传，可用文末的复核命令重新生成。

从庭院南侧围墙缺口沿木栈道前往高塔。也可双击 `Scripts/PlayTower.command`，直接在塔下进入游戏；这个入口不会清除存档。

- 塔顶观景平台比入口高 **42 米**；外侧 **21 段**连续石台与木平台，每段抬高 2 米。
- 面向浅色边沿，按 **E** 抓边、**Space** 翻上；也可在平台前直接按 Space。转角先走到落脚平台，再面向下一段。A/D 可悬挂横移，左 Ctrl 松手。
- 入口、16 米、32 米和塔顶设检查点，靠近按 E 保存、回复生命。途中坠海后返回最近保存的位置。
- 塔顶有补给和观景平台，HUD 显示攀爬高度。原有三件宝物和出口流程继续保留。
- 高塔属于可玩的原型：攀爬使用现有位置状态机，尚无专用攀爬动画和手脚 IK。

## 操作

| 操作 | 按键 |
| --- | --- |
| 移动 / 视角 | WASD / 鼠标 |
| 奔跑 | 左 Shift |
| 跳跃、翻上平台 | Space |
| 抓边、拾取、检查点、开门、出口 | E |
| 悬挂横移 / 松手 | A、D / 左 Ctrl |
| 瞄准 / 射击 | 鼠标右键 / 左键 |
| 手枪 / 自动步枪 / 换弹 | 1 / 2 / R |
| 使用医疗包 / 投掷手雷 | Q / G |
| 冒险日志 | Tab |
| 清除本项目存档并从头开始 | F5 |

## 可玩流程

1. 从海岛登陆平台出发，沿浅色石灰岩边缘攀爬三级石台。E 抓边，A/D 横移，Space 翻上，Ctrl 松手；在低台阶前按 Space 也可直接攀上。
2. 瞭望台有第一件宝物、补给和不还击的练习哨兵。蓝色信标按 E 保存检查点并恢复生命。
3. 穿越有实际坠落风险的悬索桥。庭院有三名巡逻守卫，会在视线无遮挡时攻击。
4. 搜集庭院钥匙、第二件宝物及补给，沿台阶进入神殿，用钥匙开门。
5. 收集祭坛上的第三件宝物，到出口信标按 E 完成远征。

## 系统

- 攀爬：墙面与平台顶面检测、可攀爬组件标记、抓边、横移、翻越空间检测、分阶段碰撞扫描、空中自动抓边和松手冷却。
- 武器：独立弹匣/备弹、半自动手枪、自动步枪、射速、散布、肩后瞄准、枪口遮挡检测、命中伤害、头部伤害倍率、换弹和切换。
- 手雷：有重力和反弹的弹体、2.2 秒引信、420 cm 爆炸范围和遮挡检测。当前爆炸伤害用于敌人。
- 道具：弹药、医疗包、手雷、钥匙、三件宝物；道具 ID 防止重复拾取，医疗包上限 5、手雷上限 6。
- 存档：检查点保存位置、关键任务状态、已拾取物和补给数量，重新打开游戏可继续。死亡/坠海返回当前检查点，敌人状态保留到本次关卡结束；F5 重开关卡会重置守卫。
- 界面：目标、生命、弹药、道具数量、附近交互提示、悬挂操作提示、任务信标和日志。当前游戏内 UI 使用英文，说明文档为中文。

## 本地完整项目结构

下面的 `Content/` 内容需在本机恢复或生成，未包含在 Git 仓库中。

- `Content/Maps/CliffSanctuary.umap`：可在 Unreal 中直接打开和编辑的关卡。
- `Content/Materials`：石材、岩体、木头、植被、海面、攀爬标记等原生 PBR 材质与材质实例。
- `Content/Characters`：本机 UE 模板附带的人形模型与动画。
- `Source/LostExpedition`：角色、场景、交互物、守卫、手雷、HUD 和游戏模式。
- `Scripts/setup_scene.py`：在 Unreal Python 内重新生成场景和材质；会覆盖该生成地图上的修改。
- `Docs/build.log`、`Docs/runtime-test.txt`：本次实际编译和运行验证结果。

## 当前完成度

这是可玩的单人冒险原型，场景参考海岛悬崖与海堡遗迹的氛围，采用原创布局；并非《神秘海域》某一关的精确复刻。重做版使用真实海岸扫描岩壁、残破拱门、侧塔、旧石墙、断桥、路面植被和云层远景。角色仍是 UE mannequin，攀爬仍采用位置状态机；专用攀爬动画、手脚 IK、绳索摆荡、掩体系统、声音及电影演出尚未制作，部分任务道具与特效仍为占位表达。

## 已接入的免费素材

- UE 官方模板：`/Game/Weapons/Pistol/Meshes/SM_Pistol`、`/Game/Weapons/Rifle/Meshes/SM_Rifle`，保留对应材质、法线和表面贴图。
- UE 官方 ArchVis 样例：`/Game/ArchVis/SampleScene/Tree/HillTree_02`，使用原有枝干和树叶材质。
- 本机已有 Poly Haven CC0 素材：`island_tree_01`、`grass_medium_01`、`cobblestone_floor_01`、`rocky_terrain`、`wood_planks`。
- 新下载的 CC0 扫描素材：`coastal_cliff_02`、`rock_07`、`fern_02`、`wooden_crate_02`；新表面材质：`old_stone_wall_02`、`rock_face`、`aerial_grass_rock`、`worn_mossy_plasterwall`。
- 石墙、岩体、路面和桥面分别使用不同尺度的世界坐标 PBR 材质。海面使用动态波纹法线。
- 完整来源清单在 `Docs/AssetSources/used-assets.json`；本次未加入新下载的 Fab 商店包。
- 本机原关卡与画面备份位于 `Docs/Backups`，不上传 Git。完整恢复需要先补齐 UE 模板和旧版植被/路面素材，再运行海岸素材下载、导入和地图生成脚本；详细顺序见 [素材与恢复说明](Docs/ASSETS.md)。

## 开发与验证

已在本机 UE 5.8.3 / Mac Development 完成编译，在更新后的真实游戏世界中通过 64 项自动检查（0 失败），涵盖官方树木和免费草丛实际放置、PBR 材质应用、三级攀爬、横移、翻越阻挡、桥面支撑、台阶通行、官方武器切换、命中、换弹、补给、任务条件和存档往返；新增从庭院到塔顶的连续胶囊扫描、21 段实际抓边与翻越、塔顶交互及四处检查点支撑检查。

双击 `Scripts/Build.command` 编译编辑器模块；双击 `Scripts/SmokeTest.command` 运行真实 UE 游戏世界中的自动检查。测试使用独立临时存档槽，不会覆盖检查点存档。

高塔复核截图为 `Docs/tower-overview.png`、`Docs/tower-route.png` 和 `Docs/tower-summit.png`。在编辑器启动命令中增加 `-AdventureCapture -TowerCaptureOnly -AdventureCaptureExit`，并执行 `Scripts/editor_view.py`，可重新渲染。

`-WatchtowerVisualReview` 会在真实游戏中进入高塔抓边状态，生成 `Docs/tower-gameplay.png` 后退出，用于复核角色尺度与 HUD；正常游玩请使用 `Scripts/PlayTower.command`。

原场景复核视角位于 `Docs/scene-preview.png`、`Docs/courtyard-preview.png`、`Docs/cliff-overview.png` 和 `Docs/landing-preview.png`。可用 `-AdventureVisualReview` 启动游戏生成 `Docs/gameplay-preview.png`，该验证模式会临时关闭守卫攻击并在截图后退出；正常启动不受影响。
