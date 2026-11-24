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

## 当前边界

本阶段没有把 `MockMatchSource` 注入主窗口，因此现有界面仍使用原来的静态
Mock。下一阶段才把它接入 3v3 指挥页，让倒计时和机器人卡片真正动起来。
