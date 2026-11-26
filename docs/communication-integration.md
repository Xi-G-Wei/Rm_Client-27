# RMUL 3v3 客户端通信接口接入说明

更新时间：2026-11-26  
适用分支：`feature/ui-first-3v3`  
适用赛项：RMUL 3v3  
当前基线：`3993db9`

## 1. 文档目的

当前客户端的软件主体已完成，比赛状态、机器人数据、地图、告警和操作界面已经能够由统一比赛快照驱动；真实 `GameData`、MQTT、录像回放和模拟数据源的接口均已预留。

通信组后续需要完成的是把真实比赛协议数据送入这些接口，并验证数据单位、更新频率、权限范围和异常恢复。正常接入时不需要重写 QML 页面，也不应让 QML 直接解析原始 MQTT/Protobuf 报文。

## 2. 总体数据链路

```text
MQTT / UDP / 回放文件
        ↓
NetworkManager / MqttClient
        ↓
GameData（原始业务状态和遥测）
        ↓
GameDataMatchSource（真实数据适配）
        ↓
MatchSession（统一比赛快照、校验）
        ↓
MatchUiProjection（UI 字段投影）
        ↓
Qt QML 指挥页、操作页和状态面板
```

上行控制方向：

```text
键鼠 / 页面按钮
        ↓
MainWindow / QML
        ↓
NetworkManager
        ↓
MQTT Protobuf 上行 Topic
```

## 3. 接口职责

| 接口 | 责任 | 通信组是否应直接操作 |
| --- | --- | --- |
| `NetworkManager` / `MqttClient` | MQTT/UDP 连接、收发、Protobuf 解析 | 是 |
| `GameData` | 保存比赛、机器人和事件状态 | 是，通过现有 `update*` / `processProtocolData` |
| `GameDataMatchSource` | 将 `GameData` 转换为统一比赛快照 | 需要补充字段映射时修改 |
| `MatchSession` | 快照校验和原子替换 | 否，内部自动调用 |
| `MatchUiProjection` | 将统一快照转换为 QML 字段 | 一般不需要修改 |
| `MatchSourceController` | 模拟、回放和真实数据源切换 | 否，已接入 |
| QML 页面 | 显示状态和执行界面交互 | 禁止直接处理原始协议 |

核心文件：

- `src/network/NetworkManager.h`
- `src/network/MqttClient.h`
- `src/core/GameData.h`
- `src/core/GameDataMatchSource.cpp`
- `src/core/MatchSession.h`
- `src/core/MatchSourceController.h`
- `src/ui/MatchUiProjection.h`
- `src/network/proto/robomaster.proto`

## 4. 当前运行配置

RMUL 3v3 真实数据模式要求比赛配置为 `rmul3v3`：

```json
{
  "competition": {
    "profile": "rmul3v3",
    "mqtt_broker": "127.0.0.1",
    "mqtt_port": 3333
  }
}
```

也可以使用环境变量覆盖：

```bash
export RM_MATCH_PROFILE=rmul3v3
export RM_MQTT_BROKER=127.0.0.1
export RM_MQTT_PORT=3333
export RM_CLIENT_ROBOT_ID=3
```

启动时不要设置 `RM_UI_FIRST_MOCK`。未启用模拟或回放时，主程序会创建 `GameDataMatchSource` 并将真实 `GameData` 接入 UI。

如果仍处于 `rmuc2027`、`rmul1v1` 或 `legacy2026`，`GameDataMatchSource` 会保持不可用并提示等待 RMUL 3v3 配置。

## 5. 入站数据接入

### 5.1 推荐调用链

新 Topic 处理必须沿用现有链路：

```text
Topic → Protobuf 反序列化 → GameData::update*() / processProtocolData()
      → GameData 信号 → GameDataMatchSource::refresh()
      → MatchSession::replaceSnapshot()
      → MatchUiProjection 和 QML
```

不要在 Topic 回调中直接设置 QML 属性，也不要为单个页面复制一份比赛状态。

典型处理代码：

```cpp
robomaster::GameStatus message;
if (!message.ParseFromArray(payload.constData(), payload.size())) {
    // 记录 topic、长度和解析错误；不要用半份数据覆盖上一帧
    return;
}
gameData->updateGameStatus(message);
```

### 5.2 3v3 首轮联调必须接通的数据

