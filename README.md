# Rm_Client-27

本项目是基于开源 RoboMaster 自定义客户端进行的个人二次开发版本，当前主要验证
RMUL 3v3 / 1v1 的 UI-first 开发方式：使用本地模拟比赛数据驱动比赛计时、比分、机器人
状态、超电、底盘、轮腿、告警和地图联动，并为后续真实通信保留统一数据源接口。

## 项目来源

- 直接上游：[SPR-Algorithm/SPR_Client_27](https://github.com/SPR-Algorithm/SPR_Client_27)
- 更早的上游项目：[ClearWei/RM26_Client](https://github.com/ClearWei/RM26_Client)

本仓库保留上游项目的来源信息、版权声明和 MIT 许可证。二次开发部分不代表原
上游战队、DJI 或 RoboMaster 官方立场，与官方机构不存在隶属或合作关系。

## 开发状态

当前已完成两种赛项的本地 JSON 比赛场景、比赛时钟、指挥页、操作页顶部、操作页车辆卡、
模拟控制面板、地图选车、异常状态提示、数据可见性边界和数据健康度评估。

真实 MQTT、视频链路、实车遥测和键鼠遥控尚未在本仓库中完成联调。

## 本地运行

构建说明见 [客户端使用指南](docs/getting-started/user-guide.md)。

运行本地 3v3 模拟比赛：

```bash
RM_UI_FIRST_MOCK=1 ./build/dev/RoboMasterClient2025
```

启动后按 `Ctrl+T` 进入 3v3 指挥页。

运行 1v1 本地模拟（双方各一台步兵）：

```bash
RM_MATCH_PROFILE=rmul1v1 RM_UI_FIRST_MOCK=1 ./build/dev/RoboMasterClient2025
```

沿用现有设置页选择赛项，模拟面板切换当前赛项的基础、受伤、离线、结算场景。1v1 的
体验步骤和通信边界见 [1v1 开发说明](docs/1v1-development.md)。默认启动仍为 3v3。

后续通信人员从 [3v3 / 1v1 通信接口说明](docs/communication-integration.md) 开始，
查看数据入口、字段单位、两种赛项的差异、已接通的 UI 映射以及待实车验证的部分。

## 许可证与素材

项目源代码沿用上游 [MIT License](LICENSE)。第三方代码、地图和素材的授权与署名
分别记录在 [ASSET_LICENSES.md](ASSET_LICENSES.md) 和
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 中。
