pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Layouts
import "components"
import "DashboardFormat.js" as Format

Item {
    id: root

    required property var pageRoot
    readonly property var dashboard: pageRoot.dataSource.dashboardData || ({})
    readonly property var game: pageRoot.tacticalGameContext
    readonly property bool bases: Boolean(game?.hasBases)
    readonly property bool summary: pageRoot.commandSummaryMode
    readonly property bool cameraPriority: pageRoot.videoPrimary
    readonly property string profileName: bases ? "超级对抗赛"
        : game?.hasControlPoints ? "联盟赛 3V3" : "联盟赛 1V1"
    readonly property int summaryRows: Math.max(dashboard.redRobots?.length || 0, dashboard.blueRobots?.length || 0)
    readonly property color redAccent: "#fa9083"
    readonly property color blueAccent: "#85c6f2"
    readonly property int side: Number(dashboard.leadingSide || 0)
    readonly property color redSurface: Format.redSurface(side)
    readonly property color blueSurface: Format.blueSurface(side)
    readonly property bool compact: height <= 820
    readonly property int margin: Math.max(12, Math.min(26, Math.round(width * 0.0115)))
    readonly property int gap: compact ? 7 : 8
    readonly property int sideFooterHeight: compact ? Math.max(138, Math.min(205, Math.round(width * 0.103 + 24)))
        : Math.max(145, Math.min(320, Math.round(width * 0.11 + 36)))
    readonly property int redRosterWidth: Math.max(174, Math.round(width * 0.148))
    readonly property int blueRosterWidth: Math.max(206, Math.round(width * 0.1815))
    function eventRows(limit) {
        const events = []
        const territory = root.game?.territoryStatus
        if (root.bases && territory && root.game?.gamePhase === "战斗阶段") {
            if (territory.redFortressRemainingSec > 0)
                events.push({time: header.clock, text: "蓝方正在占领红方堡垒  " + territory.redFortressRemainingSec + "s", color: "#fad18b"})
            if (territory.blueFortressRemainingSec > 0)
                events.push({time: header.clock, text: "红方正在占领蓝方堡垒  " + territory.blueFortressRemainingSec + "s", color: "#fad18b"})
        }
        const incoming = root.pageRoot.dataSource.keyEvents || []
        return events.concat(incoming).slice(0, limit)
    }

    function resourceValue(value) {
        return root.dashboard.economyReported ? Format.number(value) : "—"
    }

    DashboardBackdrop {
        anchors.fill: parent
        redSurface: root.redSurface
        blueSurface: root.blueSurface
        Behavior on redSurface { ColorAnimation { duration: 320 } }
        Behavior on blueSurface { ColorAnimation { duration: 320 } }
    }

    DashboardMatchHeader {
        id: header
        width: parent.width
        anchors.top: parent.top
        game: root.game
        dashboard: root.dashboard
        snapshot: root.pageRoot.matchSnapshot
        redTeamName: root.pageRoot.redTeamName
        blueTeamName: root.pageRoot.blueTeamName
        compact: root.compact
    }

    Rectangle {
        id: settlementBanner
        objectName: "mockSettlementBanner"
        visible: Boolean(root.pageRoot.uiFirstMode)
            && root.pageRoot.matchSnapshot?.phase === "settlement"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.topMargin: 8
        width: Math.min(520, parent.width * 0.34)
        height: 48
        radius: 3
        color: "#e5182a34"
        border.width: 1
        border.color: root.side === 2 ? root.blueAccent : root.redAccent
        z: 40

        Text {
            anchors.centerIn: parent
            text: "比赛结算   " + root.pageRoot.allyScore + " : " + root.pageRoot.enemyScore
            color: "#f2f6f8"
            font.pixelSize: 20
            font.bold: true
        }
    }

    RowLayout {
        id: battlefield
        visible: !root.summary
        anchors.top: header.bottom
        anchors.topMargin: 5
        anchors.bottom: shortcuts.top
        anchors.bottomMargin: root.gap
        anchors.left: parent.left
        anchors.leftMargin: root.margin
        anchors.right: parent.right
        anchors.rightMargin: root.margin
        spacing: root.gap

        ColumnLayout {
            Layout.minimumWidth: root.redRosterWidth
            Layout.preferredWidth: root.redRosterWidth
            Layout.maximumWidth: root.redRosterWidth
            Layout.fillHeight: true
            spacing: root.compact ? 6 : 9
            Roster {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.maximumHeight: root.bases ? Number.POSITIVE_INFINITY : 26 + robots.length * 112
                blue: false
                robots: root.dashboard.redRobots || []
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.sideFooterHeight
                color: Qt.tint("#1c2934", Qt.alpha(root.redSurface, 0.41))
                border.color: Qt.tint("#586871", Qt.alpha(root.redAccent, 0.50))
                DashboardTitleBar {
                    id: logisticsHeader
                    x: 1; y: 1
                    width: parent.width - 2
                    title: (root.pageRoot.allyIsRed ? "红方" : "蓝方") + "后勤"
                    icon: "coin"
                    topColor: Qt.tint("#3b4651", Qt.alpha(root.redSurface, .62))
                    bottomColor: "#22303a"
                }
                GridLayout {
                        anchors.fill: parent
                        anchors.topMargin: logisticsHeader.height + 6
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.bottomMargin: 6
                        columns: 2
                        rowSpacing: 4
                        columnSpacing: 4
                        Repeater {
                            model: ([
                                ["当前金币", root.pageRoot.topStatus.allyRemainingEconomy],
                                ["累计经济", root.pageRoot.topStatus.allyTotalEconomyObtained],
                                ["科技等级", root.pageRoot.topStatus.allyTechLevel],
                                ["加密等级", root.pageRoot.topStatus.allyEncryptionLevel]
                            ]).slice(0, root.bases ? 4 : 2)
                            Rectangle {
                                id: tile
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 2
                                gradient: Gradient {
                                    GradientStop { position: 0; color: "#303039" }
                                    GradientStop { position: 1; color: "#18252e" }
                                }
                                border.color: "#66515b"
                                Column {
                                    x: 6
                                    y: 6
                                    spacing: root.compact ? 4 : 16
                                    Text { text: tile.modelData[0]; font.pixelSize: 11; color: "#b0c0cb" }
                                    Text {
                                        text: root.resourceValue(tile.modelData[1])
                                        font.family: "Bahnschrift"
                                        font.pixelSize: Math.max(16, Math.min(23, Math.round(root.width * 0.0124)))
                                        font.bold: true
                                        color: "#ffffff"
                                    }
                                }
                            }
                        }
                }
            }
            Item { Layout.fillHeight: true; visible: !root.bases }
        }

        Rectangle {
            id: center
            objectName: "commandDashboardCenter"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            color: "#7a0f1823"
            radius: 3
            border.color: "#89677681"
            readonly property int briefGap: root.compact ? 4 : 6
            readonly property int dartsHeight: root.bases ? (root.compact ? 40 : 47) : 0
            readonly property int briefMinimum: root.cameraPriority
                ? (root.bases ? 163 : 118)
                : (root.bases ? (root.compact ? 128 : 148) : (root.compact ? 90 : 100))
            readonly property real aspect: root.cameraPriority ? 16 / 9
                : root.game ? root.game.fieldWidth / root.game.fieldHeight : 28 / 15
            readonly property real mapHeight: Math.max(0, Math.min((width - 10) / aspect + 30,
                height - briefMinimum - briefGap - 5))

            TacticalRadarMap {
                id: map
                objectName: "commandDashboardMap"
                parent: root.summary && !root.bases ? summaryMap
                    : root.cameraPriority ? sideMedia : center
                width: parent.width
                height: parent === center ? center.mapHeight : parent.height
                compactHeader: true
                model: root.pageRoot.tacticalMapModel
                allyStatusModel: root.pageRoot.allyRobotsForMap
                enemyStatusModel: root.pageRoot.enemyRobotsForMap
                allyIsBlue: root.pageRoot.allyIsBlue
                enemyIsBlue: root.pageRoot.enemyIsBlue
                backgroundSource: root.pageRoot.tacticalMapBackgroundSource
                viewMirrored: root.pageRoot.tacticalMapViewMirrored
                pageRoot: root.pageRoot
            }
            Rectangle {
                parent: root.cameraPriority ? center : sideMedia
                width: parent.width
                height: root.cameraPriority ? center.mapHeight : parent.height
                color: "#18232d"
                border.color: "#557280"
                radius: 2

                DashboardTitleBar {
                    id: cameraHeader
                    x: 1; y: 1
                    width: parent.width - 2
                    title: root.cameraPriority ? "工业相机主画面" : "工业相机"
                    icon: "camera"
                    trailing: camera.hasVisibleVideo ? "在线" : "待机"
                }
                CameraPreviewPanel {
                    id: camera
                    objectName: "commandDashboardCamera"
                    anchors.fill: parent
                    anchors.margins: 5
                    anchors.topMargin: cameraHeader.height + 5
                    compactChrome: true
                    centralMode: root.cameraPriority
                    showRealVideo: battlefield.visible
                    showGrid: root.pageRoot.heroCameraGridVisible
                    liveFrameSource: root.pageRoot.heroCameraFrameSource
                    liveFps: root.pageRoot.hasHeroCameraFrame ? root.pageRoot.heroCameraFps : -1
                    status: root.pageRoot.dataSource.cameraPreviewData || ({})
                }
            }
            Rectangle {
                id: darts
                visible: root.bases
                x: 5
                width: parent.width - 10
                height: center.dartsHeight
                y: center.mapHeight + center.briefGap
                color: "#61202b35"
                border.color: "#538eabb0"
                DartBar { anchors.fill: parent; anchors.margins: root.compact ? 2 : 4 }
            }
            Rectangle {
                id: report
                objectName: "commandBattlefieldReport"
                x: 5
                width: parent.width - 10
                y: root.bases
                    ? darts.y + darts.height + center.briefGap
                    : root.cameraPriority
                        ? center.mapHeight + center.briefGap
                        : map.y + map.renderedMapBottom + center.briefGap
                height: Math.max(0, parent.height - y - 5)
                color: "#94111c27"
                border.color: "#59667684"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 1
                    spacing: 0
                    Timeline {
                        objectName: "commandBattlefieldTimeline"
                        Layout.preferredWidth: report.width * 0.36
                        Layout.fillHeight: true
                    }
                    Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#53667684" }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 1
                        Comparisons { Layout.fillWidth: true }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#6c738592" }
                        CarStatus { Layout.fillWidth: true; Layout.fillHeight: true }
                    }
                }
            }
            Rectangle { width: 12; height: 2; x: 0; y: 0; color: "#b5bfc1"; z: 2 }
            Rectangle { width: 2; height: 12; x: 0; y: 0; color: "#b5bfc1"; z: 2 }
            Rectangle { width: 12; height: 2; anchors.right: parent.right; anchors.bottom: parent.bottom; color: "#b5bfc1" }
            Rectangle { width: 2; height: 12; anchors.right: parent.right; anchors.bottom: parent.bottom; color: "#b5bfc1" }
        }

        ColumnLayout {
            Layout.minimumWidth: root.blueRosterWidth
            Layout.preferredWidth: root.blueRosterWidth
            Layout.maximumWidth: root.blueRosterWidth
            Layout.fillHeight: true
            spacing: root.compact ? 6 : 9
            Roster {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.maximumHeight: root.bases ? Number.POSITIVE_INFINITY : 26 + robots.length * 112
                blue: true
                robots: root.dashboard.blueRobots || []
            }
            Rectangle {
                id: sideMedia
                Layout.fillWidth: true
                Layout.preferredHeight: root.bases ? root.sideFooterHeight : root.sideFooterHeight + 52
                color: "#18232d"
                border.color: "#557280"
                radius: 2
            }
            Item { Layout.fillHeight: true; visible: !root.bases }
        }

    }

    ColumnLayout {
        objectName: "commandDashboardSummary"
        visible: root.summary
        anchors.top: header.bottom
        anchors.topMargin: root.compact ? 8 : 15
        anchors.bottom: shortcuts.top
        anchors.bottomMargin: root.compact ? 10 : 18
        anchors.left: parent.left
        anchors.leftMargin: Math.round(root.width * 0.034)
        anchors.right: parent.right
        anchors.rightMargin: Math.round(root.width * 0.034)
        spacing: root.compact ? 9 : 16

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compact ? 30 : 38
            Layout.alignment: Qt.AlignTop
            DashboardGlyph { kind: "squad"; Layout.preferredWidth: 19; Layout.preferredHeight: 19 }
            Caption { text: "全屏战况"; font.pixelSize: root.compact ? 20 : 25; font.bold: true }
            Text {
                text: root.profileName
                color: "#b4cad7"
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            KeyTip { keyText: "Tab"; label: "松开返回" }
            KeyTip { keyText: "Esc"; label: "关闭战况" }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: root.bases
            Layout.minimumHeight: 72 + Math.max(1, root.summaryRows) * (root.compact ? 49 : 67)
            Layout.preferredHeight: Layout.maximumHeight
            Layout.maximumHeight: 72 + Math.max(1, root.summaryRows) * (root.compact ? 66 : 84)
            spacing: root.compact ? 10 : 16
            DashboardSummaryTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                blue: false
                objectName: "commandSummaryRedRoster"
                robots: root.dashboard.redRobots || []
                compact: root.compact
            }
            DashboardSummaryTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                blue: true
                robots: root.dashboard.blueRobots || []
                compact: root.compact
            }
        }
        RowLayout {
            id: summaryDetails
            Layout.fillWidth: true
            Layout.fillHeight: !root.bases
            Layout.minimumHeight: Math.round(root.height * (root.compact ? 0.175 : 0.205))
            Layout.preferredHeight: Layout.minimumHeight
            Layout.maximumHeight: root.bases ? Layout.minimumHeight : Number.POSITIVE_INFINITY
            spacing: 10
            Item {
                id: summaryMap
                objectName: "commandSummaryMap"
                visible: !root.bases
                Layout.preferredWidth: Math.round(summaryDetails.width * 0.43)
                Layout.fillHeight: true
            }
            GridLayout {
                id: summaryReport
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: root.bases ? 2 : 1
                columnSpacing: 10
                rowSpacing: 10
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: root.bases
                    Layout.preferredWidth: root.bases ? summaryReport.width * 0.62 : -1
                    Layout.preferredHeight: root.compact ? 113 : 128
                    color: "#991b252b"
                    border.color: "#5f7079"
                    DashboardTitleBar { id: statisticsTitle; x: 1; y: 1; width: parent.width - 2; title: "赛中统计"; icon: "analysis" }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        anchors.topMargin: statisticsTitle.height + 2
                        Comparisons { Layout.fillWidth: true }
                        CarStatus { Layout.fillWidth: true; Layout.fillHeight: true }
                    }
                }
                Rectangle {
                    Layout.fillWidth: !root.bases
                    Layout.preferredWidth: root.bases ? summaryReport.width * 0.38 - summaryReport.columnSpacing : -1
                    Layout.fillHeight: true
                    color: "#b416232c"
                    border.color: "#5f7079"
                    DashboardTitleBar {
                        id: historyTitle
                        x: 1; y: 1; width: parent.width - 2
                        title: "战场事件"; icon: "event"; tint: "#eed2a7"
                    }
                    Timeline {
                        anchors.fill: parent
                        anchors.margins: 1
                        anchors.topMargin: historyTitle.height + 2
                        expanded: true
                    }
                }
            }
        }
    }

    Rectangle {
        id: shortcuts
        anchors.bottom: parent.bottom
        width: parent.width
        height: root.compact ? 26 : 34
        color: "#ad101a24"
        Rectangle { width: parent.width; height: 1; color: "#7f566774" }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Math.round(root.width * 0.034)
            anchors.rightMargin: root.margin
            spacing: root.compact ? 7 : 13
            KeyTip { keyText: "Tab"; label: "按住看战况" }
            KeyTip { keyText: "M"; label: "切换大地图" }
            KeyTip {
                keyText: "P"; label: "选手端设置"
                TapHandler { onTapped: root.pageRoot.mainWindowContext?.toggleSettingsPanel() }
            }
            KeyTip { keyText: "Ctrl+Y"; label: "地图 / 工业相机" }
            KeyTip { keyText: "Ctrl+T"; label: "切换操作界面" }
            KeyTip {
                visible: root.pageRoot.uiFirstMode
                keyText: "F5"
                label: root.pageRoot.mockPaused ? "继续模拟" : "暂停模拟"
            }
            KeyTip { visible: root.pageRoot.uiFirstMode; keyText: "F6"; label: "重置模拟" }
            KeyTip { visible: root.pageRoot.uiFirstMode; keyText: "F7"; label: "切换场景" }
            Item { Layout.fillWidth: true }
            Text {
                text: root.dashboard.mockMode
                    ? (root.dashboard.leadText || "本地模拟") + " · 运行中"
                    : root.dashboard.statusFresh ? "裁判数据更新中" : "等待裁判数据"
                color: root.dashboard.statusFresh ? "#a6dfc8" : "#e3bc83"
                font.pixelSize: 10
            }
            Text {
                text: map.awaitingRadar ? "等待雷达" : "雷达 " + map.radarAgeMs + " ms"
                color: map.staleRadar ? "#edc189" : "#afc7d5"
                font.pixelSize: 10
            }
        }
    }

    component KeyTip: Row {
        id: hint
        property string keyText
        property string label
        spacing: 4
        Rectangle {
            width: buttonText.implicitWidth + 8; height: 17
            color: "#243744"; border.color: "#587484"; radius: 2
            Text { id: buttonText; anchors.centerIn: parent; text: hint.keyText; color: "#e7eff5"; font.pixelSize: 10 }
        }
        Text { anchors.verticalCenter: parent.verticalCenter; text: hint.label; color: "#b7c9d8"; font.pixelSize: 10 }
    }

    component Caption: Text {
        color: "#e7f0f8"
        font.pixelSize: 13
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }

    component Roster: Rectangle {
        id: roster
        required property bool blue
        required property var robots
        readonly property color surface: blue ? root.blueSurface : root.redSurface
        readonly property color accent: blue ? root.blueAccent : root.redAccent
        color: Qt.tint("#1c2934", Qt.alpha(surface, .55))
        border.color: Qt.tint("#536873", Qt.alpha(accent, 0.61))
        radius: 2
        clip: true
        Behavior on color { ColorAnimation { duration: 320 } }

        DashboardTitleBar {
            id: rosterHeader
            x: 1; y: 1
            width: parent.width - 2
            title: (roster.blue ? "蓝方" : "红方") + "阵容"
            icon: "squad"
            tint: roster.accent
            topColor: Qt.tint("#3b4651", Qt.alpha(roster.surface, .68))
            bottomColor: "#22303a"
        }
        Column {
            id: slots
            anchors.fill: parent
            anchors.margins: 7
            anchors.topMargin: rosterHeader.height + 7
            spacing: root.compact ? 6 : 8
            readonly property real cardHeight: Math.max(0, Math.min(137,
                (height - spacing * (roster.robots.length - 1)) / Math.max(1, roster.robots.length)))
            Repeater {
                model: roster.robots.length
                Item {
                    required property int index
                    id: slot
                    width: slots.width
                    height: slots.cardHeight
                    DashboardRobotCard {
                        anchors.fill: parent
                        robot: roster.robots[slot.index] ?? ({})
                        blue: roster.blue
                        sideSurface: roster.surface
                    }
                    Rectangle {
                        visible: slot.index > 0
                        x: 9; y: -Math.round(slots.spacing / 2) - 1
                        width: Math.max(0, slot.width - 18)
                        height: 2
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: "transparent" }
                            GradientStop { position: .18; color: Qt.alpha(roster.accent, .51) }
                            GradientStop { position: .5; color: "#4c5d68" }
                            GradientStop { position: .82; color: Qt.alpha(roster.accent, .51) }
                            GradientStop { position: 1; color: "transparent" }
                        }
                    }
                }
            }
        }
    }

    component DartBar: RowLayout {
        spacing: 4
        DartBank { Layout.fillWidth: true; Layout.fillHeight: true; dataTargets: root.dashboard.redDarts || [] }
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 48
            border.color: "#526675"
            gradient: Gradient {
                GradientStop { position: 0; color: "#364652" }
                GradientStop { position: 1; color: "#15252e" }
            }
            Text { anchors.centerIn: parent; text: "飞镖"; color: "#dce7ed"; font.pixelSize: 12; font.bold: true }
        }
        DartBank { Layout.fillWidth: true; Layout.fillHeight: true; dataTargets: root.dashboard.blueDarts || [] }
    }

    component DartBank: RowLayout {
        id: bank
        required property var dataTargets
        spacing: 2
        Repeater {
            model: 4
            Rectangle {
                id: chamber
                required property int index
                readonly property int hit: Number(bank.dataTargets[index] || 0)
                readonly property color tint: ["#a8bdcc","#e5ac87","#dbe5ea","#ffd477","#dec7fa","#b4f6ff"][hit]
                Layout.fillWidth: true
                Layout.fillHeight: true
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.tint("#202932", Qt.alpha(chamber.tint, 0.22)) }
                    GradientStop { position: 1; color: "#181f2a" }
                }
                border.color: Qt.alpha(chamber.tint, chamber.hit === 5 ? .63 : .29)
                radius: 2
                Column {
                    anchors.centerIn: parent
                    DashboardGlyph {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: root.compact ? 16 : 20
                        height: width
                        kind: "dart"
                        tint: chamber.tint
                        opacity: chamber.hit ? 1.0 : 0.67
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: ["待确认","前哨","固定","随机固定","随机移动","末端"][chamber.hit]
                        color: chamber.tint
                        font.pixelSize: 10
                    }
                }
            }
        }
    }

    component Comparisons: Rectangle {
        implicitHeight: root.compact ? 33 : 40
        color: "#4a21313e"
        RowLayout {
            anchors.fill: parent
            anchors.margins: root.compact ? 4 : 6
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: Math.round(root.width * .0115)
            Comparison {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                label: "累计伤害"
                red: root.dashboard.redDamage
                blue: root.dashboard.blueDamage
            }
            Comparison {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                label: root.game?.hasControlPoints ? "控制度" : "剩余血量"
                red: root.game?.hasControlPoints ? root.dashboard.redControl : root.dashboard.redHealth
                blue: root.game?.hasControlPoints ? root.dashboard.blueControl : root.dashboard.blueHealth
            }
        }
    }

    component Comparison: ColumnLayout {
        id: comparison
        required property string label
        property var red
        property var blue
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: Format.number(comparison.red); color: root.redAccent; font.bold: true; font.family: "Bahnschrift"; font.pixelSize: 14 }
            Text { text: comparison.label; color: "#e7f0f8"; font.pixelSize: 12; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: Format.number(comparison.blue)
                color: root.blueAccent
                font.bold: true
                font.family: "Bahnschrift"
                font.pixelSize: 14
            }
        }
        Canvas {
            id: bar
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(8, Math.min(13, Math.round(root.height * .01)))
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Connections {
                target: comparison
                function onRedChanged() { bar.requestPaint() }
                function onBlueChanged() { bar.requestPaint() }
            }
            onPaint: {
                const ctx = getContext("2d")
                const total = Number(comparison.red) + Number(comparison.blue)
                ctx.clearRect(0, 0, width, height)
                ctx.save()
                ctx.beginPath()
                ctx.moveTo(6, 0)
                ctx.lineTo(width, 0)
                ctx.lineTo(width - 6, height)
                ctx.lineTo(0, height)
                ctx.closePath()
                ctx.clip()
                ctx.fillStyle = "#394a57"
                ctx.fillRect(0, 0, width, height)
                if (total > 0) {
                    const split = width * Number(comparison.red) / total
                    if (comparison.red > 0) {
                        const red = ctx.createLinearGradient(0, 0, 0, height)
                        red.addColorStop(0, "#f59b88")
                        red.addColorStop(.56, "#bc5a54")
                        red.addColorStop(1, "#97443f")
                        ctx.fillStyle = red
                        ctx.beginPath()
                        ctx.moveTo(0, 0)
                        ctx.lineTo(split + 2, 0)
                        ctx.lineTo(split - (comparison.blue > 0 ? 6 : 0), height)
                        ctx.lineTo(0, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                    if (comparison.blue > 0) {
                        const blue = ctx.createLinearGradient(0, 0, 0, height)
                        blue.addColorStop(0, "#99d9fc")
                        blue.addColorStop(.56, "#518bb9")
                        blue.addColorStop(1, "#3a6586")
                        ctx.fillStyle = blue
                        ctx.beginPath()
                        ctx.moveTo(split + (comparison.red > 0 ? 6 : 0), 0)
                        ctx.lineTo(width, 0)
                        ctx.lineTo(width, height)
                        ctx.lineTo(split - 2, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
                ctx.restore()
            }
        }
    }

    component Timeline: Item {
        id: timeline
        property bool expanded: false
        readonly property int rowHeight: root.compact ? 17 : 21
        readonly property int capacity: Math.max(0, Math.floor((height - (expanded ? 5 : 27)) / rowHeight))
        readonly property var entries: root.eventRows(capacity)
        clip: true
        Rectangle {
            visible: !timeline.expanded && timeline.entries.length > 0
            x: 12
            y: 25
            width: 1
            height: timeline.rowHeight * timeline.entries.length
            color: "#728791"
        }
        Column {
            x: 9; y: 3
            width: parent.width - 18
            Row {
                visible: !timeline.expanded
                height: 22
                spacing: 5
                DashboardGlyph { kind: "event"; anchors.verticalCenter: parent.verticalCenter }
                Caption { text: "战场事件"; anchors.verticalCenter: parent.verticalCenter }
            }
            Repeater {
                model: timeline.entries
                Item {
                    id: event
                    required property var modelData
                    width: parent.width
                    height: timeline.rowHeight
                    Rectangle {
                        visible: timeline.expanded
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: "#40515b"
                    }
                    Rectangle {
                        visible: !timeline.expanded
                        anchors.verticalCenter: parent.verticalCenter
                        x: 1; width: 5; height: 5; radius: 3
                        color: event.modelData.color || "#8fb1c5"
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: timeline.expanded ? 4 : 14
                        anchors.rightMargin: 3
                        spacing: 7
                        Text {
                            Layout.preferredWidth: 37
                            text: event.modelData.time || ""
                            color: event.modelData.color || "#a6c9de"
                            font.pixelSize: 11
                        }
                        Text {
                            Layout.fillWidth: true
                            text: event.modelData.text || ""
                            color: event.modelData.color === "#fad18b" ? "#ffe4a4" : "#eaf0f5"
                            font.pixelSize: timeline.rowHeight < 21 ? 11 : 12
                            elide: Text.ElideRight
                        }
                    }
                }
            }
            Text {
                visible: timeline.entries.length === 0
                text: "暂无战场事件"
                color: "#9ab0c1"
                font.pixelSize: 11
            }
        }
    }

    component CarStatus: Rectangle {
        id: feed
        color: "#52263341"
        readonly property var chips: {
            const items = (root.dashboard.injury || []).map(item => ({label: item.label, value: item.value, color: "#ffa995"}))
            if (root.dashboard.economyDelta) {
                const delta = root.dashboard.economyDelta
                items.push({label: "己方金币", value: (delta > 0 ? "+" : "") + delta, color: "#f7d19f"})
            }
            const team = root.pageRoot.allyIsRed ? root.dashboard.redRobots : root.dashboard.blueRobots
            const own = (team || []).find(robot => robot.selected)
            return items.concat((own?.effects || []).map(effect => ({
                label: effect.label, value: effect.left + "s", color: "#9ce4ca"
            })))
        }
        Column {
            anchors.fill: parent
            anchors.margins: 7
            anchors.topMargin: 4
            spacing: 3
            Row {
                spacing: 7
                Caption { text: "车况"; font.pixelSize: 12 }
                Text {
                    text: root.dashboard.playerName || "等待登录"
                    color: root.pageRoot.allyIsRed ? root.redAccent : root.blueAccent
                    font.pixelSize: 11; font.bold: true
                }
            }
            Item {
                width: parent.width
                height: Math.max(0, parent.height - y)
                clip: true
                Flow {
                    width: parent.width
                    spacing: 5
                    Repeater {
                        model: feed.chips
                        Rectangle {
                            id: statusChip
                            required property var modelData
                            width: chip.implicitWidth + 11
                            height: 17
                            color: "#bb152330"
                            border.color: "#5977899a"
                            radius: 2
                            Text {
                                id: chip
                                anchors.centerIn: parent
                                text: statusChip.modelData.label + " " + statusChip.modelData.value
                                color: statusChip.modelData.color
                                font.pixelSize: 11
                            }
                        }
                    }
                }
                Text {
                    visible: feed.chips.length === 0
                    text: "暂无新变化"
                    color: "#9ab0c1"
                    font.pixelSize: 11
                }
            }
        }
    }
}