| Topic | 主要字段 | UI 用途 | 现有入口 |
| --- | --- | --- | --- |
| `GameStatus` | 阶段、倒计时、比分、轮次、暂停、结果 | 顶部计时、阶段和比分 | `updateGameStatus` |
| `GlobalUnitStatus` | 基地/前哨血量、机器人血量、允许弹量、总伤害、控制度 | 队伍状态、兵种血量、资源页 | `updateGlobalUnitStatus` |
| `RobotStaticStatus` | 连接、存活、机器人 ID/类型、等级、最大血量、热量上限、功率和能量上限 | 判断在线、计算比例 | `updateRobotStaticStatus` |
| `RobotDynamicStatus` | 当前血量、当前热量、底盘能量、缓冲能量、经验、允许弹量、脱战状态 | 生命条、热量、能量和车辆卡 | `updateRobotDynamicStatus` |
| `RobotPosition` | 机器人 ID、位置、高度、朝向 | 地图标记和方向 | `updateRobotPosition` |
| `GlobalLogisticsStatus` | 经济、累计经济、科技和加密等级 | 经济与队伍资源 | `updateGlobalLogisticsStatus` |
| `Event` | 事件 ID 和参数 | 战场事件、击杀和提示 | `processProtocolData` |
| `RobotModuleStatus` | 各硬件模块在线状态 | 左下模块状态 | `updateRobotModuleStatus` |

建议第二批接入：

- `Buff`：增益和剩余时间。
- `RadarInfoToClient`：敌方位置和雷达信息。
- `RobotInjuryStat`：伤害类型统计。
- `RobotRespawnStatus`：复活进度和兑换复活。
- `PenaltyInfo`：判罚、超速和射击锁定。
- `RuneStatusSync`：能量机关状态。
- `SentryStatusSync`、`AirSupportStatusSync`：哨兵和空中支援状态。
- `CustomByteBlock`：视频负载和自定义扩展数据。

完整订阅读取列表见 `src/network/MqttClient.h` 和 `NetworkManager.cpp`。

## 6. 数据单位和未知值

统一快照遵循以下约定：

| 数据 | 单位 |
| --- | --- |
| 血量、热量、弹量、功率上限 | 数值按协议字段解释 |
| 功率 | W |
| 能量、缓冲能量 | J |
| 超级电容 | 0～100% |
| 地图位置 | 输入为米；`MatchSnapshot` 中归一化为 0～1 |
| 朝向、关节角 | 度 |
| 轮速 | m/s |
| 轮腿高度 | mm |
| 倒计时 | 秒 |

未知值必须使用：

```cpp
kUnknownMatchMetric = -1;   // 整数未知值
kUnknownMatchReal = -1.0;   // 浮点未知值
```

不能把“没有收到”直接当成 0。否则会把未收到的弹量、热量、超电或位置错误显示为真实零值。

Protobuf 的 `optional` 字段未出现时，应保留“未上报”语义；只有协议明确规定该字段存在且值可以为 0 时，才更新为 0。

## 7. 3v3 快照约束

当前真实数据适配器面向 3v3：

- 比赛配置必须是 `RM::MatchProfile::Rmul3v3`。
- 当前机器人列表按车位 `1、3、4` 生成双方机器人。
- 当前角色默认映射为：车位 1 是重装，车位 3、4 是步兵。
- 如果新规则调整车位或角色，需要同步修改 `GameDataMatchSource::buildSnapshot()`。
- 己方操作机器人标记为 `Private`，显示内部超电、功率和底盘数据。
- 其他机器人标记为 `Public`，不显示内部遥测。
- 比赛快照必须通过 `MatchSession::replaceSnapshot()` 校验，校验失败时不会替换当前有效快照。

轮腿、底盘、能量等字段已经在 `MatchSnapshot` 中预留独立结构。通信代码提供真实数据后，需要在 `GameDataMatchSource` 中补充映射，不能继续写回模糊的 `chassisPower` 字段。

## 8. 数据健康度

`GameDataMatchSource` 每 `100 ms` 将当前 `GameData` 投影为一份比赛快照，同时监听比赛、机器人和事件更新信号立即刷新。

数据健康度阈值：

- 小于等于 `1500 ms`：正常。
- `1500～5000 ms`：延迟。
- 大于 `5000 ms`：过期。

通信组需要保证：

- 断线后不继续伪造新数据时间。
- 解析失败不能覆盖上一份有效快照。
- 重连后能够重新接收完整静态状态，不能只补动态状态。
- 高频视频数据不要进入比赛状态快照。

## 9. 上行控制接口

现有上行 Topic 和入口：

