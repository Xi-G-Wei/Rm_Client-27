# UI-first 3v3 本地比赛引擎

第二阶段实现不依赖网络的 JSON 比赛源。它只推进 `MatchSnapshot`，不连接
MQTT、图传、LIF 固件或实车控制。

## 阶段映射

本地阶段与 2027 通信协议 `GameStatus.current_stage` 保持一致：

| 协议值 | 模型值 | 显示 |
|---:|---|---|
| 0 | `not_started` | 未开始 |
| 1 | `preparation` | 准备阶段 |
| 2 | `self_check` | 裁判系统自检 |
| 3 | `countdown` | 5 秒倒计时 |
| 4 | `battle` | 比赛中 |
| 5 | `settlement` | 结算中 |

模拟时钟每秒递减当前阶段的剩余时间。时间线到达阶段动作时，可以用动作中的
`timeRemainingSec` 重置下一阶段时间。

## 场景文件

场景位于 `mock/3v3/`：

- `match_3v3_basic.json`：完整阶段、得分、机器人和超电变化
- `match_3v3_damage.json`：受伤、阵亡和复活
- `match_3v3_offline.json`：数据中断、恢复和低超电
- `match_3v3_settlement.json`：比赛进入结算并结束

根对象字段：

```text
schemaVersion   当前固定为 1
id              场景唯一 ID
name            场景显示名称
durationSec     场景总时长
initial         MatchSnapshot 初始状态
timeline        按场景运行秒数触发的动作
```

动作支持：

```text
atSec             相对场景开始的秒数
phase             切换比赛阶段
timeRemainingSec  重置当前阶段剩余时间
redScore          更新红方比分
blueScore         更新蓝方比分
robotUpdates      按机器人 ID 更新状态
events            追加瞬时事件
```

`robotUpdates` 可以更新血量、在线、存活、热量、超电百分比、缓冲能量、底盘
能量、底盘功率、底盘模式、轮腿模式和轮腿姿态等字段。动作中的超电、底盘和
轮腿字段直接写在各更新对象上，例如：

```json
{
  "id": "red-3",
  "superCapPercent": 25.0,
  "chassisPowerW": 97.0,
  "chassisMode": "spin",
  "wheelLegMode": "transition"
}
```

## 控制接口

`MockMatchSource` 提供：

- `loadFromFile` / `loadFromJson`
- `start` / `pause` / `resume` / `reset` / `restart`
- `setPlaybackRate`，支持 0.25 到 4 倍速
- `advanceByMs`，供测试和录制回放显式推进时间
- `matchSession.snapshot`，供后续 QML 绑定

场景载入时会先完整预演一次时间线，并逐动作校验快照。错误场景不会替换当前
有效场景，避免运行到一半才发现字段错误。

## 启动本地模拟

本地模拟默认关闭，不影响现有比赛数据路径。正常启动客户端后按 `Ctrl+T`
进入 3v3 指挥页；需要动态模拟时使用：

```bash
RM_UI_FIRST_MOCK=1 ./build/dev/RoboMasterClient2025
```

选择其他场景：

```bash
RM_UI_FIRST_MOCK=1 \
RM_UI_FIRST_SCENARIO=match_3v3_offline \
./build/dev/RoboMasterClient2025
```

`RM_UI_FIRST_SCENARIO` 可以填写 `mock/3v3` 下的场景文件名，也可以填写绝对路径
或 `qrc:` 资源路径。

本地模拟运行时可用：

| 快捷键 | 操作 |
|---|---|
| `F5` | 暂停或继续比赛时钟 |
| `F6` | 从当前场景开头重新开始 |
| `F7` | 顺序切换基础、受伤、离线、结算场景 |
| `Shift+F7` | 反向切换场景 |

快捷键只在 `RM_UI_FIRST_MOCK` 模式启用，不会占用正常比赛模式下的功能键。
3v3 指挥页右上区域同时显示可视化模拟控制条，可直接选择基础、受伤、离线、
结算场景，并控制暂停、重置和 0.5/1/2 倍速。该控制条不会出现在正式模式。

## 当前 UI 映射

`MatchUiProjection` 将动态快照投影成现有指挥页字段：

- 比赛头部：阶段、剩余时间、局数、红蓝比分
- 操作页顶部：与指挥页共享同一模拟时钟、比分和双方机器人阵容
- 双方阵容：每方三张机器人卡片
- 机器人卡片：血量、弹量、热量、超电、底盘功率和轮腿状态
- 地图：机器人坐标和朝向
- 战场事件：按时间倒序显示
- 异常状态：低超电、超功率、离线、阵亡改变卡片边框和状态文字
- 地图选车：点击机器人后阵容卡片高亮，并显示血量、超电、功率、底盘和轮腿详情
- 结算提示：进入结算阶段后显示最终模拟比分

真实通信接入后应继续生成 `MatchSnapshot`，再复用该投影。QML 不读取原始
MQTT Payload。

## 当前边界

本地模拟已经接入 3v3 指挥页和操作页顶部，但仍然只读：操作页左下车辆详情、
提示音、视频区域和上行控制还没有使用本地快照。下一阶段将补齐低超电、
超功率、离线、阵亡和结算提示，并增加本地暂停、重置和场景切换入口。
