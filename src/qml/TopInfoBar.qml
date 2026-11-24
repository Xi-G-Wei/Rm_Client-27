/**
 * @file TopInfoBar.qml
 * @brief 顶部信息栏组件
 * @details 主界面顶部栏按官方客户端结构对齐，禁止默认展示 mock UI。
 */

pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Layouts 1.15
import "Tactical" as Tactical

Item {
    id: root
    width: parent ? parent.width : 1920
    height: 170
    readonly property string redTeamLogoSource: "qrc:/images/top_mid/red_team_logo.png"
    readonly property string blueTeamLogoSource: "qrc:/images/top_mid/blue_team_logo.png"
    readonly property int teamNameWidth: 360
    readonly property int teamNameOuterInset: 28

    // === 数据绑定属性 ===
    // gameData 由 MainWindow 注入，顶部栏统一从这个入口读取比赛状态。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    property var tacticalSource: null
    property var uiFirstSource: null
    readonly property bool uiFirstMode: Boolean(root.uiFirstSource?.active)
    readonly property var activeGame: root.uiFirstMode ? root.uiFirstSource : root.gameDataContext
    property int redBaseHP: activeGame ? activeGame.redBaseHealth : 2500
    property int redBaseMaxHP: activeGame ? activeGame.redBaseMaxHealth : 5000
    property int blueBaseHP: activeGame ? activeGame.blueBaseHealth : 3000
    property int blueBaseMaxHP: activeGame ? activeGame.blueBaseMaxHealth : 5000
    property int redOutpostHP: activeGame ? activeGame.redOutpostHealth : 1500
    property int redOutpostMaxHP: activeGame ? activeGame.redOutpostMaxHealth : 1500
    property int blueOutpostHP: activeGame ? activeGame.blueOutpostHealth : 1500
    property int blueOutpostMaxHP: activeGame ? activeGame.blueOutpostMaxHealth : 1500
    property int redOutpostRebuildCount: activeGame ? activeGame.redOutpostRebuildCount : 2
    property int blueOutpostRebuildCount: activeGame ? activeGame.blueOutpostRebuildCount : 2
    property int redOutpostMaxRebuildCount: activeGame ? activeGame.redOutpostMaxRebuildCount : 2
    property int blueOutpostMaxRebuildCount: activeGame ? activeGame.blueOutpostMaxRebuildCount : 2
    property bool showOutpostModule: true
    property int redScore: activeGame ? activeGame.redScore : 0
    property int blueScore: activeGame ? activeGame.blueScore : 0
    property int currentRound: activeGame ? activeGame.currentRound : 1
    property int gameTime: activeGame ? activeGame.remainingTime : 420
    property string gamePhase: activeGame ? activeGame.gamePhase : "未开始"
    property bool gamePaused: activeGame ? Boolean(activeGame.is_paused) : false
    property int redEconomy: activeGame ? activeGame.redEconomy : 550
    property int blueEconomy: activeGame ? activeGame.blueEconomy : 550
    property string redTeamName: activeGame ? activeGame.redTeamName : "红方"
    property string blueTeamName: activeGame ? activeGame.blueTeamName : "蓝方"

    // 顶部栏只允许显示已有真实字段，禁止继续展示 mock 占领/固定 buff 值
    property int redDefenseBonus: activeGame ? activeGame.redDefenseBonus : 0
    property int blueDefenseBonus: activeGame ? activeGame.blueDefenseBonus : 0
    property bool redInvincible: activeGame ? (activeGame.redBaseInvincible || false) : false
    property bool blueInvincible: activeGame ? (activeGame.blueBaseInvincible || false) : false
    readonly property bool hasBases: root.activeGame
        ? Boolean(root.activeGame.hasBases) : true
    readonly property bool showControlPoints: root.activeGame
        ? Boolean(root.activeGame.hasControlPoints) : false
    readonly property bool currentSeason: root.uiFirstMode || (root.activeGame
        ? Boolean(root.activeGame.uses2027Protocol) : false)
    readonly property var territory: root.activeGame ? root.activeGame.territoryStatus : ({})

    readonly property bool redOutpostDestroyed: redOutpostHP <= 0
    readonly property bool blueOutpostDestroyed: blueOutpostHP <= 0
    readonly property bool redInvincibleEffective: redInvincible || (!root.currentSeason && !redOutpostDestroyed)
    readonly property bool blueInvincibleEffective: blueInvincible || (!root.currentSeason && !blueOutpostDestroyed)

    property bool showPerformanceOverlay: false
    property bool showOccupationHints: false
    property double redOccupationProgress: 0.0
    property int redOccupationTime: 0
    property bool isRedOccupied: false

    property double blueOccupationProgress: 0.0
    property int blueOccupationTime: 0
    property bool isBlueOccupied: false

    property string formattedTime: {
        var m = Math.floor(root.gameTime / 60);
        var s = root.gameTime % 60;
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    // === 帧率显示 ===
    FpsDisplay {
        visible: root.showPerformanceOverlay
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 5
        z: 100 // 确保帧率显示位于最上层
    }

    Loader {
        anchors.top: parent.top
        width: root.width
        active: root.currentSeason
        sourceComponent: Tactical.PilotMatchHud {
            objectName: "pilotMatchHud"
            game: root.activeGame
            dashboard: root.tacticalSource?.dashboardData || ({})
            snapshot: root.uiFirstSource?.snapshot || ({})
            redTeamName: root.redTeamName
            blueTeamName: root.blueTeamName
            compact: root.width < 1400
        }
    }

    Item {
        visible: !root.currentSeason
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 0

        // ========================================
        // [中间] 统一背景面板
        // ========================================
        Item {
            id: centerPanel
            width: 340
            height: 86
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 4

            Image {
                anchors.fill: parent
                source: "qrc:/images/top_middle_background_v2.png"
                fillMode: Image.PreserveAspectFit
                opacity: 0.8
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                visible: root.showControlPoints
                text: qsTr("控制度  %1 : %2").arg(root.territory.redPoints ?? "—")
                                         .arg(root.territory.bluePoints ?? "—")
                color: "#e5eaf3"
                font.pixelSize: 12
            }

            // --- 文字覆盖层 ---
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 13
                text: (root.gamePaused ? "比赛暂停" : root.gamePhase) + " · Round - " + root.currentRound
                color: root.gamePaused ? "#FFD54F" : "#66CCFF"
                font.pixelSize: 11
                font.bold: true
                font.family: "Arial"
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 11
                text: root.formattedTime
                color: root.gamePaused ? "#FFD54F" : "#FF3333"
                font.family: "Arial"
                font.pixelSize: 26
                font.bold: true
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 72
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 0
                text: root.redScore.toString()
                color: "#FFFFFF"
                font.pixelSize: 20
                font.bold: true
                font.family: "Arial"
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 72
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 0
                text: root.blueScore.toString()
                color: "#FFFFFF"
                font.pixelSize: 20
                font.bold: true
                font.family: "Arial"
                style: Text.Outline
                styleColor: "#000000"
            }
        }

        // ========================================
        // [中间下方] 经济面板
        // ========================================
        Item {
            id: economyPanel
            width: 200
            height: 80
            anchors.top: centerPanel.bottom
            anchors.topMargin: -5
            anchors.horizontalCenter: parent.horizontalCenter

            Image {
                anchors.fill: parent
                source: "qrc:/images/BattleVolumn_Bg.png"
                fillMode: Image.Stretch
                opacity: 0.8
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 25
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 2
                text: root.redEconomy.toString()
                color: "#FFFFFF"
                font.pixelSize: 24
                font.bold: true
                font.family: "Arial"
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 25
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 2
                text: root.blueEconomy.toString()
                color: "#FFFFFF"
                font.pixelSize: 24
                font.bold: true
                font.family: "Arial"
                style: Text.Outline
                styleColor: "#000000"
            }
        }

        // ========================================
        // [左侧] 红方区域
        // ========================================
        Item {
            id: redArea
            anchors.right: centerPanel.left
            anchors.rightMargin: 7
            anchors.top: parent.top
            height: parent.height
            width: 800

            Text {
                id: redTeamNameText
                width: root.teamNameWidth
                anchors.right: redHealthContainer.right
                anchors.rightMargin: root.teamNameOuterInset
                anchors.top: parent.top
                anchors.topMargin: 0
                text: root.redTeamName
                color: "#FFFFFF"
                font.pixelSize: 17
                minimumPixelSize: 13
                fontSizeMode: Text.HorizontalFit
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                style: Text.Outline
                styleColor: "#101214"
                z: 20
            }

            Item {
                id: redHealthContainer
                width: 470
                height: 56
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 4

                BaseHealthBar {
                    id: redBar
                    visible: root.hasBases
                    width: 332
                    height: 30
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom

                    currentHealth: root.redBaseHP
                    maxHealth: root.redBaseMaxHP
                    isBlue: false
                    invincible: root.redInvincibleEffective
                }

                OutpostStatusBadge {
                    id: redOutpostModule
                    visible: root.hasBases && root.showOutpostModule
                        && (!root.currentSeason || root.territory.redOutpostHp !== undefined)
                    width: 92
                    height: 52
                    anchors.right: redBar.left
                    anchors.rightMargin: 12
                    anchors.verticalCenter: redBar.verticalCenter
                    anchors.verticalCenterOffset: 0
                    opacity: root.redOutpostDestroyed ? 0.55 : 1.0
                    z: 12
                    isBlue: false
                    hp: root.redOutpostHP
                    maxHp: root.redOutpostMaxHP
                    rebuildCount: root.currentSeason ? 0 : root.redOutpostRebuildCount
                    maxRebuildCount: root.redOutpostMaxRebuildCount
                }

                Row {
                    anchors.top: redBar.bottom
                    anchors.topMargin: 5
                    anchors.left: redBar.left
                    anchors.leftMargin: 20
                    spacing: 4
                    visible: root.hasBases && (root.redInvincible || root.redDefenseBonus > 0)

                    Image {
                        source: "qrc:/images/buffs/shield.png"
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: root.redInvincible ? "护盾激活" : (root.redDefenseBonus + "%")
                        color: "#EEEEEE"
                        font.pixelSize: 10
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: root.redInvincible ? "基地状态" : "防御增益"
                        color: "#AAAAAA"
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Item {
                    id: redLogoBadge
                    width: 72
                    height: 72
                    anchors.left: redBar.right
                    anchors.leftMargin: -28
                    anchors.verticalCenter: redBar.verticalCenter
                    anchors.verticalCenterOffset: 0
                    z: 14

                    Image {
                        anchors.fill: parent
                        source: root.redTeamLogoSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }
            }

            // 红方底部栏
                RowLayout {
                    anchors.right: parent.right
                    anchors.rightMargin: -38
                    anchors.top: redHealthContainer.bottom
                    anchors.topMargin: 2
                    spacing: 8

                // 1. 机器人列表
                RowLayout {
                    spacing: 2
                    RobotStatusItem {
                        robotId: 7
                        avatarSource: "qrc:/images/top_robots/red_guard_avatar.png"
                    }
                    RobotStatusItem {
                        robotId: 6
                        avatarSource: "qrc:/images/top_robots/red_teammate_avatar_airplane.png"
                    }
                    RobotStatusItem {
                        robotId: 4
                        avatarSource: "qrc:/images/top_robots/red_teammate_avatar_soldier.png"
                    }
                    RobotStatusItem {
                        robotId: 3
                        avatarSource: "qrc:/images/top_robots/red_teammate_avatar_soldier.png"
                    }
                    RobotStatusItem {
                        robotId: 2
                        avatarSource: "qrc:/images/top_robots/red_teammate_avatar_engineer.png"
                    }
                    RobotStatusItem {
                        robotId: 1
                        avatarSource: "qrc:/images/top_robots/red_teammate_avatar_hero.png"
                    }
                }

                // 2. 占领提示进度环
                ColumnLayout {
                    spacing: 4
                    visible: root.showOccupationHints && root.isRedOccupied

                    Item {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignHCenter

                        // 进度环背景占位圆
                        Rectangle {
                            anchors.fill: parent
                            radius: 16
                            color: "transparent"
                            border.color: "#60333333"    // 半透明边框
                            border.width: 2
                        }

                        Canvas {
                            id: redProgressCanvas
                            anchors.fill: parent
                            rotation: -90 // 从顶部开始绘制
                            property double progress: root.redOccupationProgress
                            property color progressColor: "#FF3333" // 红方颜色

                            onProgressChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d");
                                var centerX = width / 2;
                                var centerY = height / 2;
                                var radius = width / 2 - 2; // 为边框预留空间
                                var startAngle = 0;
                                var endAngle = progress * 2 * Math.PI;

                                ctx.reset();
                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, startAngle, endAngle);
                                ctx.strokeStyle = progressColor;
                                ctx.lineWidth = 3;
                                ctx.stroke();
                            }
                        }

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/images/sundry_bullet.png"
                            width: 16
                            height: 16
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    Text {
                        text: "对方占领中 " + root.redOccupationTime + "s"
                        color: "#FFEB3B"
                        font.pixelSize: 10
                        visible: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // ========================================
        // [右侧] 蓝方区域 (对称镜像)
        // ========================================
        Item {
            id: blueArea
            anchors.left: centerPanel.right
            anchors.leftMargin: 7
            anchors.top: parent.top
            height: parent.height
            width: 800

            Text {
                id: blueTeamNameText
                width: root.teamNameWidth
                anchors.left: blueHealthContainer.left
                anchors.leftMargin: root.teamNameOuterInset
                anchors.top: parent.top
                anchors.topMargin: 0
                text: root.blueTeamName
                color: "#FFFFFF"
                font.pixelSize: 17
                minimumPixelSize: 13
                fontSizeMode: Text.HorizontalFit
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                style: Text.Outline
                styleColor: "#101214"
                z: 20
            }

            Item {
                id: blueHealthContainer
                width: 470
                height: 56
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 4

                BaseHealthBar {
                    id: blueBar
                    visible: root.hasBases
                    width: 332
                    height: 30
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    currentHealth: root.blueBaseHP
                    maxHealth: root.blueBaseMaxHP
                    isBlue: true
                    invincible: root.blueInvincibleEffective
                }

                OutpostStatusBadge {
                    id: blueOutpostModule
                    visible: root.hasBases && root.showOutpostModule
                        && (!root.currentSeason || root.territory.blueOutpostHp !== undefined)
                    width: 92
                    height: 52
                    anchors.left: blueBar.right
                    anchors.leftMargin: 12
                    anchors.verticalCenter: blueBar.verticalCenter
                    anchors.verticalCenterOffset: 0
                    opacity: root.blueOutpostDestroyed ? 0.55 : 1.0
                    z: 12
                    isBlue: true
                    hp: root.blueOutpostHP
                    maxHp: root.blueOutpostMaxHP
                    rebuildCount: root.currentSeason ? 0 : root.blueOutpostRebuildCount
                    maxRebuildCount: root.blueOutpostMaxRebuildCount
                }

                Row {
                    anchors.top: blueBar.bottom
                    anchors.topMargin: 5
                    anchors.right: blueBar.right
                    anchors.rightMargin: 20
                    spacing: 4
                    visible: root.hasBases && (root.blueInvincible || root.blueDefenseBonus > 0)
                    layoutDirection: Qt.RightToLeft

                    Image {
                        source: "qrc:/images/buffs/shield.png"
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: root.blueInvincible ? "护盾激活" : (root.blueDefenseBonus + "%")
                        color: "#EEEEEE"
                        font.pixelSize: 10
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: root.blueInvincible ? "基地状态" : "防御增益"
                        color: "#AAAAAA"
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Item {
                    id: blueLogoBadge
                    width: 72
                    height: 72
                    anchors.right: blueBar.left
                    anchors.rightMargin: -28
                    anchors.verticalCenter: blueBar.verticalCenter
                    anchors.verticalCenterOffset: 0
                    z: 14

                    Image {
                        anchors.fill: parent
                        source: root.blueTeamLogoSource
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }
            }

            // 蓝方底部栏
                RowLayout {
                    anchors.left: parent.left
                    anchors.leftMargin: -38
                    anchors.top: blueHealthContainer.bottom
                    anchors.topMargin: 2
                    spacing: 8

                // 2. 占领提示
                ColumnLayout {
                    spacing: 4
                    visible: root.showOccupationHints && root.isBlueOccupied

                    Item {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignHCenter

                        // 进度环背景占位圆
                        Rectangle {
                            anchors.fill: parent
                            radius: 16
                            color: "transparent"
                            border.color: "#60333333"    // 半透明边框
                            border.width: 2
                        }

                        Canvas {
                            id: blueProgressCanvas
                            anchors.fill: parent
                            rotation: -90 // 从顶部开始绘制
                            property double progress: root.blueOccupationProgress
                            property color progressColor: "#3399FF" // 蓝方颜色

                            onProgressChanged: requestPaint()

                            onPaint: {
                                var ctx = getContext("2d");
                                var centerX = width / 2;
                                var centerY = height / 2;
                                var radius = width / 2 - 2;
                                var startAngle = 0;
                                var endAngle = progress * 2 * Math.PI;

                                ctx.reset();
                                ctx.beginPath();
                                ctx.arc(centerX, centerY, radius, startAngle, endAngle);
                                ctx.strokeStyle = progressColor;
                                ctx.lineWidth = 3;
                                ctx.stroke();
                            }
                        }

                        Image {
                            anchors.centerIn: parent
                            source: "qrc:/images/sundry_bullet.png"
                            width: 16
                            height: 16
                            fillMode: Image.PreserveAspectFit
                        }
                    }

                    Text {
                        text: "对方占领中 " + root.blueOccupationTime + "s"
                        color: "#FFEB3B"
                        font.pixelSize: 10
                        visible: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                // 1. 机器人列表
                RowLayout {
                    spacing: 2
                    RobotStatusItem {
                        robotId: 101
                        avatarSource: "qrc:/images/top_robots/blue_teammate_avatar_hero.png"
                        isBlue: true
                    }
                    RobotStatusItem {
                        robotId: 102
                        avatarSource: "qrc:/images/top_robots/blue_teammate_avatar_engineer.png"
                        isBlue: true
                    }
                    RobotStatusItem {
                        robotId: 103
                        avatarSource: "qrc:/images/top_robots/blue_teammate_avatar_soldier.png"
                        isBlue: true
                    }
                    RobotStatusItem {
                        robotId: 104
                        avatarSource: "qrc:/images/top_robots/blue_teammate_avatar_soldier.png"
                        isBlue: true
                    }
                    RobotStatusItem {
                        robotId: 106
                        avatarSource: "qrc:/images/top_robots/blue_teammate_avatar_airplane.png"
                        isBlue: true
                    }
                    RobotStatusItem {
                        robotId: 107
                        avatarSource: "qrc:/images/top_robots/blue_guard_avatar.png"
                        isBlue: true
                    }
                }
            }
        }
    }
}