| Topic | 接口 | 频率或说明 |
| --- | --- | --- |
| `KeyboardMouseControl` | `NetworkManager::sendKeyboardMouseControl` | 最快约 75 Hz，内部限频 13 ms |
| `CommonCommand` | `sendCommonCommand` | 复活、兑换等通用命令，不高于 10 Hz |
| `MapClickInfoNotify` | `sendMapClickInfo` | 地图标记，坐标单位米 |
| `RobotPerformanceSelectionCommand` | `sendRobotPerformanceSelection` | 射击/底盘性能体系选择 |
| `AirSupportCommand` | `sendAirSupportCommand` | 1 Hz |
| `AssemblyCommand` | `sendAssemblyCommand` | 1 Hz |
| `DartCommand` | `sendDartCommand` | 目标、开闸、确认发射 |
| `HeroDeployModeEventCommand` | `sendHeroDeployMode` | 部署模式 |
| `RuneActivateCommand` | `sendRuneActivate` | 能量机关激活 |
| `CustomControl` | `sendData(QByteArray...)` | 自定义扩展载荷 |

键鼠字段：

- `mouse_x`、`mouse_y`、`mouse_z`：本帧相对增量，不是屏幕绝对坐标。
- `left_button_down`、`right_button_down`、`mid_button_down`：鼠标按键状态。
- `keyboard_value`：16 位按键位图，按下为 1。

| Bit | 按键 | Bit | 按键 |
| --- | --- | --- | --- |
| 0 | W | 8 | R |
| 1 | S | 9 | F |
| 2 | A | 10 | G |
| 3 | D | 11 | Z |
| 4 | Shift | 12 | X |
| 5 | Ctrl | 13 | C |
| 6 | Q | 14 | V |
| 7 | E | 15 | B |

注意：当前 2027/RMUL 3v3 配置会对部分旧 UDP 和 `KeyboardMouseControl` 入口主动返回失败，等待按新版协议重新确认。通信组接入时可以在保持公共方法签名不变的前提下替换内部实现，但必须重新验证频率、字段语义、失联保护和发射安全联锁。

## 10. 新字段接入步骤

新增一个比赛字段时按以下顺序处理：

1. 在 `src/network/proto/robomaster.proto` 增加字段。
2. 在 `NetworkManager` 或 `MqttClient` 中解析 Topic 并调用对应 `GameData::update*()`。
3. 在 `GameData` 的机器人、队伍或比赛结构体中保存该字段。
4. 在 `GameDataMatchSource::buildSnapshot()` 中映射到 `MatchSnapshot`。
5. 在 `MatchUiProjection` 中增加明确的 UI 字段，避免复用语义不清的名称。
6. 在 QML 中绑定该字段。
7. 添加单元测试、非法数据测试和模拟场景，再与真实报文比对。

如果没有完成第 2～4 步，不要直接修改 QML 读取原始报文。

## 11. 联调与验收

### 11.1 使用模拟数据先验证 UI

```bash
cd ~/RM/Client_27
RM_UI_FIRST_MOCK=1 ./build/dev/RoboMasterClient2025
```

启动后按 `Ctrl+T` 切换指挥页和操作页，确认模拟状态能正常变化。

### 11.2 录制真实报文或回放

```bash
export RM_UI_FIRST_REPLAY=/absolute/path/match.jsonl
./build/dev/RoboMasterClient2025
```

录制模拟比赛：

```bash
RM_UI_FIRST_MOCK=1 \
RM_UI_FIRST_RECORD=/absolute/path/match.jsonl \
./build/dev/RoboMasterClient2025
```

MQTT 日志默认写入项目 `tmp/log/mqtt_full_时间.jsonl`，可用 `RM_LOG_DIR` 修改目录。

### 11.3 通信验收清单

- `GameStatus` 能让计时、阶段和比分正确变化。
- `GlobalUnitStatus` 能让机器人血量、弹量和控制度正确变化。
- `RobotStaticStatus` 到达后，静态上限字段不会被动态消息覆盖。
- `RobotDynamicStatus` 的热量、能量、脱战状态与真实机器人一致。
- `RobotPosition` 到达后，地图位置和朝向换算正确。
- 缺少字段显示“未知”，不会错误显示为 0。
- 断线、重连、乱序和解析失败不会冻结或污染 UI。
- 己方内部数据与敌方公开数据没有越权显示。
- 上行键鼠频率、位图、按钮和失联归零行为已验证。
- Windows 和比赛主机上完成至少一次完整比赛流程验收。

## 12. 推荐分工

- 通信组：Topic、连接、Protobuf、重连和原始消息日志。
- 电控/机器人组：确认字段单位、坐标系、频率和状态含义。
- 客户端组：`GameData` 字段、快照映射、UI 投影和界面验收。

任何字段单位、坐标系或未知值含义发生变化时，应同步修改本文档和对应测试。

当前仓库的协议清单文件把 schema 对齐状态标记为 `in-progress`，因此正式联调前仍需用最新版赛事协议逐项核对 `robomaster.proto`，不能只依赖旧注释或本地模拟字段。
