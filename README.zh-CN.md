# Lost Expedition / 失落远征

[English](README.md) | [简体中文](README.zh-CN.md)

UE 5.8.3 原生 C++ 第三人称冒险原型。当前关卡为热带海岛：低处是滨海沙滩与浅水，中部是岩石高地，残破塔楼建在高地上；椰子树、乔木、蕨类和草丛形成丛林。塔楼西侧设有可实际攀爬的石把手。

![海岛全景：滨海沙滩、热带丛林、中央高地与残塔](Docs/Images/island-overview.jpg)

## 场景截图

以下图片均为当前海岛关卡的 Unreal Engine 实际画面。

**沙滩与丛林入口**

![从沙滩穿过椰树林，前往高地残塔](Docs/Images/island-beach.jpg)

**高地上的残破塔楼**

![带有窗洞、破损墙体和外墙攀爬路线的石塔](Docs/Images/island-tower.jpg)

**实际攀爬画面**

![角色抓住塔墙石把手，进行贴墙攀爬](Docs/Images/tower-gameplay.jpg)

## 启动与素材

这是源码仓库，包含代码、配置、生成脚本、测试报告和 README 压缩预览图。模型、贴图、地图、原始全分辨率截图、编译产物及存档只保留在本机，不上传 Git。**新克隆需按 [素材与恢复说明](Docs/ASSETS.md) 补齐资源并生成地图后再 Play。**

- `Scripts/OpenEditor.command`：打开编辑器，默认查看整个海岛。
- `Scripts/Play.command`：从沙滩进入游戏，或继续已有检查点存档。
- `Scripts/PlayTower.command`：直接到残塔下体验石把手攀爬，不清除存档。
- `Scripts/Build.command`：编译编辑器模块。
- `Scripts/SmokeTest.command`：运行真实游戏世界中的检查。

Mac 脚本默认使用 `/Users/Shared/Epic Games/UE_5.8`。其他平台使用对应 C++ 工具链构建。内部地图资源名仍为 `/Game/Maps/CliffSanctuary`。

## 场景与路线

1. 从西南侧沙滩出发，穿过椰林，沿连续的林间坡道走向中央高地。
2. 高地约高出海面 22 米；塔顶平台比高地再高 28 米。残塔有窗洞、破损顶墙、断裂屋梁、落石和墙生植物。
3. 塔楼西侧有 23 个凸出的石把手。纵向错位的路线中间需要两次横移，最上方把手可翻上塔顶。
4. 沙滩、林间、塔下和塔顶设有检查点，靠近蓝色信标按 E 保存并恢复生命。
5. 沙滩、树林和塔顶各有一件宝物。找到高地钥匙、打开塔楼东侧门，收齐宝物后返回沙滩出口。

## 操作

| 操作 | 按键 |
| --- | --- |
| 移动 / 视角 | WASD / 鼠标 |
| 奔跑 / 跳跃 | 左 Shift / Space |
| 抓住石把手 / 交互 | E |
| 贴墙向上、向下换手 | W / S |
| 贴墙左右横移 | A / D |
| 最上方把手翻上塔顶 | Space |
| 从塔顶下攀 | 走到西侧缺口，面朝外侧，按 E |
| 松手 | 左 Ctrl |
| 瞄准 / 射击 | 鼠标右键 / 左键 |
| 手枪 / 步枪 / 换弹 | 1 / 2 / R |
| 医疗包 / 手雷 | Q / G |
| 冒险日志 / 清除本海岛存档重开 | Tab / F5 |

攀爬包含朝向检测、相邻把手选择、碰撞扫描、横移、下攀、登顶和反向抓边。程序化手脚姿态让手臂抬向把手、腿部弯曲贴墙。中间把手不能直接翻成站立状态；横向缺口需要使用 A/D。

## 其他玩法

保留手枪、步枪、换弹、肩后瞄准、命中伤害、手雷及爆炸遮挡、医疗包、钥匙、宝物和检查点存档。新海岛使用独立的 `LostExpedition_Island_Checkpoint` 存档槽，避免读取旧地图位置；旧存档未删除。

## 生成与验证

素材恢复后，在系统终端运行 `python3 Scripts/prepare_island_assets.py`，生成原创椰子树并下载沙滩贴图。随后在 Unreal 的 **Tools → Execute Python Script** 中执行 `Scripts/setup_scene.py`。该脚本会覆盖生成地图上的手动修改，请先保存自己的调整。

- `IslandTerrain.h`：岛形、高地与坡道高度函数。
- `ExpeditionWorld.cpp`：连续地形、海水、植被实例与残塔。
- `ExpeditionTower.h`：石把手坐标与塔楼尺寸。
- `ExplorerCharacter.cpp`：攀爬、手脚姿态、战斗与存档。
- `Scripts/create_island_materials.py`：沙滩/岩壁混合、浅海、浪花与丛林材质。

当前记录为 **63 项检查通过、0 项失败**，详见 [测试报告](Docs/runtime-test.txt)，覆盖沙滩至塔下的连续通行、全部把手换手、横向缺口、阻挡、贴墙姿态、登顶、下攀及武器道具存档回归。该结果来自已配置素材的本机工程；新克隆恢复素材后应重新运行检查。

本机预览为 `Docs/island-overview.png`、`island-beach.png`、`island-tower.png` 和 `island-grips.png`。编辑器启动时执行 `Scripts/editor_view.py` 并增加 `-AdventureCapture -AdventureCaptureExit` 可重新生成。`-WatchtowerVisualReview` 会进入真实抓边状态，生成 `Docs/tower-gameplay.png` 后退出。

Git 中仅包含 `Docs/Images/` 下的压缩预览副本。

当前仍是单人冒险原型：使用 UE mannequin 和程序化贴墙姿态，尚无完整动捕攀爬动画、绳索摆荡、声音和电影演出。资源来源见 [素材说明](Docs/ASSETS.md)。
