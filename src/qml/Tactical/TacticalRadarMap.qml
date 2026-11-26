pragma ComponentBehavior: Bound

import QtQuick 2.15
import ".." as HUD
import "../RobotMapSymbol.js" as RobotSymbol

// 战术雷达地图 — 三层：官方小地图底图 + 机器人位置/方向 + 战术叠加
Rectangle {
    id: root
    color: root.compactHeader ? "transparent" : "#172127"
    border.color: "#455760"
    border.width: root.compactHeader ? 0 : root.visualScale
    radius: 3 * root.visualScale

    property var model: ({})
    property var allyStatusModel: []
    property var enemyStatusModel: []
    property bool allyIsBlue: false
    property bool enemyIsBlue: true
    property string backgroundSource: ""
    property bool viewMirrored: false
    property var pageRoot: null
    // 大地图按目标逻辑分辨率缩放；普通战术页保持 1.0，大屏上的文字、标记和血条随地图放大。
    property real visualScale: 1.0
    // 4K 大地图的 Canvas 像素量较大，使用 Qt 私有 Canvas 线程，避免 10 Hz 雷达刷新占满 GUI 线程。
    property bool threadedCanvasRendering: false
    property bool largeView: false
    property bool compactHeader: false
    property int radarAgeMs: root.model.radarAgeMs ?? -1
    readonly property bool awaitingRadar: root.radarAgeMs < 0 || root.radarAgeMs >= 99999
    readonly property string radarStatus: root.pageRoot && root.pageRoot.linkHealth
        ? (root.pageRoot.linkHealth.radarStatus || "") : ""
    readonly property bool staleRadar: !root.awaitingRadar
        && (root.radarStatus === "stale" || root.radarStatus === "lost")
    readonly property color redColor: "#f18a80"
    readonly property color blueColor: "#82b9f3"
    readonly property color allyColor: root.allyIsBlue ? root.blueColor : root.redColor
    readonly property color enemyColor: root.enemyIsBlue ? root.blueColor : root.redColor
    readonly property color healthBarFrameColor: "#465860"
    readonly property color healthBarTrackColor: "#243138"
    readonly property color healthBarPanelColor: "#101b21"
    readonly property real headerHeight: (root.compactHeader ? 30 : 38) * root.visualScale
    readonly property real footerHeight: (root.compactHeader ? 0 : root.largeView || root.currentSeason ? 26 : 8) * root.visualScale
    readonly property real sideInset: (root.compactHeader ? 5 : 10) * root.visualScale
    readonly property real renderedMapBottom: mapArea.y + mapArea.height

    onAllyIsBlueChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }

    onEnemyIsBlueChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    onViewMirroredChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    onVisualScaleChanged: {
        fallbackGrid.requestPaint()
        mapCanvas.requestPaint()
    }
    // gameData 由 MainWindow 注入，地图内统一从这个入口读取当前机器人编号。
    // qmllint disable unqualified
    readonly property var gameDataContext: typeof gameData !== "undefined" ? gameData : null
    // qmllint enable unqualified
    readonly property bool currentSeason: root.pageRoot
        && root.pageRoot.uiFirstMode ? true : root.gameDataContext
            ? Boolean(root.gameDataContext.uses2027Protocol) : false
    readonly property real fieldWidth: root.pageRoot && root.pageRoot.uiFirstMode
        ? Number(root.pageRoot.dataSource?.fieldWidth || 12) : root.currentSeason
            ? root.gameDataContext.fieldWidth : 28
    readonly property real fieldHeight: root.pageRoot && root.pageRoot.uiFirstMode
        ? Number(root.pageRoot.dataSource?.fieldHeight || 8) : root.currentSeason
            ? root.gameDataContext.fieldHeight : 15
    readonly property real fieldAspect: root.fieldWidth / root.fieldHeight
    readonly property string fieldName: !root.currentSeason ? "2026 联调"
        : (root.fieldWidth === 28 ? "超级对抗赛" : (root.fieldWidth === 12 ? "联盟赛 3V3" : "联盟赛 1V1"))
    readonly property var objectiveData: root.pageRoot
        && root.pageRoot.tacticalTerritoryData !== undefined
            ? root.pageRoot.tacticalTerritoryData
            : (root.gameDataContext?.territoryStatus || ({}))
    readonly property var objectiveLocations: root.currentSeason && root.fieldWidth === 28 ? [
        { key: "redBase", x: 24.11, y: 75, base: true, red: true },
        { key: "blueBase", x: 255.89, y: 75, base: true, red: false },
        { key: "redOutpost", x: 109.76, y: 113.4, base: false, red: true },
        { key: "blueOutpost", x: 170.24, y: 36.6, base: false, red: false }
    ] : []
    readonly property var fortressLocations: root.currentSeason && root.fieldWidth === 28
        && root.gameDataContext?.gamePhase === "战斗阶段" ? [
        { fortress: "redFortressRemainingSec", base: "redBaseControl", x: 65.71, red: true },
        { fortress: "blueFortressRemainingSec", base: "blueBaseControl", x: 214.29, red: false }
    ] : []

    function objectiveStatus(objective) {
        const progress = Number(root.objectiveData[objective.key + "ProgressSec"] || 0)
        if (progress !== 0)
            return (progress > 0 ? "己方推进  " : "对方推进  ") + Math.abs(progress) + " s"
        const control = root.objectiveData[objective.key + "Control"]
        if (!objective.base) {
            return ["", "己方完全控制", "己方临时控制", "对方临时控制", "对方完全控制"][control] || ""
        }
        const state = root.objectiveData[objective.key + "Status"]
        return ["100% 防御", "装甲闭合", "装甲展开"][state] || ""
    }

    readonly property var terrainLabels: !root.currentSeason ? [] : (
        root.fieldWidth === 28 ? [
            { x: 56, y: 27, label: "高地" },
            { x: 224, y: 123, label: "高地" },
            { x: 132, y: 49, label: "中央高地" },
            { x: 119, y: 91, label: "机库" },
            { x: 161, y: 59, label: "机库" },
            { x: 107, y: 131, label: "坡道" },
            { x: 173, y: 19, label: "坡道" },
            { x: 119, y: 145, label: "隧道" },
            { x: 161, y: 5, label: "隧道" },
            { x: 80, y: 121, label: "隧道" },
            { x: 200, y: 29, label: "隧道" },
            { x: 174, y: 136, label: "隧道" },
            { x: 106, y: 14, label: "隧道" }
        ] : root.fieldWidth === 12 ? [
            { x: 12.5, y: 12, label: "补给区" }, { x: 107.5, y: 68, label: "补给区" },
            { x: 35, y: 21, label: "高地" }, { x: 85, y: 59, label: "高地" },
            { x: 35.5, y: 11.8, label: "隧道" }, { x: 84.5, y: 68.2, label: "隧道" },
            { x: 35, y: 32, label: "高地控制区" }, { x: 85, y: 48, label: "高地控制区" },
            { x: 60, y: 48, label: "中央控制区" }
        ] : [
            { x: 5.5, y: 7.6, label: "启动区" }, { x: 44.5, y: 42.4, label: "启动区" },
            { x: 17.4, y: 25.5, label: "掩体" }, { x: 32.6, y: 24.5, label: "掩体" }
        ])
    readonly property int currentRobotNumber: root.pageRoot
        && root.pageRoot.operatorRobotNumber !== undefined
            ? Number(root.pageRoot.operatorRobotNumber)
            : (root.gameDataContext
                ? Number(root.gameDataContext.currentRobotId || 0) % 100 : 0)
    readonly property color selfRobotColor: "#9be1c6"
    onCurrentRobotNumberChanged: mapCanvas.requestPaint()
    function robotNumberText(robot) {
        var number = Number(robot && robot.number !== undefined ? robot.number : 0)
        return number > 0 ? number.toString() : ""
    }

    function clampNormalized(value, fallbackValue) {
        var number = Number(value)
        if (isNaN(number))
            number = fallbackValue !== undefined ? Number(fallbackValue) : 0
        return Math.max(0, Math.min(1, number))
    }

    function normalizeAngle(angle) {
        var value = Number(angle)
        if (isNaN(value))
            return 0
        value = value % 360
        if (value < 0)
            value += 360
        return value
    }

    function mapNormX(x) {
        var normalized = root.clampNormalized(x, 0)
        return root.viewMirrored ? 1.0 - normalized : normalized
    }

    function mapNormY(y) {
        var normalized = root.clampNormalized(y, 0)
        return root.viewMirrored ? 1.0 - normalized : normalized
    }

    function mapAngle(angle) {
        var normalized = root.normalizeAngle(angle)
        return root.viewMirrored ? root.normalizeAngle(normalized + 180) : normalized
    }

    function transformRobot(robot) {
        if (!robot)
            return robot
        var transformed = {}
        for (var key in robot)
            transformed[key] = robot[key]
        transformed.x = root.mapNormX(robot.x)
        transformed.y = root.mapNormY(robot.y)
        if (robot.angle !== undefined)
            transformed.angle = root.mapAngle(robot.angle)
        return transformed
    }

    function transformRobots(robots) {
        var transformed = []
        for (var i = 0; i < robots.length; ++i)
            transformed.push(root.transformRobot(robots[i]))
        return transformed
    }

    function transformRoute(route) {
        if (!route)
            return route
        var transformed = {}
        for (var key in route)
            transformed[key] = route[key]
        transformed.fromX = root.mapNormX(route.fromX)
        transformed.toX = root.mapNormX(route.toX)
        transformed.fromY = root.mapNormY(route.fromY)
        transformed.toY = root.mapNormY(route.toY)
        return transformed
    }

    function transformRoutes(routes) {
        var transformed = []
        for (var i = 0; i < routes.length; ++i)
            transformed.push(root.transformRoute(routes[i]))
        return transformed
    }

    function transformDangerZone(zone) {
        if (!zone)
            return zone
        var transformed = {}
        for (var key in zone)
            transformed[key] = zone[key]
        transformed.centerX = root.mapNormX(zone.centerX)
        transformed.centerY = root.mapNormY(zone.centerY)
        return transformed
    }

    function transformDangerZones(zones) {
        var transformed = []
        for (var i = 0; i < zones.length; ++i)
            transformed.push(root.transformDangerZone(zones[i]))
        return transformed
    }

    function transformBuffZone(zone) {
        if (!zone)
            return zone
        var transformed = {}
        for (var key in zone)
            transformed[key] = zone[key]
        transformed.x = root.mapNormX(zone.x)
        transformed.y = root.mapNormY(zone.y)
        return transformed
    }

    function transformBuffZones(zones) {
        var transformed = []
        for (var i = 0; i < zones.length; ++i)
            transformed.push(root.transformBuffZone(zones[i]))
        return transformed
    }

    function isSelfRobot(robot) {
        if (!robot)
            return false
        return Number(robot.number || 0) === root.currentRobotNumber
    }

    function normalizedRobotScreenY(robot) {
        return 1 - Number(robot && robot.y !== undefined ? robot.y : 0)
    }

    function isRobotHit(robot, normalizedX, normalizedY) {
        if (!robot)
            return false
        var dx = normalizedX - Number(robot.x || 0)
        var dy = normalizedY - root.normalizedRobotScreenY(robot)
        return dx * dx + dy * dy < 0.0025
    }

    function mergedRobotModel(positionRobots, statusRobots) {
        var statusById = {}
        for (var i = 0; i < statusRobots.length; ++i) {
            var statusRobot = statusRobots[i]
            statusById[String(statusRobot.id || "")] = statusRobot
        }

        var merged = []
        for (var j = 0; j < positionRobots.length; ++j) {
            var positionedRobot = positionRobots[j]
            var robotId = String(positionedRobot.id || "")
            var mergedRobot = {}
            for (var key in positionedRobot) {
                mergedRobot[key] = positionedRobot[key]
            }
            var matchedStatus = statusById[robotId]
            if (matchedStatus) {
                for (var statusKey in matchedStatus) {
                    if (mergedRobot[statusKey] === undefined) {
                        mergedRobot[statusKey] = matchedStatus[statusKey]
                    }
                }
            }
            merged.push(mergedRobot)
        }
        return merged
    }

    DashboardTitleBar {
        x: 1; y: 1
        width: parent.width - 2
        visible: root.compactHeader
        title: "战术地图"
        icon: "map"
        subtitle: width >= 400 ? root.fieldName : ""
        trailing: width < 400 ? (root.allyIsBlue ? "蓝方视角" : "红方视角")
            : root.fieldWidth + " × " + root.fieldHeight + " m   │   " + (root.allyIsBlue ? "蓝方视角" : "红方视角")
        topColor: "#3d334550"
        bottomColor: "#91314451"
    }

    Row {
        id: mapTitle
        visible: !root.compactHeader

        anchors.left: parent.left
        anchors.leftMargin: root.sideInset
        anchors.verticalCenter: headerRule.verticalCenter
        anchors.verticalCenterOffset: -root.headerHeight / 2 + 2 * root.visualScale
        spacing: 10 * root.visualScale

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "战术地图"
            color: "#edf2f0"
            font.pixelSize: root.largeView ? Math.max(15, 14 * root.visualScale) : 14 * root.visualScale
            font.weight: Font.DemiBold
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.width >= 355 * root.visualScale
            text: root.fieldName + "  " + root.fieldWidth + " × " + root.fieldHeight + " m"
            color: "#a9babd"
            font.pixelSize: 10 * root.visualScale
        }
    }

    Row {
        visible: !root.compactHeader
        anchors.right: parent.right
        anchors.rightMargin: root.sideInset
        anchors.verticalCenter: mapTitle.verticalCenter
        spacing: 5 * root.visualScale

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 5 * root.visualScale
            height: width
            radius: width / 2
            color: root.awaitingRadar ? "#83949a" : (root.staleRadar ? "#edc189" : "#9be1c6")
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.awaitingRadar ? "等待雷达" : "雷达 " + root.radarAgeMs + " ms"
            color: root.awaitingRadar ? "#b7c4c5" : (root.staleRadar ? "#edc189" : "#e3edeb")
            font.pixelSize: 10 * root.visualScale
        }
    }

    Rectangle {
        id: headerRule
        visible: !root.compactHeader
        x: root.sideInset
        y: root.headerHeight - 4 * root.visualScale
        width: Math.max(0, root.width - 2 * x)
        height: root.visualScale
        color: "#34444b"
    }

    Rectangle {
        id: mapArea
        objectName: "tacticalRadarMapArea"
        readonly property real availableWidth: Math.max(0, root.width - 2 * root.sideInset)
        readonly property real availableHeight: Math.max(0, root.height - root.headerHeight - root.footerHeight)
        width: root.currentSeason ? Math.min(availableWidth, availableHeight * root.fieldAspect)
                                  : availableWidth
        height: root.currentSeason ? width / root.fieldAspect : availableHeight
        x: (root.width - width) / 2
        y: root.headerHeight + (root.compactHeader ? 0 : (availableHeight - height) / 2)
        color: "#202c34"
        clip: true

        Image {
            id: mapBgImg
            anchors.fill: parent
            source: root.backgroundSource || root.model.mapImageSource || ""
            fillMode: Image.PreserveAspectFit
            smooth: true
            rotation: root.currentSeason && root.viewMirrored ? 180 : 0
        }

        HUD.ArenaStartZones {
            anchors.fill: parent
            visible: root.currentSeason && root.fieldWidth === 5
            mirrored: root.viewMirrored
        }

        Repeater {
            model: root.fortressLocations

            FortressCapture {
                required property var modelData
                readonly property real nx: modelData.x / 280
                x: (root.viewMirrored ? 1 - nx : nx) * mapArea.width - width / 2
                y: (mapArea.height - height) / 2
                width: 2.35 / 28 * mapArea.width
                height: 2.10 / 15 * mapArea.height
                visualScale: root.visualScale
                captureColor: modelData.red ? root.blueColor : root.redColor
                remainingSec: Number(root.objectiveData[modelData.fortress] || 0)
                temporarilyControlled: root.objectiveData[modelData.base] === 2
            }
        }

        Repeater {
            model: root.largeView ? root.terrainLabels : []

            Rectangle {
                id: terrainTag

                required property var modelData
                readonly property real nx: modelData.x / (root.fieldWidth * 10)
                readonly property real ny: modelData.y / (root.fieldHeight * 10)
                x: (root.viewMirrored ? 1 - nx : nx) * mapArea.width - width / 2
                y: (root.viewMirrored ? 1 - ny : ny) * mapArea.height - height / 2
                width: terrainText.implicitWidth + 4 * root.visualScale
                height: terrainText.implicitHeight + 2 * root.visualScale
                radius: 2 * root.visualScale
                color: "#401b272e"
                visible: mapBgImg.status === Image.Ready

                Text {
                    id: terrainText
                    anchors.centerIn: parent
                    text: terrainTag.modelData.label
                    color: "#c4dce4df"
                    style: Text.Outline
                    styleColor: "#8f141e24"
                    font.pixelSize: Math.max(9, 8 * root.visualScale)
                }
            }
        }

        // 官方地图未加载时绘制简化场地标识。
        Canvas {
            id: fallbackGrid
            anchors.fill: parent
            renderStrategy: root.threadedCanvasRendering
                ? Canvas.Threaded : Canvas.Immediate
            visible: mapBgImg.source === "" || mapBgImg.status !== Image.Ready
            onPaint: {
                var ctx = getContext("2d")
                var w = width; var h = height
                var s = root.visualScale
                ctx.clearRect(0, 0, w, h)
                // 网格
                ctx.strokeStyle = "#142436"; ctx.lineWidth = s
                for (var i = 0; i < 6; i++) {
                    ctx.beginPath(); ctx.moveTo(i*w/5, 0); ctx.lineTo(i*w/5, h); ctx.stroke()
                }
                for (var j = 0; j < 4; j++) {
                    ctx.beginPath(); ctx.moveTo(0, j*h/3); ctx.lineTo(w, j*h/3); ctx.stroke()
                }
                // 己方基地区
                ctx.fillStyle = root.allyIsBlue ? "rgba(68,136,255,0.12)" : "rgba(255,68,68,0.12)"
                ctx.fillRect(0, h*0.25, w*0.1, h*0.5)
                // 敌方基地区
                ctx.fillStyle = root.enemyIsBlue ? "rgba(68,136,255,0.12)" : "rgba(255,68,68,0.12)"
                ctx.fillRect(w*0.9, h*0.25, w*0.1, h*0.5)
                // 缓冲区
                ctx.strokeStyle = "rgba(0,255,136,0.12)"
                ctx.lineWidth = s
                ctx.setLineDash([2 * s, 6 * s])
                ctx.beginPath(); ctx.arc(w*0.45, h*0.35, w*0.05, 0, Math.PI*2); ctx.stroke()
                ctx.setLineDash([])
            }
        }

        // === 第 2、3 层：战术叠加画布 ===
        Canvas {
            id: mapCanvas
            anchors.fill: parent
            z: 1
            renderStrategy: root.threadedCanvasRendering
                ? Canvas.Threaded : Canvas.Immediate

            property var allyRobots: root.transformRobots(
                root.mergedRobotModel(root.model.allyRobots || [], root.allyStatusModel || [])
            )
            property var enemyRobots: root.transformRobots(
                root.mergedRobotModel(root.model.enemyRobots || [], root.enemyStatusModel || [])
            )
            property var routes: root.transformRoutes(root.model.routes || [])
            property var dangerZones: root.transformDangerZones(root.model.dangerZones || [])
            property var buffZones: root.currentSeason ? []
                : root.transformBuffZones(root.model.buffZones || [])

            onPaint: {
                var ctx = getContext("2d")
                var w = width; var h = height
                var s = root.visualScale
                ctx.clearRect(0, 0, w, h)

                // --- 危险区 (红色半透明圆) ---
                for (var d = 0; d < dangerZones.length; d++) {
                    var dz = dangerZones[d]
                    ctx.fillStyle = "rgba(255,50,50,0.15)"
                    ctx.strokeStyle = "rgba(255,50,50,0.4)"
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(dz.centerX * w, dz.centerY * h, (dz.radius || 0.05) * w, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.stroke()
                    // 危险标签
                    ctx.fillStyle = "rgba(255,80,80,0.6)"
                    ctx.font = (8 * s) + "px sans-serif"
                    ctx.fillText("⚠", dz.centerX * w - 6 * s, dz.centerY * h - (dz.radius || 0.05) * w - 2 * s)
                }

                // --- 推荐路线 (金色虚线) ---
                ctx.strokeStyle = "rgba(255,170,0,0.7)"
                ctx.lineWidth = 2 * s
                ctx.setLineDash([4 * s, 4 * s])
                for (var r = 0; r < routes.length; r++) {
                    var rt = routes[r]
                    ctx.beginPath()
                    ctx.moveTo(rt.fromX * w, rt.fromY * h)
                    ctx.lineTo(rt.toX * w, rt.toY * h)
                    ctx.stroke()
                    // 箭头
                    var midX = (rt.fromX + rt.toX) / 2 * w
                    var midY = (rt.fromY + rt.toY) / 2 * h
                    ctx.fillStyle = "rgba(255,170,0,0.6)"
                    ctx.font = (9 * s) + "px sans-serif"
                    ctx.fillText("→", midX - 4 * s, midY + 4 * s)
                }
                ctx.setLineDash([])

                // --- 增益点 (绿色) ---
                for (var b = 0; b < buffZones.length; b++) {
                    var bz = buffZones[b]
                    // 外圈
                    ctx.strokeStyle = "rgba(0,255,136,0.5)"
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(bz.x * w, bz.y * h, 7 * s, 0, Math.PI * 2)
                    ctx.stroke()
                    // 内点
                    ctx.fillStyle = "#00FF88"
                    ctx.beginPath()
                    ctx.arc(bz.x * w, bz.y * h, 4 * s, 0, Math.PI * 2)
                    ctx.fill()
                    // 标签
                    ctx.fillStyle = "rgba(0,255,136,0.7)"
                    ctx.font = "bold " + (7 * s) + "px sans-serif"
                    ctx.fillText("BUFF", bz.x * w - 12 * s, bz.y * h - 10 * s)
                }

                for (var e = 0; e < enemyRobots.length; e++)
                    drawRobot(ctx, enemyRobots[e], root.enemyColor, false)
                for (var a = 0; a < allyRobots.length; a++) {
                    var ally = allyRobots[a]
                    if (root.currentSeason) {
                        RobotSymbol.drawAlliedMarker(ctx, ally.x * w, root.normalizedRobotScreenY(ally) * h,
                                                s, root.allyColor, root.isSelfRobot(ally),
                                                ally.angle, root.robotNumberText(ally))
                    } else {
                        drawRobot(ctx, ally, root.allyColor, root.isSelfRobot(ally))
                    }
                }
            }

            function drawRobot(ctx, robot, teamColor, isSelf) {
                var s = root.visualScale
                var x = robot.x * width
                var y = root.normalizedRobotScreenY(robot) * height
                var accent = isSelf ? root.selfRobotColor : teamColor
                var radius = 10.5 * s

                if (robot.angle !== undefined) {
                    ctx.save()
                    ctx.translate(x, y)
                    ctx.rotate(robot.angle * Math.PI / 180)
                    ctx.fillStyle = accent
                    ctx.strokeStyle = "#111c22"
                    ctx.lineWidth = 1.25 * s
                    ctx.beginPath()
                    ctx.moveTo(0, -17 * s)
                    ctx.lineTo(4.2 * s, -10 * s)
                    ctx.lineTo(-4.2 * s, -10 * s)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.fill()
                    ctx.restore()
                }

                ctx.strokeStyle = "#111c22"
                ctx.lineWidth = 4 * s
                ctx.beginPath()
                ctx.arc(x, y, radius, 0, Math.PI * 2)
                ctx.stroke()

                ctx.fillStyle = isSelf ? root.selfRobotColor : "#17242b"
                ctx.fill()
                ctx.strokeStyle = accent
                ctx.lineWidth = 1.7 * s
                ctx.stroke()

                if (isSelf) {
                    ctx.strokeStyle = teamColor
                    ctx.lineWidth = s
                    ctx.beginPath()
                    ctx.arc(x, y, radius + 2.7 * s, 0, Math.PI * 2)
                    ctx.stroke()
                }

                ctx.fillStyle = isSelf ? "#192c2b" : "#f1f4ef"
                ctx.font = "bold " + (13 * s) + "px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText(root.robotNumberText(robot), x, y + 0.5 * s)
            }

            // 数据变化触发重绘
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onAllyRobotsChanged: requestPaint()
            onEnemyRobotsChanged: requestPaint()
            onRoutesChanged: requestPaint()
            onDangerZonesChanged: requestPaint()
            onBuffZonesChanged: requestPaint()
        }

        Repeater {
            model: mapCanvas.enemyRobots

            Item {
                id: enemyHealthBar
                required property var modelData
                property var robot: enemyHealthBar.modelData
                readonly property bool online: Boolean(enemyHealthBar.robot.online)
                readonly property int hp: Number(enemyHealthBar.robot.hp || 0)
                readonly property int maxHp: Math.max(1, Number(enemyHealthBar.robot.maxHp || 0))
                readonly property real hpRatio: Math.max(0, Math.min(1, enemyHealthBar.hp / enemyHealthBar.maxHp))
                readonly property bool criticalLowHealth: enemyHealthBar.online && enemyHealthBar.hp > 0 && enemyHealthBar.hpRatio < 0.2
                readonly property color teamColor: root.enemyColor
                readonly property color fillColor: enemyHealthBar.hpRatio > 0.55 ? enemyHealthBar.teamColor
                    : (enemyHealthBar.hpRatio > 0.25 ? "#ffb22e" : "#ff4e4e")

                readonly property real markerX: Number(enemyHealthBar.modelData.x || 0) * mapArea.width
                readonly property real markerY: root.normalizedRobotScreenY(enemyHealthBar.modelData) * mapArea.height

                visible: enemyHealthBar.modelData !== undefined
                    && enemyHealthBar.modelData.online !== undefined
                    && enemyHealthBar.markerX >= 0 && enemyHealthBar.markerX <= mapArea.width
                    && enemyHealthBar.markerY >= 0 && enemyHealthBar.markerY <= mapArea.height
                x: Math.round(Math.max(0, Math.min(mapArea.width - enemyHealthBar.width, enemyHealthBar.markerX - enemyHealthBar.width / 2)))
                y: Math.round(Math.max(0, Math.min(mapArea.height - enemyHealthBar.height, enemyHealthBar.markerY - 25 * root.visualScale)))
                z: 2
                width: 29 * root.visualScale
                height: 5 * root.visualScale

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    color: root.healthBarPanelColor
                    border.width: root.visualScale
                    border.color: root.healthBarFrameColor
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.visualScale
                    radius: 2 * root.visualScale
                    color: root.healthBarTrackColor

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(enemyHealthBar.hpRatio > 0 ? 2 * root.visualScale : 0, parent.width * enemyHealthBar.hpRatio)
                        radius: 2 * root.visualScale
                        color: enemyHealthBar.fillColor
                        visible: enemyHealthBar.hpRatio > 0
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    visible: enemyHealthBar.criticalLowHealth
                    color: "#d80d18"
                    opacity: enemyHealthBar.criticalLowHealth ? 0.16 : 0

                    SequentialAnimation on opacity {
                        running: enemyHealthBar.criticalLowHealth
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.10; to: 0.35; duration: 360; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.35; to: 0.10; duration: 520; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        Repeater {
            model: mapCanvas.allyRobots

            Item {
                id: allyHealthBar
                required property var modelData
                property var robot: allyHealthBar.modelData
                readonly property bool online: Boolean(allyHealthBar.robot.online)
                readonly property int hp: Number(allyHealthBar.robot.hp || 0)
                readonly property int maxHp: Math.max(1, Number(allyHealthBar.robot.maxHp || 0))
                readonly property real hpRatio: Math.max(0, Math.min(1, allyHealthBar.hp / allyHealthBar.maxHp))
                readonly property bool criticalLowHealth: allyHealthBar.online && allyHealthBar.hp > 0 && allyHealthBar.hpRatio < 0.2
                readonly property bool selfRobot: root.isSelfRobot(allyHealthBar.modelData)
                readonly property color teamColor: allyHealthBar.selfRobot ? root.selfRobotColor : root.allyColor
                readonly property color fillColor: allyHealthBar.hpRatio > 0.55 ? allyHealthBar.teamColor
                    : (allyHealthBar.hpRatio > 0.25 ? "#ffb22e" : "#ff4e4e")

                readonly property real markerX: Number(allyHealthBar.modelData.x || 0) * mapArea.width
                readonly property real markerY: root.normalizedRobotScreenY(allyHealthBar.modelData) * mapArea.height

                visible: allyHealthBar.modelData !== undefined
                    && allyHealthBar.modelData.online !== undefined
                    && allyHealthBar.markerX >= 0 && allyHealthBar.markerX <= mapArea.width
                    && allyHealthBar.markerY >= 0 && allyHealthBar.markerY <= mapArea.height
                x: Math.round(Math.max(0, Math.min(mapArea.width - allyHealthBar.width, allyHealthBar.markerX - allyHealthBar.width / 2)))
                y: Math.round(Math.max(0, Math.min(mapArea.height - allyHealthBar.height, allyHealthBar.markerY + 17 * root.visualScale)))
                z: 2
                width: 29 * root.visualScale
                height: 5 * root.visualScale

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    color: root.healthBarPanelColor
                    border.width: root.visualScale
                    border.color: root.healthBarFrameColor
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: root.visualScale
                    radius: 2 * root.visualScale
                    color: root.healthBarTrackColor

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(allyHealthBar.hpRatio > 0 ? 2 * root.visualScale : 0, parent.width * allyHealthBar.hpRatio)
                        radius: 2 * root.visualScale
                        color: allyHealthBar.fillColor
                        visible: allyHealthBar.hpRatio > 0
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 3 * root.visualScale
                    visible: allyHealthBar.criticalLowHealth
                    color: "#d80d18"
                    opacity: allyHealthBar.criticalLowHealth ? 0.16 : 0

                    SequentialAnimation on opacity {
                        running: allyHealthBar.criticalLowHealth
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.10; to: 0.35; duration: 360; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 0.35; to: 0.10; duration: 520; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        Repeater {
            model: root.objectiveLocations

            FieldObjective {
                id: objective

                required property var modelData
                readonly property real nx: modelData.x / (root.fieldWidth * 10)
                readonly property real ny: modelData.y / (root.fieldHeight * 10)
                objectName: modelData.key + "MapObjective"
                x: (root.viewMirrored ? 1 - nx : nx) * mapArea.width
                y: (root.viewMirrored ? 1 - ny : ny) * mapArea.height
                z: 0.5
                statusLayer: mapArea
                visualScale: root.visualScale
                compact: !root.largeView
                isBase: modelData.base
                readonly property bool ownerRed: modelData.base ? modelData.red
                    : (root.objectiveData[modelData.key + "OwnerRed"] ?? modelData.red)
                teamColor: ownerRed ? root.redColor : root.blueColor
                hp: root.objectiveData[modelData.key + "Hp"]
                maxHp: (modelData.base ? root.gameDataContext?.redBaseMaxHealth
                                       : root.gameDataContext?.redOutpostMaxHealth) ?? 0
                status: root.objectiveStatus(modelData)
            }
        }

        // 地图点击 — 命中检测机器人
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                if (!root.pageRoot) return
                var mx = mouse.x / width
                var my = mouse.y / height

                // 遍历敌方机器人（优先命中）
                var enemies = mapCanvas.enemyRobots
                for (var ei = 0; ei < enemies.length; ei++) {
                    var er = enemies[ei]
                    if (root.isRobotHit(er, mx, my)) { // 半径 ~0.05 归一化
                        root.pageRoot.selectRobot(er.id, true)
                        return
                    }
                }

                // 遍历我方机器人
                var allies = mapCanvas.allyRobots
                for (var allyIndex = 0; allyIndex < allies.length; allyIndex++) {
                    var ar = allies[allyIndex]
                    if (root.isRobotHit(ar, mx, my)) {
                        root.pageRoot.selectRobot(ar.id, false)
                        return
                    }
                }

                // 点击空白处取消选中
                root.pageRoot.clearSelection()
            }
        }
    }

    Item {
        visible: !root.compactHeader && (root.largeView || root.currentSeason)
        anchors.left: mapArea.left
        anchors.right: mapArea.right
        anchors.top: mapArea.bottom
        height: root.footerHeight

        Row {
            visible: root.largeView
            spacing: 10 * root.visualScale
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: [
                    { name: "本机", color: root.selfRobotColor },
                    { name: "红方", color: root.redColor },
                    { name: "蓝方", color: root.blueColor }
                ]

                Row {
                    id: legendItem
                    required property var modelData
                    spacing: 4 * root.visualScale

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 6 * root.visualScale
                        height: width
                        radius: width / 2
                        color: legendItem.modelData.color
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: legendItem.modelData.name
                        color: "#bfcac8"
                        font.pixelSize: 8.5 * root.visualScale
                    }
                }
            }
        }

        HUD.FieldBuffStatus {
            objectName: "tacticalFieldBuffStatus"
            anchors.centerIn: parent
            buffs: root.currentSeason
                ? (root.pageRoot && root.pageRoot.operatorBuffData !== undefined
                    ? root.pageRoot.operatorBuffData
                    : (root.gameDataContext?.buffTimedData || ({})))
                : ({})
            visualScale: root.visualScale
        }

        Row {
            visible: root.currentSeason && root.largeView
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5 * root.visualScale

            Rectangle {
                readonly property int meters: root.fieldWidth > 12 ? 5 : (root.fieldWidth === 12 ? 2 : 1)
                id: distanceScale
                anchors.verticalCenter: parent.verticalCenter
                width: mapArea.width * meters / root.fieldWidth
                height: 3 * root.visualScale
                color: "#b7c8c7"
                Rectangle {
                    anchors.right: parent.right
                    width: parent.width / 2
                    height: parent.height
                    color: "#576f77"
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "#b7c8c7"
                text: distanceScale.meters + " m"
                font.pixelSize: 8.5 * root.visualScale
            }
        }
    }
}
