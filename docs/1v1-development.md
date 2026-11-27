# 1v1 客户端体验与接入说明

更新时间：2026-11-27。适用分支：`feature/ui-first-3v3`。

## 当前完成内容

沿用 3v3 的“模拟/回放/真实数据 → MatchSession → MatchUiProjection → QML”链路，
新增完整 1v1 模式，双方各一台 3 号步兵，共用车辆、热量、模块、Buff 和遥测接口。

- 指挥页：复用 3v3 的现有布局，显示 1v1 地图、双方车辆卡、比分、计时、事件和血量对比。
- 操作页：双方各一张顶部卡、本机详情、模块状态、发射机构/底盘状态、准心与热量环。
- 场景：基础比赛、受伤和状态恢复、离线重连、比赛结算；可暂停、重置、倍速、录制和回放。
- 本地按住左键模拟射击，消耗弹量、增加热量，松开冷却；不会把模拟射击当作实车控制。
- 公开卡片不显示内部超电、功率、弹量和热量；蓝方操作视角、头像和友敌关系同样支持。

5×5 米场地与 120 秒战斗时长沿用现有仓库配置。演示中的受伤、恢复和比分变化是
UI 测试动作，不实现或声称替代正式裁判的判胜、伤害、复活规则。

## 如何体验

Linux（先在仓库根目录完成 `cmake --build --preset dev`）：

```bash
cd ~/RM/Client_27
RM_MATCH_PROFILE=rmul1v1 RM_UI_FIRST_MOCK=1 ./build/dev/RoboMasterClient2025
```

Windows CMD（已按仓库说明安装 MSYS2 并完成依赖 setup）：

```bat
set "RM_MATCH_PROFILE=rmul1v1"
set "RM_UI_FIRST_MOCK=1"
start-windows.bat build
```

按 `Ctrl+T` 切换指挥页/操作页。沿用现有设置页选择赛项，模拟面板按钮切换
基础、受伤、离线、结算；`F5` 暂停/继续，`F6` 重新开始，`F7` 在当前赛项内轮换场景。
基础演示在本地第 12 秒进入 `02:00` 战斗阶段，第 60 秒跳到结算，方便快速检查 UI。

指定场景也可以使用 `RM_UI_FIRST_SCENARIO=match_1v1_damage`（另外三个后缀为
`basic`、`offline`、`settlement`）。`RM_UI_FIRST_RECORD` 和 `RM_UI_FIRST_REPLAY`
沿用原有 JSONL 录制/回放方式，保存并恢复 `profile=rmul1v1`。

操作页直接复用 3v3 的顶部卡、左下详情卡、准心热量环和原有状态面板，不新增面板。

默认配置仍为 3v3；上述环境变量仅影响当前启动。正式体验真实源时必须取消
`RM_UI_FIRST_MOCK` 和 `RM_UI_FIRST_REPLAY`，避免实际数据被模拟源遮住。

## 后续通信人员做什么

设置 `competition.profile=rmul1v1`、本机身份 `3` 或 `103`，解析报文后更新现有
`GameData::update*()`。`GameDataMatchSource` 已支持 1v1，会将双方 3 号机器人的数据
交给统一快照与 UI，通常不需要修改页面。

轮腿扩展仍需实际协议填入模型；图传、网络鉴权、实车控制和安全保护要分别联调。
本次验证以 Linux 本地模拟、数据适配与 QML 渲染为准，尚未做 Windows 实机和现场验收。
细节见 [通信接口说明](communication-integration.md)。
